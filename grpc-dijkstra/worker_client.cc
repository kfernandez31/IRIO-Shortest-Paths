#include "worker_client.hh"

// TODO: think about the necessity of these
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

#include "common.hh"



ShortestPathsWorkerClient::ShortestPathsWorkerClient(
    const std::shared_ptr<Channel> channel,
    std::shared_ptr<WorkerState> worker_state,
    const std::string &main_address,
    const std::string &own_address) : stub_(ShortestPathsMainService::NewStub(channel)),
                                      worker_state_(worker_state),
                                      address_(own_address),
                                      main_address_(main_address)
{
    // TODO: load the map
    // TODO: set distance (of which vertices?) to infinity, parents

    ClientContext context;
    RegionReply region;
    HelloRequest request;
    request.set_addr(own_address);
    Status status = stub_->hello_and_get_region(&context, request, &region);
    std::cout << "WORKER CLIENT HELLO AND GET REGION" << std::endl;
    region_ = region.region_num();
    worker_state_->my_vertices_ = load_graph(region_);
    neighbors_jobs_ = std::map<region_id_t, std::vector<ContinueJob>>();
}

void ShortestPathsWorkerClient::post_request_cleanup()
{
    worker_state_->distances_.clear();
    worker_state_->parents_.clear();
    while(!worker_state_->pq_.empty()){worker_state_->pq_.pop();};
    neighbors_jobs_.clear();
    worker_state_->notification_counter = 0;
}



dist_t ShortestPathsWorkerClient::get_distance(const vertex_id_t vertex)
{
    auto search = worker_state_->distances_.find(vertex);

    if (search == worker_state_->distances_.end())
    {
        (worker_state_->distances_)[vertex] = INF;
    }
    else {
        std::cout <<"FOUND VERTEX DIST = " << search->second;
    }
    return (worker_state_->distances_)[vertex];
}

void ShortestPathsWorkerClient::run()
{
    while (true)
    {
        this->compute_phase();
    }
}

void ShortestPathsWorkerClient::compute_phase()
{
    std::cout << "ENTERING COMPUTE PHASE" << std::endl;
    bool end = false;
    while (!end)
    {
        std::unique_lock<std::mutex> lock(worker_state_->phase_mutex_); //@todo 
        std::cout<< "Im in hell"<<std::endl;
        while (worker_state_->notification_counter < 1) {
            worker_state_->phase_cond_.wait(lock);
        }
        worker_state_->notification_counter=0;
        
        std::cout << "I AM AWAKE" << std::endl;
        switch (worker_state_->phase_)
        {
            case WorkerComputationPhase::AWAIT_MAIN: 
                break;
            case WorkerComputationPhase::HANDLE_JOBS:
                this->dijkstra_within_region();
                break;
            case WorkerComputationPhase::END_OF_EXCHANGE:
                this->send_end_of_echange_to_main();
                break;
            case WorkerComputationPhase::STOP_WORK:
                end = true;
                break;

        }
    }
    this->post_request_cleanup();
}


