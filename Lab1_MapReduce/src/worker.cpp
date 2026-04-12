#include <iostream>
#include <fstream>
#include <dlfcn.h>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <grpcpp/grpcpp.h>
#include "proto/master.grpc.pb.h"
#include "proto/master.pb.h"
#include "worker.hpp"


int ihash(const std::string& key) {
    const uint32_t fnv_prime = 16777619u;
    uint32_t hash = 2166136261u;  

    for (unsigned char c : key) {
        hash ^= c;
        hash *= fnv_prime;
    }

    return static_cast<int>(hash & 0x7fffffff);
}

int getReduceCount(std::unique_ptr<mapreduce::MasterService::Stub>& stub) {
    grpc::ClientContext ctx;
    mapreduce::Empty req;
    mapreduce::CountReply reply;
    grpc::Status status = stub->GetReduceCount(&ctx, req, &reply);
    return status.ok() ? reply.count() : -1;
}

int getMapCount(std::unique_ptr<mapreduce::MasterService::Stub>& stub) {
    grpc::ClientContext ctx;
    mapreduce::Empty req;
    mapreduce::CountReply reply;
    grpc::Status status = stub->GetMapCount(&ctx, req, &reply);
    return status.ok() ? reply.count() : -1;
}

void writeMapOutput(std::vector<KeyValuePair> intermediate, int reduceCount, int taskID)
{
    std::vector<std::ofstream> outFiles;
    outFiles.reserve(reduceCount);
    for (int i{0}; i<reduceCount; i++)
    {
        std::string fname { "mr-" + std::to_string(taskID) + "-" + std::to_string(i) };
        outFiles.emplace_back(fname);
        if (!outFiles.back())
        {
            std::cerr << "Cannot open " << fname << std::endl;
        }
    }

    for (const auto& kv : intermediate)
    {
        int i = ihash(kv.key_) % reduceCount;
        nlohmann::json j;
        j["key"] = kv.key_;
        j["value"] = kv.value_;
        outFiles[i] << j.dump() << "\n";
    }
}

void doMapTask(const std::string& filename, MapFunc mapFunc, int reduceCount, int taskID)
{
    std::ifstream inf { filename };
    if (!inf)
    {
        std::cerr << "Cannot open file " << filename << std::endl;
        return;
    }
    
    std::stringstream buffer {};
    buffer << inf.rdbuf();
    inf.close();

    std::vector<KeyValuePair> intermediate {};
    auto kva = mapFunc(filename, buffer.str());
    for (const auto& kv : kva)
        intermediate.push_back(kv);

    writeMapOutput(intermediate, reduceCount, taskID);
}

void doReduceTask(int taskID, ReduceFunc reduceFunc, int mapCount)
{
    std::vector<KeyValuePair> intermediate {};
    for (int i{0}; i<mapCount; i++)
    {
        std::ifstream inf { "mr-" + std::to_string(i) + "-" + std::to_string(taskID) };
        if (!inf)
        {
            std::cerr << "Cannot open file intermediate file for reduce task" << std::endl;
        }

        std::string line;
        while(std::getline(inf, line))
        {
            KeyValuePair kv;
            nlohmann::json j = nlohmann::json::parse(line);
            kv.key_ = j["key"].get<std::string>();
            kv.value_ = j["value"].get<std::string>();
            intermediate.emplace_back(kv);
        }
    }
 
    std::sort(
        intermediate.begin(), intermediate.end(),
        [](const KeyValuePair& a, const KeyValuePair& b) {
            return a.key_ < b.key_;
        }
    );

    // Open output file
    std::ofstream ofile("mr-out-" + std::to_string(taskID));

    // Run Reduce on each distinct key
    for (size_t i{0}; i < intermediate.size();) {
        size_t j = i + 1;
        while (j < intermediate.size() && intermediate[j].key_ == intermediate[i].key_) {
            j++;
        }
        std::vector<std::string> values;
        for (size_t k{i}; k < j; k++) {
            values.push_back(intermediate[k].value_);
        }

        std::string output = reduceFunc(intermediate[i].key_, values);
        ofile << intermediate[i].key_ << " " << output << "\n";
        i = j;
    }
}

