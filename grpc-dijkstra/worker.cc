#include "worker.hh"

// TODO: think about the necessity of these
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <bitset>
#include <optional>

const dist_t INFINITY = std::numeric_limits<dist_t>::max();

Edge::Edge(const dist_t weight, const std::shared_ptr<Vertex>& end)
    : weight_(weight_), end_(end_) {}

bool Edge::reaches_optimally(const region_id_t region) const {
    return region_mask.test(region);
}

ShortestPathsWorker::ShortestPathsWorker(
    const std::shared_ptr<Channel>& channel,
    const std::shared_ptr<std::mutex>& pq_mutex,
    const std::shared_ptr<maxheap_t>& pq,
    const std::string& main_address,
    const std::string& own_address,
): 
    stub_(ShortestPathsMainWorker::NewStub(channel)), 
    pq_mutex_(pq_mutex), 
    pq_(pq), 
    own_address_(own_address), 
    main_address_(main_address)
{
    //TODO: load the map
    //TODO: set distance (of which vertices?) to infinity, parents 

    ClientContext context;
    RegionReply region;
    Status status = stub_->hello_and_get_region(&context, own_address, &region);
    region_ = region.region_num();
}

void ShortestPathsWorker::post_request_cleanup() {
    distances_.clear();
    parents_.clear();
}

// TODO: send either a stream of `these` jobs or a `repeated`
void ShortestPathsWorker::send_jobs_to_neighbors() {
    // ContinueJobs;
    // for (const auto& [neighbor_region, payloads] : neighbor_jobs_) {
    //     for (const auto& payload : payloads) {
    //         ContinueJob job;
    //         job.set_parent(payload.parent_);
    //         job.set_parent_region(region_);
    //         job.set_child(payload.child_);
    //         job.set_child_region(neighbor_region);
    //         job.set_distance_from_start(payload.distance_from_start_);
    //         job.set_edge_weight(payload.edge_weight_)
    //     }
    // }
}


dist_t ShortestPathsWorker::get_distance(const vertex_id vertex) {
    if (auto search = distances.find(vertex->id); search == distances.end()) {
        distances_[vertex] = INFINITY;
    }
    return distances_[vertex];
}

void ShortestPathsWorker::run() {
    ClientContext context;
    Job job;
    auto job_reader = std::make_unique<ClientReader<Job>>(stub_->job_stream(&context, my_address));
    std::thread dijkstra_worker;

    // czytamy joba typu start -> uruchamiamy wątek obliczeniowy
    // czytamy joba typu continue -> wsadzamy threadowi rzeczy na kolejkę i informujemy go o tym
    // nie przyjdzie już więcej rzeczy -> informujemy threada o tym i czekamy aż się skończy
    // wysyłamy wyniki do maina

    // while (job_reader->Read(&job)) {
    //     //TODO: how will we use job.id()? for logging?
    //     if (job.type_of_job() == JobType::START) {
    //         auto start_vertex = start;
    //         pq_mutex_->lock();
    //         { // TODO: the lock's scope should be shorter, like so;
    //             pq_->emplace(0, start_vertex, nullptr);
    //         }
    //         run_shortestpaths();
    //     }
    //     destination_ = job.end_vertex();
    //     destination_region_ = job.end_vertex_region();
    // }

    dijkstra_worker.join();
}

//FIXME
void ShortestPathsWorker::send_partial_path_results(const region_id_t region, const path_t& path, const dist_t distance) {
    ClientContext context;
    Ok ok;

    std::unique_ptr<ClientWriter<PathVert>> writer;
    if (region == MAIN_REGION) {
        writer = std::make_unique<ClientWriter<PathVert>>(stub_->path_found(&context, &ok));
    } else {
        writer = std::make_unique<ClientWriter<PathVert>>(neighbors_[region]->send_path(&context, &ok));
    }

    for (const Edge& edge : path) {
        PathVert msg;
        msg.set_islastvert(i == path.size());
        msg.set_distance(path[i].first);
        msg.set_ver_id(path[i].second->id);
        writer->Write(msg);
    }
}

void ShortestPathsWorker::retrieve_path(const Vertex& vertex) {
    std::vector<std::shared_ptr<Vertex>>2{last_vertex};
    auto last_vertex_res = get_distance(vertex);
    while (true){
        auto father = shortestpaths_res.find(last_vertex);
        //may error check find end, nullptr
        if (father->second.second == nullptr){
            send_path_back_to_main(path);
        }
        //tochange_names
        path.push_back(father->second.second);
        if (father->second.second->region != region_){
            send_path(father->second.second->region, path,distance);
        } else {
            last_vertex = father->second.second; 
        }
    }
}

void ShortestPathsWorker::dijkstra_within_region(const std::shared_ptr<Vertex>& destination) {
    while (!pq_->empty()) {
        auto top = pq_->top();
        pq_->pop();

        if (top.distance_ > get_distance(top.vertex_)) { 
            continue; // top is outdated
        } 

        for (const Edge& edge : top.vertex_->edges_) {
            dist_t new_dist = top.distance_ + edge.weight;
            if (edge.reaches_optimally(destination->region_) {
                if (region_ != edge.end_->region_) {
                    (*neighbor_jobs_)[edge.end_->region_]
                        .emplace_back(new_dist, edge.end_, top.vertex_);
                } else if (new_dist < get_distance(edge.end_)) {
                    distances[edge.end_] = new_dist;
                    parents[edge.end_] = top.vertex_;
                    pq->emplace(new_dist, edge.end_, top.vertex_);
                }
            }
        }
    }
}

void ShortestPathsWorker::compute_shortest_paths(const std::shared_ptr<Vertex>& destination) {
    while (phase_ == ComputationPhase::WAIT_FOR_JOBS) {
        //raz serwer, raz client (todo: co to znaczy?)
        std::unique_lock<std::mutex> lock(*pq_mutex_);
        pq_cond_->wait(lock, [] { return phase_ != ComputationPhase::WAIT_FOR_JOBS; });

        if (phase_ == ComputationPhase::WAIT_FOR_PATH_RETRIEVAL) {
            break;
        }

        assert(phase_ == ComputationPhase::HANDLE_JOBS);

        dijkstra(destination);
        //todo: rozładuj payloady
        phase_ = ComputationPhase::WAIT_FOR_JOBS;
        pq_cond_->notify_one();
    }

    // we've reached the destination => we're the last worker
    if (get_distance(distance) != INFINITY) {
        phase_ = ComputationPhase::RETRIEVE_PATH;
    } else {
        std::unique_lock<std::mutex> lock(*pq_mutex_);     
        pq_cond_->wait(lock, [] { return phase_ != ComputationPhase::WAIT_FOR_PATH_RETRIEVAL; });
    }

    retrieve_path(0);
    pq_cond_->notify_one();  
}
