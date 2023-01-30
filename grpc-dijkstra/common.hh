#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"
#include <queue>
#include <map>
#include <bitset>
#include <memory>
#include <limits>
#include <condition_variable>
#include <mutex>

using namespace shortestpaths;

static constexpr int NUM_PARTITIONS = 2;

enum WorkerComputationPhase {
    AWAIT_MAIN = 0,
    HANDLE_JOBS=1,
    END_OF_EXCHANGE = 2,
    STOP_WORK=3 //TODO: will this be ever used?
};


enum MainComputationPhase{
    STARTUP = 0,
    WAIT_FOR_QUERY = 1, //server
    SEND_QUERY = 2, //client
    WAIT_FOR_EXCHANGE = 3, //server --- tutaj dostajemy end_computephase
    EXCHANGE_PHASE_MAIN = 4, 
    RETRIEVE_PATH_MAIN =5,
};


using vertex_id_t = int;// @todo
using region_id_t = int;// @todo
using edge_id_t = int;// @todo
using dist_t = uint64_t; // @todo
using pdv_t = std::pair<dist_t, vertex_id_t>; //tochange
using graph_t = std::map<std::pair<vertex_id_t, vertex_id_t>, dist_t>;
class Vertex;

class Edge {
public:
    dist_t weight_;
    vertex_id_t end_;
    region_id_t end_region_;

    Edge(const dist_t weight, const vertex_id_t end,  region_id_t end_region, std::vector<bool> region_mask): 
        weight_(weight), 
        end_(end), 
        end_region_(end_region), 
        region_mask_(region_mask) {};

    bool reaches_optimally(const region_id_t region) const
    {
        return region_mask_[region];
    }

private:
    std::vector<bool> region_mask_; //@todo
};

class Vertex {
public:
    vertex_id_t id;
    region_id_t region_;
    std::vector<Edge> edges;
    Vertex(vertex_id_t _id, region_id_t _region, std::vector<Edge> _edges): id(_id), region_(_region), edges(_edges){};
};

using vertex_path_info_t = std::tuple<dist_t, vertex_id_t, vertex_id_t, region_id_t>;
using maxheap_t = std::priority_queue<vertex_path_info_t, std::vector<vertex_path_info_t>, std::greater<vertex_path_info_t>>; 

static constexpr dist_t INF = std::numeric_limits<dist_t>::max();


std::map<vertex_id_t, std::shared_ptr<Vertex>> load_graph(region_id_t region_num);

std::map<region_id_t, std::vector<region_id_t>> load_region_borders();


class WorkerState{
public:
    WorkerComputationPhase phase_;
    std::mutex phase_mutex_;
    std::condition_variable phase_cond_;
    size_t notification_counter;
    maxheap_t pq_;
    std::map<vertex_id_t, std::pair<vertex_id_t, region_id_t>> parents_;
    std::map<vertex_id_t, dist_t> distances_; 
    std::map<vertex_id_t, std::shared_ptr<Vertex>> my_vertices_;
    std::map<region_id_t, std::shared_ptr<ShortestPathsWorkerService::Stub>> neighbors_;
    vertex_id_t destination_;
    region_id_t destination_region_;
};

