#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <mutex>
#include "../include/buttonrpc-master/buttonrpc.hpp"

const int TASK_TIME_OUT_SEC { 10 };

enum class TaskState { 
    UNASSIGNED, 
    ASSIGNED, 
    COMPLETED 
};

enum class TaskType { 
    MAPTASK, 
    REDUCETASK, 
    EMPTYTASK, 
    COMPLETETASK
};

class Task
{
public:
    using ptr = std::shared_ptr<Task>;

    /**
     * @brief Default constructor for RPC
     */
    Task()
        : m_taskId { -1 }
        , m_fileName { "" }
        , m_taskType { TaskType::EMPTYTASK }
    {}

    /**
     * @brief Constructor
     */
    Task(int taskId, std::string fileName, TaskType TaskType)
        : m_taskId { taskId }
        , m_fileName { fileName }
        , m_taskType { TaskType }
    {}

    /**
     * @brief Implementation for buttonrpc
     */
    friend Serializer& operator>> (Serializer& in, Task& d) {
		in >> d.m_taskId >> d.m_fileName >> d.m_taskType >> d.m_taskState >> d.m_deadline;
		return in;
	}

    /**
     * @brief Implementation for buttonrpc
     */
	friend Serializer& operator<< (Serializer& out, Task& d) {
		out << d.m_taskId << d.m_fileName << d.m_taskType << d.m_taskState << d.m_deadline;
		return out;
	}

    /**
     * @brief Return task id
     */
    int getTaskId() const { return m_taskId; }

    /**
     * @brief Return filename of task
     */
    std::string getFileName() const { return m_fileName; }

    /**
     * @brief Return task type
     */
    TaskType getTaskType() const { return m_taskType; }

    /**
     * @brief Return task state
     */
    TaskState getTaskState() const { return m_taskState; }

    /**
     * @brief Set task state
     */
    void setTaskState(TaskState taskState) { m_taskState = taskState; }

    /**
     * @brief Return task deadline
     */
    std::chrono::steady_clock::time_point getDeadline() const { return m_deadline; }

    /**
     * @brief Set task deadline
     */
    void setTaskDeadline(std::chrono::steady_clock::time_point deadline) { m_deadline = deadline; }


private:
    int m_taskId {};
    std::string m_fileName {};
    TaskType m_taskType {};
    TaskState m_taskState { TaskState::UNASSIGNED };
    std::chrono::steady_clock::time_point m_deadline { std::chrono::steady_clock::time_point::min() };
};


class Master
{
public:
    /**
     * @brief Constructor
     */
    Master(int mapCount, int reduceCount, const std::vector<std::string>& fileNames);

    /**
     * @brief Return total number of reduce task 
     * 
     * const is not used since buttonrpc does not support template overlaods for const member function pointer
     */
    int getReduceCount() { return m_reduceCount; }

    /**
     * @brief Handles worker request for tasks
     * 
     * When there are map tasks remaining, look for the first map task with state UNASSIGNED. 
     * Update task state to ASSIGNED and task deadline to ten seconds afterwards. Return task to worker.
     * 
     * If all map tasks are completed, we do the same for reduce tasks.
     */
    Task getTaskForWorker();

    /**
     * @brief Return unassigned map task from m_mapTasks. Otherwise, return task with type EMPTYTASK
     */
    Task::ptr selectMapTask();

    /**
     * @brief Return unassigned reduce task from m_reduceTasks. Otherwise, return task with type EMPTYTASK
     */
    Task::ptr selectReduceTask();

  
private:
    std::vector<Task::ptr> m_mapTasks {};  
    std::vector<Task::ptr> m_reduceTasks {};              
    int m_mapRemaining {};
    int m_reduceRemaining {};
    int m_reduceCount {};
    std::mutex m_mapMutex {};
    std::mutex m_reduceMutex {};

};