#include <iostream>
#include <fstream>
#include <dlfcn.h>
#include <thread>
#include <chrono>
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

void writeMapOutput(std::vector<KeyValuePair> intermediate, int reduceCount, int taskID)
{
    std::vector<std::ofstream> outFiles(reduceCount);
    for (int i=0; i<reduceCount; i++)
    {
        std::string fname { "mr-" + std::to_string(taskID) + "-" + std::to_string(i) };
        outFiles[i].open(fname);
        if (!outFiles[i])
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

    for (auto& f : outFiles)
    {
        f.close();
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



int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: ./worker ../tests/xxx.so";
        return 1;
    }

    /*
        Load map and reduce function from shared object file
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
        Initialize rpc client
    */
    buttonrpc client;
	client.as_client("127.0.0.1", 5555);

    
    /*

    */
    bool completed { false };

    while(!completed)
    {
        Task task { client.call<Task>("getTaskForWorker").val() };
        int reduceCount { client.call<int>("getReduceCount").val() };
        std::cout << task.getTaskId() << std::endl;
        
        if (task.getTaskType() == TaskType::MAPTASK)
        {
            doMapTask(task.getFileName(), mapFunc, reduceCount, task.getTaskId());
            client.call<void>("reportMapComplete", task.getTaskId());
        }
        else if (task.getTaskType() == TaskType::REDUCETASK)
        {
            // doReduceTask(task.taskID_, reduceFunc);
            // reportReduceComplete(task.taskID_);
        }
        else if (task.getTaskType() == TaskType::EMPTYTASK)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        else if (task.getTaskType() == TaskType::COMPLETETASK)
        {
            completed = true;
        }
    }
    

    return 0;
	
}

// g++-13 -std=c++23 worker.cpp -o worker -lzmq -ldl
// ./worker ../tests/xxx.so
