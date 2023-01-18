#pragma once


#include <queue>
#include <map>
#include "limits.h"

using vertex_id_t = int;
using region_id_t = int;
using edge_id_t = int;
using dist_t = int;
using pdv_t = std::pair<dist_t, vertex_id_t>;
using graph_t = std::map<std::pair<vertex_id_t, vertex_id_t>, dist_t>;
using maxheap_t = std::priority_queue<pdv_t, std::vector<pdv_t>, std::greater<pdv_t>>;
using path_t = std::vector<Edge>

const region_id_t MAIN_REGION = -1;
