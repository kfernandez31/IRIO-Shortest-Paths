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
    ClientContext context;
    RegionReply region;
    HelloRequest request;
    request.set_addr(own_address);
    Status status = stub_->hello_and_get_region(&context, request, &region);
    region_ = region.region_num();
    worker_state_->my_vertices_ = load_graph(region_);
    neighbors_jobs_ = std::map<region_id_t, std::vector<ContinueJob>>();

    std::cerr << "WORKER CLIENT HELLO AND GET REGION" << std::endl;
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
    bool end = false;
    while (!end)
    {
        std::unique_lock<std::mutex> lock(worker_state_->phase_mutex_); //@todo 
        while (worker_state_->notification_counter < 1) {
            worker_state_->phase_cond_.wait(lock);
        }
        worker_state_->notification_counter=0;
        switch (worker_state_->phase_)
        {
            case WorkerComputationPhase::AWAIT_MAIN: 
                break;
            case WorkerComputationPhase::HANDLE_JOBS:
                {
                    auto anything_to_send = this->dijkstra_within_region();
                    lock.unlock();
                    std::cerr << "END OF DIJKSTRA" << std::endl;
                    this->send_to_neighbors();
                    std::cerr << "SENT TO NEIGH" << std::endl;
                    this->send_end_of_phase_to_main(anything_to_send);
                    std::cerr << "SENT TO MAIN" << std::endl;
                    
                }
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


bool ShortestPathsWorkerClient::dijkstra_within_region()
{
    bool anything_to_send = false;
    while (!worker_state_->pq_.empty()) {
        vertex_path_info_t top = worker_state_->pq_.top();
        worker_state_->pq_.pop();
        dist_t dist = std::get<0>(top);
        vertex_id_t vertex_id = std::get<1>(top);
        vertex_id_t father_id = std::get<2>(top);
        region_id_t father_region = std::get<3>(top);
        std::cout << " GOT TOP OF QUEUE " << vertex_id << std::endl;
        std::shared_ptr<Vertex> vertex = (worker_state_->my_vertices_)[vertex_id];

        if (dist >= get_distance(vertex->id)) {
            continue; // top is outdated
        }

        (worker_state_->distances_)[vertex_id] = dist;
        (worker_state_->parents_)[vertex_id] = std::make_pair(father_id, father_region);   
        for (const Edge& edge : vertex->edges) {
            std::cout << " ITERATING OVER EDGES" << std::endl;
            dist_t new_dist = dist + edge.weight_;
            if (edge.reaches_optimally(worker_state_->destination_region_)) {
                if (region_ != edge.end_region_) {
                    ContinueJob cont_job;
                    std::cout << "New job for region" << edge.end_region_ << " from vertex " << vertex->id << " to" << edge.end_ << std::endl;
                    cont_job.set_distance_to_child(new_dist);
                    cont_job.set_parent(vertex->id);
                    cont_job.set_child(edge.end_);
                    cont_job.set_child_region(edge.end_region_);
                    neighbors_jobs_[edge.end_region_].push_back(std::move(cont_job));
                    anything_to_send = true;
                } else if (new_dist < get_distance(edge.end_)) {
                    worker_state_->pq_.emplace(new_dist, edge.end_, vertex->id, region_);
                }
            }
        }
    }

    return anything_to_send;
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
        std::cout << "SENDING JOB TO :" << region_info.first << std::endl;
        ContinueJobs continue_jobs;
        continue_jobs.set_parent_region(region_);
        auto region_id = region_info.first;
        auto region_jobs = region_info.second;
        for (const auto& job : region_jobs) {
            std::cout << "     Job " << job.child() << std::endl;
            ContinueJob* continue_job = continue_jobs.add_job();
            *continue_job = job;
        }
        Ok ok_reply;
        ClientContext context;

        (worker_state_->neighbors_)[region_id]->send_jobs_to_neighbors(&context, continue_jobs, &ok_reply); 
        region_jobs.clear();
    }
}
