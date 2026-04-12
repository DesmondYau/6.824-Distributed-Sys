#include "master_service.hpp"

MasterServiceImpl::MasterServiceImpl(Master& master)
        : m_master { master }
    {}

::grpc::Status MasterServiceImpl::GetTaskForWorker(::grpc::ServerContext* context, const ::mapreduce::Empty* request, ::mapreduce::Task* response)
{
    Task task { m_master.getTaskForWorker()};
    response->set_taskid(task.getTaskID());
    response->set_filename(task.getFileName());
    response->set_tasktype(static_cast<mapreduce::Task::TaskType>(task.getTaskType()));
    response->set_taskstate(static_cast<mapreduce::Task::TaskState>(task.getTaskState()));
    return ::grpc::Status::OK;
    
}

::grpc::Status MasterServiceImpl::GetReduceCount(::grpc::ServerContext* context, const ::mapreduce::Empty* request, ::mapreduce::CountReply* response)
{
    response->set_count(m_master.getReduceCount());
    return ::grpc::Status::OK;
}

::grpc::Status MasterServiceImpl::ReportMapComplete(::grpc::ServerContext* context, const ::mapreduce::TaskIdRequest* request, ::mapreduce::Empty* response)
{
    m_master.reportMapComplete(request->taskid());
    return ::grpc::Status::OK;
}

::grpc::Status MasterServiceImpl::GetMapCount(::grpc::ServerContext* context, const ::mapreduce::Empty* request, ::mapreduce::CountReply* response)
{
    response->set_count(m_master.getMapCount());
    return ::grpc::Status::OK;
}

::grpc::Status MasterServiceImpl::ReportReduceComplete(::grpc::ServerContext* context, const ::mapreduce::TaskIdRequest* request, ::mapreduce::Empty* response)
{
    m_master.reportReduceComplete(request->taskid());
    return ::grpc::Status::OK;
}