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
    const std::shared_ptr<std::mutex> phase_mutex,
    const std::shared_ptr<maxheap_t> pq,
    std::shared_ptr<WorkerComputationPhase> phase,
    const std::string &main_address,
    const std::string &own_address) : stub_(ShortestPathsMainService::NewStub(channel)),
                                      phase_mutex_(phase_mutex),
                                      pq_(pq),
                                      phase_(phase),
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
}

void ShortestPathsWorkerClient::post_request_cleanup()
{
    distances_.clear();
    parents_.clear();
}



dist_t ShortestPathsWorkerClient::get_distance(const vertex_id_t vertex)
{
    auto search = distances_.find(vertex);
    if (search == distances_.end())
    {
        distances_[vertex] = INF;
    }
    return distances_[vertex];
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
        std::unique_lock<std::mutex> lock(*phase_mutex_); //@todo 

        switch (*phase_)
        {
        case WorkerComputationPhase::WAITING_FOR_QUERY: 
            break;
        case WorkerComputationPhase::EXCHANGE_PHASE: 
            break;

        case WorkerComputationPhase::HANDLE_JOBS:
            this->dijkstra_within_region();
            break;
        case WorkerComputationPhase::END_OF_EXCHANGE:
            this->send_end_of_echange_to_main();
            break;
        case WorkerComputationPhase::WAIT_FOR_PATH_RETRIEVAL:
            break;
        case WorkerComputationPhase::RETRIEVE_PATH:
            this->retrieve_path();
            break;
        case WorkerComputationPhase::STOP_WORK:
            end = true;
            break;
        }

      //  std::cout << "EXIT COMPUTE PHASE" << std::endl;
        // may need to free lock if RAII does not
    }
}

// FIXME
void ShortestPathsWorkerClient::send_partial_path_results(const region_id_t region, const path_t &path, const dist_t distance)
{
    // ClientContext context;
    // Ok ok;

    // std::unique_ptr<ClientWriter<PathVert>> writer;
    // if (region == MAIN_REGION)
    // {
    //     writer = std::make_unique<ClientWriter<PathVert>>(stub_->path_found(&context, &ok));
    // }
    // else
    // {
    //     writer = std::make_unique<ClientWriter<PathVert>>(neighbors_[region]->send_path(&context, &ok));
    // }

    // for (int i = 0; i < path.size(); ++i)
    // {
    //     PathVert msg;
    //     msg.set_is_last_vertex(i == path.size());
    //     msg.set_distance(path[i].first);
    //     msg.set_vertex(path[i].second->id);
    //     writer->Write(msg);
    // }
}

void ShortestPathsWorkerClient::retrieve_path()
{
    // std::vector<std::shared_ptr<Vertex>> path{last_vertex};
    // auto last_vertex_res = get_distance(vertex);
    // while (true){
    //     auto father = shortestpaths_res.find(last_vertex);
    //     //may error check find end, nullptr
    //     if (father->second.second == nullptr){
    //         send_path_back_to_main(path);
    //     }
    //     //tochange_names
    //     path.push_back(father->second.second);
    //     if (father->second.second->region != region_){
    //         send_path(father->second.second->region, path,distance);
    //     } else {
    //         last_vertex = father->second.second;
    //     }
    // }
}

void ShortestPathsWorkerClient::dijkstra_within_region()
{
    bool anything_to_send = false;
    while (!pq_->empty()) {
        vertex_path_info_t top = pq_->top();
        pq_->pop();

        dist_t dist = std::get<0>(top);
        vertex_id_t vertex_id = std::get<1>(top);
        vertex_id_t father_id = std::get<2>(top);

        std::shared_ptr<Vertex> vertex = my_vertices_[vertex_id];

        if (dist >= get_distance(vertex->id)) {
            continue; // top is outdated
        }

        distances_[vertex_id] = dist;
        parents_[vertex_id] = father_id;
        
        for (const Edge& edge : vertex->edges) {
            dist_t new_dist = dist + edge.weight_;
            if (edge.reaches_optimally(destination_region_)) {
                if (region_ != edge.end_->region_) {
                    ContinueJob cont_job;
                    cont_job.set_parent(vertex->id);
                    cont_job.set_child(edge.end_->id);
                    cont_job.set_child(edge.end_->region_);
                    neighbors_jobs_[edge.end_->region_].push_back(std::move(cont_job));
                    anything_to_send = true;
                } else if (new_dist < get_distance(edge.end_->id)) {
                    distances_[edge.end_->id] = new_dist;
                    parents_[edge.end_->id] = vertex->id;
                    pq_->emplace(new_dist, edge.end_->id, vertex->id);
                }
            }
        }
    }
    std::cout<<"RAN A DIJKSTRA PHASE"<<std::endl;
    this->send_end_of_phase_to_main(anything_to_send);
    this->send_to_neighbors();
    *this->phase_ = WorkerComputationPhase::EXCHANGE_PHASE;
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
        ContinueJobs continue_jobs;
        continue_jobs.set_parent_region(region_);
        auto region_id = region_info.first;
        auto region_jobs = region_info.second;
        for (const auto& job : region_jobs) {
            ContinueJob* continue_job = continue_jobs.add_job();
            *continue_job = job;
        }
        Ok ok_reply;
        ClientContext context;
        neighbors_[region_id]->send_jobs_to_neighbors(&context, continue_jobs, &ok_reply); 
        region_jobs.clear();
    }
}
