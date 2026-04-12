#pragma once
#include <grpcpp/grpcpp.h>
#include "proto/master.grpc.pb.h"
#include "proto/master.pb.h"
#include "master.hpp" 

class MasterServiceImpl final : public mapreduce::MasterService::Service
{
public:
    MasterServiceImpl(Master& master);
    ::grpc::Status GetTaskForWorker(::grpc::ServerContext* context, const ::mapreduce::Empty* request, ::mapreduce::Task* response) override;
    ::grpc::Status GetReduceCount(::grpc::ServerContext* context, const ::mapreduce::Empty* request, ::mapreduce::CountReply* response) override;
    ::grpc::Status ReportMapComplete(::grpc::ServerContext* context, const ::mapreduce::TaskIdRequest* request, ::mapreduce::Empty* response) override;
    ::grpc::Status GetMapCount(::grpc::ServerContext* context, const ::mapreduce::Empty* request, ::mapreduce::CountReply* response) override;
    ::grpc::Status ReportReduceComplete(::grpc::ServerContext* context, const ::mapreduce::TaskIdRequest* request, ::mapreduce::Empty* response) override;


private:
    Master& m_master;
};