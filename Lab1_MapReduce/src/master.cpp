#include <iostream>
#include <queue>
#include <chrono>
#include "master.hpp"

Master::Master(int mapCount, int reduceCount, const std::vector<std::string>& fileNames)
    : m_mapRemaining { mapCount }
    , m_reduceRemaining { reduceCount }
    , m_reduceCount { reduceCount }
{
    for (int i=0; i<mapCount; i++)
    {
        m_mapTasks.emplace_back(std::make_shared<Task>(i, fileNames[i], TaskType::MAPTASK));
    }

    for (int i=0; i<reduceCount; i++)
    {
        m_reduceTasks.emplace_back(std::make_shared<Task>(i, "", TaskType::REDUCETASK));
    }
}


Task Master::getTaskForWorker()
{
    Task::ptr taskPtr;

    if (m_mapRemaining > 0)
    {
        taskPtr = selectMapTask();
    }
    else if (m_reduceRemaining > 0)
    {
        taskPtr = selectReduceTask() ;
    }
    else
    {
        taskPtr = std::make_shared<Task>(-1, "", TaskType::COMPLETETASK);
    }
    return *taskPtr;
}

Task::ptr Master::selectMapTask()
{
    std::lock_guard<std::mutex> lockGuard(m_mapMutex);

    for (auto& i : m_mapTasks)
    {
        if (i->getTaskState() == TaskState::UNASSIGNED)
        {
            i->setTaskState(TaskState::ASSIGNED);
            i->setTaskDeadline(std::chrono::steady_clock::now() + std::chrono::seconds(TASK_TIME_OUT_SEC));
            return i;
        }
    }
    return std::make_shared<Task>(-1, "", TaskType::EMPTYTASK);
}

Task::ptr Master::selectReduceTask()
{
    std::lock_guard<std::mutex> lockGuard(m_reduceMutex);
    
    for (auto& i : m_reduceTasks)
    {
        if (i->getTaskState() == TaskState::UNASSIGNED)
        {
            i->setTaskState(TaskState::ASSIGNED);
            i->setTaskDeadline(std::chrono::steady_clock::now() + std::chrono::seconds(TASK_TIME_OUT_SEC));
            return i;
        }
    }
    return std::make_shared<Task>(-1, "", TaskType::EMPTYTASK);
}

void Master::reportMapComplete(int taskId)
{
    for (auto& i : m_reduceTasks)
    {
        if (i->getTaskId() == taskId)
            i->setTaskState(TaskState::COMPLETED);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Missing parameter! Input format is './master <number of reduce tasks> ../tests/pg*.txt'" << std::endl;
        return 1;
    }

    /*
        Parse arguments
    */ 
    int mapCount { argc - 2 };
    int reduceCount { std::stoi(argv[1])};

    std::vector<std::string> fileNames;
    for (int i=2; i<argc; i++) {
        fileNames.push_back(argv[i]);
    }


    /*
        Initialize master 
    */ 
    Master master { mapCount, reduceCount, fileNames };

    buttonrpc server;
    server.as_server(5555);
    server.bind("getTaskForWorker", &Master::getTaskForWorker, &master); 
    server.bind("getReduceCount", &Master::getReduceCount, &master); 
    server.bind("reportMapComplete", &Master::reportMapComplete, &master); 


    // Run RPC server
    std::cout << "Starting server on RPC server on port 5555" << std::endl;
    server.run();

    return 0;
}

// g++-13 -std=c++23 master.cpp -o master -lzmq 
// ./master <number of reduce tasks> ../tests/pg*.txt