#include <iostream>
#include <queue>
#include <chrono>
#include "../include/buttonrpc-master/buttonrpc.hpp"

const int timeoutSec { 10 };

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

    // Implementation for buttonrpc
    friend Serializer& operator >> (Serializer& in, Task& d) {
		in >> d.taskID_ >> d.fileName_ >> d.taskType_ >> d.taskState >> d.deadline;
		return in;
	}

    // Implementation for buttonrpc
	friend Serializer& operator << (Serializer& out, Task& d) {
		out << d.taskID_ << d.fileName_ << d.taskType_ << d.taskState << d.deadline;
		return out;
	}
};
using TaskPtr = std::unique_ptr<Task>;


class Master
{
public:
    Master(int mapCount, int reduceCount, const std::vector<std::string>& fileNames);
    int getReduceCount();
    Task requestTask();
  
private:
    std::queue<TaskPtr> unassignedMapTasks_;   
    std::queue<TaskPtr> unassignedReduceTasks_;                  
    std::queue<TaskPtr> assignedTasks_;                  
    int mapRemaining_;
    int reduceRemaining_;
    int reduceCount_;
};

Master::Master(int mapCount, int reduceCount, const std::vector<std::string>& fileNames)
    : mapRemaining_ { mapCount }
    , reduceRemaining_ { reduceCount }
    , reduceCount_ { reduceCount }
{
    for (int i=0; i<mapCount; i++)
    {
        TaskPtr taskPtr { std::make_unique<Task>(
            i,
            fileNames[i],
            TaskType::mapTask,
            TaskState::unassigned,
            std::chrono::steady_clock::time_point {}
        )};
        
        unassignedMapTasks_.push(std::move(taskPtr));
    }

    for (int i=0; i<reduceCount; i++)
    {
        TaskPtr taskPtr { std::make_unique<Task>(
            i,
            "",
            TaskType::mapTask,
            TaskState::unassigned,
            std::chrono::steady_clock::time_point {}
        )};
        
        unassignedReduceTasks_.push(std::move(taskPtr));
    }
}

int Master::getReduceCount()
{
    return reduceCount_;
}

Task Master::requestTask()
{
    if (mapRemaining_)
    {
        if (!unassignedMapTasks_.empty())
        {
            TaskPtr taskPtr = std::move(unassignedMapTasks_.front());
            taskPtr->deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec);

            Task task { *taskPtr };
            assignedTasks_.push(std::move(taskPtr));

            return task;
        }
        else
            return Task{ -1, "", TaskType::emptyTask, TaskState::completed, std::chrono::steady_clock::time_point{} };
    }
    
    if (reduceRemaining_)
    {
        if (!unassignedReduceTasks_.empty())
        {
            TaskPtr taskPtr = std::move(unassignedReduceTasks_.front());
            taskPtr->deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec);
            
            Task task { *taskPtr };
            assignedTasks_.push(std::move(taskPtr));

            return task;
        }
        else
            return Task{ -1, "", TaskType::emptyTask, TaskState::completed, std::chrono::steady_clock::time_point{} };
    }

    return Task{ -1, "", TaskType::completeTask, TaskState::completed, std::chrono::steady_clock::time_point{} };
}


int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Missing parameter! Input format is './master <number of reduce tasks> ../testfiles/pg*.txt'" << std::endl;
        return 1;
    }

    // Parse arguments
    int mapCount { argc - 2 };
    int reduceCount { std::stoi(argv[1])};
    std::vector<std::string> fileNames;
    for (int i=2; i<argc; i++) {
        fileNames.push_back(argv[i]);
    }

    // Initialize master and RPC server
    buttonrpc server;
    server.as_server(5555);
    Master master { mapCount, reduceCount, fileNames };   
    server.bind("requestTask", &Master::requestTask, &master); 
    server.bind("getReduceCount", &Master::getReduceCount, &master); 

    // Run RPC server
    std::cout << "Starting server on RPC server on port 5555" << std::endl;
    server.run();

    return 0;
}