void reportMapComplete(std::unique_ptr<mapreduce::MasterService::Stub>& stub, int taskId) {
    mapreduce::TaskIdRequest req;
    req.set_taskid(taskId);
    grpc::ClientContext ctx;
    mapreduce::Empty resp;
    grpc::Status status = stub->ReportMapComplete(&ctx, req, &resp);
    if (!status.ok()) {
        std::cerr << "ReportMapComplete RPC failed: " << status.error_message() << std::endl;
    }
}

void reportReduceComplete(std::unique_ptr<mapreduce::MasterService::Stub>& stub, int taskId) {
    mapreduce::TaskIdRequest req;
    req.set_taskid(taskId);
    grpc::ClientContext ctx;
    mapreduce::Empty resp;
    grpc::Status status = stub->ReportReduceComplete(&ctx, req, &resp);
    if (!status.ok()) {
        std::cerr << "ReportReduceComplete RPC failed: " << status.error_message() << std::endl;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: ./worker ../tests/xxx.so";
        return 1;
    }

    /*
        1. Load map and reduce function from shared object file
    */
    // Load shared object file
    void* handle = dlopen(argv[1], RTLD_LAZY);
    if (!handle)
    {
        std::cout << "Cannot load shared object file " << argv[1] << std::endl;
        return 1;
    }

    // Obtain pointer to Map function in shared object file
    MapFunc mapFunc = (MapFunc)dlsym(handle, "Map");
    if (!mapFunc)
    {
        std::cout << "Cannot find Map function in shared object file" << std::endl;
        return 1;
    }

    // Obtain pointer to Reduce function in shared object file
    ReduceFunc reduceFunc = (ReduceFunc)dlsym(handle, "Reduce");
    if (!reduceFunc)
    {
        std::cout << "Cannot find Reduce function in shared object file" << std::endl;
        return 1;
    }


    /*
        2. Initialize gRPC client
    */
    auto channel = grpc::CreateChannel("127.0.0.1:5555", grpc::InsecureChannelCredentials());
    std::unique_ptr<mapreduce::MasterService::Stub> stub = mapreduce::MasterService::NewStub(channel);
    std::cout << "Starting Worker process ID: " << getpid() << std::endl;

    
    /*
        3. Worker handle map task and reduce task
    */
    bool completed { false };

    while(!completed)
    {
        
        grpc::ClientContext context;
        mapreduce::Empty request;
        mapreduce::Task task;

        // Worker requesting task from master
        grpc::Status status = stub->GetTaskForWorker(&context, request, &task);
        if (!status.ok())
        {
            std::cerr << "RPC failed: " << status.error_message() << ". Worker exiting..." << std::endl;
            return 1;
        }

        // Get total number of map tasks and reduces tasks
        int reduceCount = getReduceCount(stub);
        int mapCount = getMapCount(stub);

        // Perform respective task according to task type
        if (task.tasktype() == mapreduce::Task::MAPTASK)
        {
            doMapTask(task.filename(), mapFunc, reduceCount, task.taskid());
            reportMapComplete(stub, task.taskid());
        }
        else if (task.tasktype() == mapreduce::Task::REDUCETASK)
        {
            doReduceTask(task.taskid(), reduceFunc, mapCount);
            reportReduceComplete(stub, task.taskid());
        }
        else if (task.tasktype() == mapreduce::Task::EMPTYTASK)
        {
            std::cout << "No current available tasks. Retry after 5 seconds" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        else if (task.tasktype() == mapreduce::Task::COMPLETETASK)
        {
            std::cout << "All map and reduce tasks completed. Worker exiting" << std::endl;
            completed = true;
            
        }
    }

    return 0;
	
}

// ./worker ../tests/xxx.so
