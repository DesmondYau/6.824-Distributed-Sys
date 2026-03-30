#include <iostream>
#include <fstream>
#include <dlfcn.h>
#include "../include/buttonrpc-master/buttonrpc.hpp"
#include "../include/json.hpp"

enum class TaskState { 
    unassigned, 
    assigned, 
    completed 
};

enum class TaskType { 
    mapTask, 
    reduceTask, 
    emptyTask, 
    completeTask 
};

struct Task
{
    int taskID_;
    std::string fileName_;
    TaskType taskType_;
    TaskState taskState;
    std::chrono::steady_clock::time_point deadline;

    friend Serializer& operator >> (Serializer& in, Task& d) {
		in >> d.taskID_ >> d.fileName_ >> d.taskType_ >> d.taskState >> d.deadline;
		return in;
	}

	friend Serializer& operator << (Serializer& out, Task& d) {
		out << d.taskID_ << d.fileName_ << d.taskType_ << d.taskState << d.deadline;
		return out;
	}
};

struct KeyValuePair
{
    std::string key_;
    std::string value_;
};

using MapFunc = std::vector<KeyValuePair>(*)(const std::string& filename, const std::string& contents);
using ReduceFunc = std::string(*)(const std::string& key, const std::vector<std::string> values);

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
        std::cout << "Usage: ./worker ../testfiles/xxx.so";
        return 1;
    }

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

    buttonrpc client;
	client.as_client("127.0.0.1", 5555);


    Task task = client.call<Task>("requestTask").val();
    int reduceCount = client.call<int>("getReduceCount").val();
    if (task.taskType_ == TaskType::mapTask)
    {
        doMapTask(task.fileName_, mapFunc, reduceCount, task.taskID_);
        // reportMapComplete(task.taskID_);
    }
    else if (task.taskType_ == TaskType::reduceTask)
    {
        // doReduceTask(task.taskID_, reduceFunc);
        // reportReduceComplete(task.taskID_);
    }
    

    return 0;
	
}