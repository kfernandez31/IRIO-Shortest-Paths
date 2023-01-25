#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <tuple>
#include <bitset>
#include <optional>

#include "worker_server.hh"

Status ShortestPathsWorkerServer::send_jobs_to_neighbors(ServerContext *context, const ContinueJobs *continue_jobs, Ok *ok_reply) {
    const auto parent_region = continue_jobs->parent_region();
    for (const auto& job : continue_jobs->job()) {
        auto const parent_vertex_id = job.parent();
        auto const child_vertex_id = job.child();
        auto const distance = job.distance_to_child();
        pq_->emplace(distance, child_vertex_id, parent_vertex_id);
    }
    received_jobs_from_neighbours_[parent_region] = true;
    received_jobs_from_neighbours_counter_++;

    if (received_jobs_from_neighbours_counter_ == neighbours_number_) {
        received_jobs_from_neighbours_.clear();
        received_jobs_from_neighbours_counter_ = 0;

        auto lock = std::unique_lock<std::mutex> (*phase_mutex_);

        *phase_ = WorkerComputationPhase::END_OF_EXCHANGE;
    }
    return Status::OK;
}

Status ShortestPathsWorkerServer::begin_new_query(ServerContext *context, const NewJob *new_job, Ok *ok_reply) {
        
    auto destination_ = new_job->end_vertex();
    auto destination_region_ = new_job->end_vertex_region();
    auto is_first = new_job->is_first();
    auto start_vertex = new_job->start_vertex();

    if (is_first)
    {
        pq_->emplace(0, start_vertex, start_vertex);
    }

    *phase_ = WorkerComputationPhase::HANDLE_JOBS;
    std::cout << "RECEIVED BEGIN NEW QUERY" << std::endl;
    return Status::OK;
}



Status ShortestPathsWorkerServer::begin_next_round(ServerContext *context, const Ok *next_round_signal, Ok *ok_reply) {
    std::unique_lock<std::mutex> lock(*phase_mutex_);
    *phase_ = WorkerComputationPhase::HANDLE_JOBS;
    std::cout << "RECEIVED BEGING NEXT ROUND" << std::endl;
    return Status::OK;
}


Status ShortestPathsWorkerServer::send_path(ServerContext *context, const ServerReader<PathVert> *path_stream, Ok *ok_reply) {
    return Status::OK;
}

void ShortestPathsWorkerServer::run() {
    std::string server_address(this->addr_);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials()); //TODO: do we want authentication?
    builder.RegisterService(this);
    auto server = builder.BuildAndStart();

    std::cout << "Server listening on " << server_address << std::endl;

    server->Wait();
}