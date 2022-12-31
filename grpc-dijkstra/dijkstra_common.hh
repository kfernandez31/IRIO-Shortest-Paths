#include <iostream>
#include <string>
#include <queue>
#include <vector>

using dist_t = int;
using vertex_id_t = int;
using path_t = std::vector<vertex_id_t>;
using region_id_t = int;  //TODO: this shouldn't be known by the client

const std::string server_addr = "localhost";  
const region_id_t NUM_REGIONS = 5; //TODO: this shouldn't be known by the client
const dist_t INFTY = INT_MAX;
size_t base_port = 500050;

inline region_id_t get_region(vertex_id_t vertex) {  //TODO: this shouldn't be known by the client
    return vertex % NUM_REGIONS;
}