void ShortestPathsWorkerClient::dijkstra_within_region()
{
    std::cout << "ENTERING DIJKSTRA" << std::endl;
    bool anything_to_send = false;
    while (!worker_state_->pq_.empty()) {
        vertex_path_info_t top = worker_state_->pq_.top();
        worker_state_->pq_.pop();

        std::cout << "GOT AND POPPED TOP" << std::endl;

        dist_t dist = std::get<0>(top);
        vertex_id_t vertex_id = std::get<1>(top);
        vertex_id_t father_id = std::get<2>(top);
        region_id_t father_region = std::get<3>(top);


        std::cout << "SEARCHING FOR VERTEX: " << vertex_id << std::endl;
        std::shared_ptr<Vertex> vertex = (worker_state_->my_vertices_)[vertex_id];

        std::cout << "RETRIEVED MY VERTEX" <<std::endl;

        std::cout << "RETRIEVED ID: " << vertex->id << std::endl;

        if (dist > get_distance(vertex->id)) {
            std::cout << "IN IF" << std::endl;
            continue; // top is outdated
        }

        std::cout << "AFTER IF" << std::endl;

        (worker_state_->distances_)[vertex_id] = dist;
        (worker_state_->parents_)[vertex_id] = std::make_pair(father_id, father_region);

        std::cout << "DIJSKATRA LOOP VERTEX:" << vertex->id << " " << vertex->region_ << std::endl;
        
        for (const Edge& edge : vertex->edges) {
            dist_t new_dist = dist + edge.weight_;
            std::cout << "INNER DIJSKTRA LOOP EDGE  " << std::endl;
            if (edge.reaches_optimally(worker_state_->destination_region_)) {
                std::cout << "REACHES OPTIMALLIY" << std::endl;
                if (region_ != edge.end_region_) {
                    std::cout << "bad " << region_ << " "<< edge.end_region_<<  std::endl;
                    ContinueJob cont_job;
                    cont_job.set_distance_to_child(new_dist);
                    cont_job.set_parent(vertex->id);
                    cont_job.set_child(edge.end_);
                    cont_job.set_child_region(edge.end_region_);
                    neighbors_jobs_[edge.end_region_].push_back(std::move(cont_job));
                    anything_to_send = true;
                } else if (new_dist < get_distance(edge.end_)) {
                    std::cout<<"GOT DISTANCE TO VERTEX:"<< edge.end_ << std::endl;
                    (worker_state_->distances_)[edge.end_] = new_dist;
                    (worker_state_->parents_)[edge.end_] = std::make_pair(vertex->id, region_);
                    worker_state_->pq_.emplace(new_dist, edge.end_, vertex->id, region_);
                    std::cout << "EMPLACED" << std::endl;
                }
                std::cout << "INNER IF ENDED" << std::endl;
            }
        }
    }
    std::cout<<"RAN A DIJKSTRA PHASE"<<std::endl;
    this->send_end_of_phase_to_main(anything_to_send);
    std::cout<< "SEND END OF PHASE TO MAIN" << std::endl;
    this->send_to_neighbors();
    std::cout<< "SEND JOBS TO NEIGHBOURS" << std::endl;
    worker_state_->phase_ = WorkerComputationPhase::AWAIT_MAIN;
    std::cout << "CHANGE PHASE TO EXCHANGE PHASE" << std::endl;
}

void ShortestPathsWorkerClient::send_end_of_echange_to_main() {
    ClientContext context;
    ExchangePhaseEnd exchange_phase_end;
    Ok ok_reply;
    Status status = stub_->end_of_exchange_phase(&context, exchange_phase_end, &ok_reply);
}

void ShortestPathsWorkerClient::send_end_of_phase_to_main(bool anything_to_send) {
    ClientContext context;
    LocalPhaseEnd local_phase_end;
    local_phase_end.set_anything_to_send(anything_to_send);
    Ok ok_reply;
    Status status = stub_->end_of_local_phase(&context, local_phase_end, &ok_reply);
}


// TODO: send either a stream of `these` jobs or a `repeated`
void ShortestPathsWorkerClient::send_to_neighbors()
{
    for (const auto region_info : neighbors_jobs_) {
        std::cout << "NEIGHBOUR SENDING LOOP: " << region_info.first << std::endl;
        ContinueJobs continue_jobs;
        continue_jobs.set_parent_region(region_);
        auto region_id = region_info.first;
        auto region_jobs = region_info.second;
        std::cout <<"Pre pushing jobs"<<std::endl;
        for (const auto& job : region_jobs) {
            ContinueJob* continue_job = continue_jobs.add_job();
            *continue_job = job;
        }
        std::cout <<"Created grpc"<<std::endl;
        Ok ok_reply;
        ClientContext context;
        std::cout<< "Now I'm sending "<<std::endl;
        (worker_state_->neighbors_)[region_id]->send_jobs_to_neighbors(&context, continue_jobs, &ok_reply); 
        std::cout<<" I have sent"<<std::endl;
        region_jobs.clear();
    }
}
