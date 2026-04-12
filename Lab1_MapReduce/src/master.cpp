#include <iostream>
#include <chrono>
#include <thread>
#include "master.hpp"

Master::Master(int mapCount, int reduceCount, const std::vector<std::string>& fileNames)
    : m_mapRemaining { mapCount }
    , m_reduceRemaining { reduceCount }
    , m_mapCount { mapCount }
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
    std::lock_guard<std::mutex> lockGuard(m_Mutex);
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
    std::lock_guard<std::mutex> lockGuard(m_Mutex);

    for (auto& i : m_mapTasks)
    {
        if (i->getTaskID() == taskId)
        {
            i->setTaskState(TaskState::COMPLETED);
            m_mapRemaining--;
            std::cout << "Map task " << taskId << " completed." << std::endl;
            std::cout << "Map tasks remained: " << this->getMapRemaining() << std::endl;
        }
    }
}

void Master::reportReduceComplete(int taskId)
{
    std::lock_guard<std::mutex> lockGuard(m_Mutex);

    for (auto& i : m_reduceTasks)
    {
        if (i->getTaskID() == taskId)
        {
            i->setTaskState(TaskState::COMPLETED);
            m_reduceRemaining--;
            std::cout << "Reduce task " << taskId << " completed." << std::endl;
            std::cout << "Reduce tasks remained: " << this->getReduceRemaining() << std::endl;
        }
    }
}

void Master::refreshMapTaskState() 
{
    std::lock_guard<std::mutex> lockGuard(m_Mutex);

    for (auto& taskPtr : m_mapTasks)
    {
        if (taskPtr->getTaskState() == TaskState::ASSIGNED && taskPtr->getDeadline() < std::chrono::steady_clock::now())
        {
            std::cout << "Deadline for map task " << taskPtr->getTaskID() << " exceeded. Reset to UNASSIGNED" << std::endl;
            taskPtr->setTaskState(TaskState::UNASSIGNED);
        } 
    }
}


void Master::refreshReduceTaskState()
{
    std::lock_guard<std::mutex> lockGuard(m_Mutex);

    for (auto& taskPtr : m_reduceTasks)
    {
        if (taskPtr->getTaskState() == TaskState::ASSIGNED && taskPtr->getDeadline() < std::chrono::steady_clock::now())
        {
            std::cout << "Deadline for map task " << taskPtr->getTaskID() << " exceeded. Reset to UNASSIGNED" << std::endl;
            taskPtr->setTaskState(TaskState::UNASSIGNED);
        } 
    }
        
} 
