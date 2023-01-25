#pragma once


#include <queue>
#include <map>
#include <bitset>
#include "limits.h"

static constexpr int NUM_PARTITIONS = 1;

enum WorkerComputationPhase {
    WAITING_FOR_QUERY = 0,
    EXCHANGE_PHASE =1,
    HANDLE_JOBS=2,
    END_OF_EXCHANGE = 3,
    WAIT_FOR_PATH_RETRIEVAL=4,
    RETRIEVE_PATH=5,
    STOP_WORK=6 //TODO: will this be ever used?
};


enum MainComputationPhase{
    STARTUP = 0,
    WAIT_FOR_QUERY = 1, //server
    SEND_QUERY = 2, //client
    WAIT_FOR_EXCHANGE = 3, //server --- tutaj dostajemy end_computephase
    EXCHANGE_PHASE_MAIN = 4, 
    RETRIEVE_PATH_MAIN =5,
    WAIT_FOR_PATH_RETRIEVAL_MAIN = 6,
    BROADCAST_END_OF_QUERY= 7
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
    std::shared_ptr<Vertex> end_;

    Edge(const dist_t weight, const std::shared_ptr<Vertex> end): weight_(weight_), end_(end_) {};

    bool reaches_optimally(const region_id_t region) const
    {
        return region_mask.test(region);
    }

private:
    std::bitset<NUM_PARTITIONS> region_mask; //@todo
};

class Vertex {
public:
    vertex_id_t id;
    region_id_t region_;
    std::vector<Edge> edges;
};



using vertex_path_info_t = std::tuple<dist_t, vertex_id_t, vertex_id_t>;
using maxheap_t = std::priority_queue<vertex_path_info_t, std::vector<vertex_path_info_t>, std::greater<vertex_path_info_t>>; 
using path_t = std::vector<Edge>;
const region_id_t MAIN_REGION = -1;

static constexpr dist_t INF = std::numeric_limits<dist_t>::max();
