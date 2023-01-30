#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <tuple>
#include <chrono>

#include "worker_server.hh"

Status ShortestPathsWorkerServer::send_jobs_to_neighbors(ServerContext *context, const ContinueJobs *continue_jobs, Ok *ok_reply) {
    auto lock = std::unique_lock<std::mutex> (worker_state_->phase_mutex_);
    const auto parent_region = continue_jobs->parent_region();
    for (const auto& job : continue_jobs->job()) {
        auto const parent_vertex_id = job.parent();
        auto const child_vertex_id = job.child();
        auto const distance = job.distance_to_child();
        std::cout << "GOT NEW JOB" << parent_vertex_id << " " << child_vertex_id << " " << std::endl; 
        worker_state_->pq_.emplace(distance, child_vertex_id, parent_vertex_id, parent_region);
        std::cout <<"EMPLACEd JOB"<< parent_vertex_id << " " << child_vertex_id << " " << std::endl; 
    }
    received_jobs_from_neighbours_[parent_region] = true;
    received_jobs_from_neighbours_counter_++;

    if (received_jobs_from_neighbours_counter_ == neighbours_number_) {
        received_jobs_from_neighbours_.clear();
        received_jobs_from_neighbours_counter_ = 0;

        

        worker_state_->phase_ = WorkerComputationPhase::END_OF_EXCHANGE;
        worker_state_->notification_counter++;
        worker_state_->phase_cond_.notify_one();
        
    }
    return Status::OK;
}

Status ShortestPathsWorkerServer::begin_new_query(ServerContext *context, const NewJob *new_job, Ok *ok_reply) {
    auto lock = std::unique_lock<std::mutex> (worker_state_->phase_mutex_);
    worker_state_->destination_ = new_job->end_vertex();
    worker_state_->destination_region_ = new_job->end_vertex_region();
    std::cout << "new query region:" << worker_state_->destination_region_ << std::endl;
    auto is_first = new_job->is_first();
    auto start_vertex = new_job->start_vertex();

    for (auto const neighbour : new_job->neighbours()) {
        auto addr = neighbour.address();
        auto region = neighbour.region_number();
        auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
        auto stub = shortestpaths::ShortestPathsWorkerService::NewStub(channel);
        (worker_state_->neighbors_)[region] = std::move(stub);
        std::cout << "Added neighboring region id :" << region << " " << addr << std::endl; 
    }

    if (is_first)
    {
        std::chrono::milliseconds timespan(2000); // or whatever

        std::this_thread::sleep_for(timespan);
        std::cout << "I AM FIRST " + std::to_string(start_vertex) << std::endl;
        worker_state_->pq_.emplace(0, start_vertex, start_vertex, (worker_state_->my_vertices_)[start_vertex]->region_);
    }

    worker_state_->phase_ = WorkerComputationPhase::HANDLE_JOBS;
    worker_state_->notification_counter++;
    worker_state_->phase_cond_.notify_one();
    
    std::cout << "RECEIVED BEGIN NEW QUERY" << std::endl;
    return Status::OK;
}



Status ShortestPathsWorkerServer::begin_next_round(ServerContext *context, const Ok *next_round_signal, Ok *ok_reply) {
    auto lock = std::unique_lock<std::mutex> (worker_state_->phase_mutex_);
    worker_state_->phase_ = WorkerComputationPhase::HANDLE_JOBS;
    worker_state_->notification_counter++;
    worker_state_->phase_cond_.notify_one();
    
    std::cout << "RECEIVED BEGING NEXT ROUND" << std::endl;
    return Status::OK;
}


Status ShortestPathsWorkerServer::send_path(ServerContext *context, const RetVertex *ret_vertex, Path *path_reply) {
    std::cout << "RECEIVED SEND PATH FROM MAIN" << std::endl;
    auto lock = std::unique_lock<std::mutex> (worker_state_->phase_mutex_);
    auto act_vertex = ret_vertex->vertex_id();
    auto my_region = (worker_state_->my_vertices_)[act_vertex]->region_;
    std::cout<<"GOING BACK ACT VERTEX IS "<< act_vertex<<std::endl;
    while (true){
        
        auto pre_act_vertex = (worker_state_->parents_)[act_vertex]; // id, reg
        std::cout << "GOING BACK ACT VERTEX IS " << pre_act_vertex.first <<std::endl;
    
        auto distance = (worker_state_->distances_)[act_vertex];
        
        PathVert *path_vert = path_reply->add_path_vericies();

        path_vert->set_next_region_id(pre_act_vertex.second);
        path_vert->set_distance(distance);
        path_vert->set_vertex(pre_act_vertex.first);
        if (my_region != pre_act_vertex.second || act_vertex == pre_act_vertex.first) {
            break;
        }
        
        act_vertex = pre_act_vertex.first;
    }
   std::cout<<"ENDEND GOING BACK" <<std::endl;
   return Status::OK;
}

Status ShortestPathsWorkerServer::end_of_query(ServerContext *context, const Ok *end_of_query_signal, Ok *ok_reply) {
    auto lock = std::unique_lock<std::mutex> (worker_state_->phase_mutex_);
    worker_state_->phase_ = WorkerComputationPhase::STOP_WORK;
    worker_state_->notification_counter++;

    worker_state_->phase_cond_.notify_one();
    

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