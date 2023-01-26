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
    std::shared_ptr<std::map<region_id_t, std::shared_ptr<ShortestPathsWorkerService::Stub>>> neighbors,
    std::shared_ptr<std::map<vertex_id_t, std::pair<vertex_id_t,region_id_t>>> parents,
    std::shared_ptr<std::map<vertex_id_t, dist_t>> distances,
    std::shared_ptr<std::map<vertex_id_t, std::shared_ptr<Vertex>>> my_vertices,
    const std::string &main_address,
    const std::string &own_address) : stub_(ShortestPathsMainService::NewStub(channel)),
                                      phase_mutex_(phase_mutex),
                                      pq_(pq),
                                      phase_(phase),
                                      neighbors_(neighbors),
                                      parents_(parents),
                                      distances_(distances),
                                      my_vertices_(my_vertices),
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
    *my_vertices_ = load_graph(region_);
    neighbors_jobs_ = std::map<region_id_t, std::vector<ContinueJob>>();
}

void ShortestPathsWorkerClient::post_request_cleanup()
{
    distances_->clear();
    parents_->clear();
    while(!pq_->empty()){pq_->pop();};
    neighbors_jobs_.clear();
}



dist_t ShortestPathsWorkerClient::get_distance(const vertex_id_t vertex)
{
    auto search = distances_->find(vertex);

    if (search == distances_->end())
    {
        (*distances_)[vertex] = INF;
    }
    else {
        std::cout <<"FOUND VERTEX DIST = " << search->second;
    }
    return (*distances_)[vertex];
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
    }
    this->post_request_cleanup();
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

        std::cout << "GOT AND POPPED TOP" << std::endl;

        dist_t dist = std::get<0>(top);
        vertex_id_t vertex_id = std::get<1>(top);
        vertex_id_t father_id = std::get<2>(top);
        region_id_t father_region = std::get<3>(top);


        std::cout << "SEARCHING FOR VERTEX: " << vertex_id << std::endl;
        std::shared_ptr<Vertex> vertex = (*my_vertices_)[vertex_id];

        std::cout << "RETRIEVED MY VERTEX" <<std::endl;

        std::cout << "RETRIEVED ID: " << vertex->id << std::endl;

        if (dist > get_distance(vertex->id)) {
            std::cout << "IN IF" << std::endl;
            continue; // top is outdated
        }

        std::cout << "AFTER IF" << std::endl;

        (*distances_)[vertex_id] = dist;
        (*parents_)[vertex_id] = std::make_pair(father_id, father_region);

        std::cout << "DIJSKATRA LOOP VERTEX:" << vertex->id << " " << vertex->region_ << std::endl;
        
        for (const Edge& edge : vertex->edges) {
            dist_t new_dist = dist + edge.weight_;
            std::cout << "INNER DIJSKTRA LOOP EDGE" << std::endl;
            if (edge.reaches_optimally(destination_region_)) {
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
                    (*distances_)[edge.end_] = new_dist;
                    (*parents_)[edge.end_] = std::make_pair(vertex->id, region_);
                    pq_->emplace(new_dist, edge.end_, vertex->id, region_);
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
    *phase_ = WorkerComputationPhase::EXCHANGE_PHASE;
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
        (*neighbors_)[region_id]->send_jobs_to_neighbors(&context, continue_jobs, &ok_reply); 
        std::cout<<" I have sent"<<std::endl;
        region_jobs.clear();
    }
}
