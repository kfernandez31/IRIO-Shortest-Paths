#include <algorithm>
#include <compare>
#include <vector>
#include <queue>
#include <bitset>
#include <iostream>
#include <climits>
#include <stddef.h>

#define INF LLONG_MAX
#define NUM_COARSE_PARTITIONS 25
#define NUM_FINE_PARTITIONS 9
#define PLACEHOLDER_VERTEX_ID 0

typedef size_t vertex_id_t;
typedef size_t region_id_t;
typedef size_t edge_id_t;
typedef size_t dist_t;

struct graph {
    using pdv = std::pair<dist_t, vertex_id_t>;

    struct edge {
        dist_t len;
        vertex_id_t to;

        std::bitset<NUM_COARSE_PARTITIONS> coarse_mask;
        std::bitset<NUM_FINE_PARTITIONS> fine_mask;

        edge(dist_t len, vertex_id_t to) : len(len), to(to) {}
        
        bool is_reachable_within_shortest_path(const region_id_t region, const bool coarse = true) const {
            if (coarse) {
                return coarse_mask.test(region);
            } else {
                return fine_mask.test(region);
            }
        }
    };

    struct vertex {
        region_id_t coarse_region_id, fine_region_id;

        vertex(region_id_t coarse_region_id, region_id_t fine_region_id): coarse_region_id(coarse_region_id), fine_region_id(fine_region_id) {}

        vertex(): vertex(PLACEHOLDER_VERTEX_ID, PLACEHOLDER_VERTEX_ID) {}
    };

    std::vector<std::vector<edge>> adj;
    std::vector<vertex> vertices;

    graph(vertex_id_t n) {
        adj = std::vector<std::vector<edge>>(n);
        vertices = std::vector<vertex>(n);
    }

    inline void add_edge(dist_t weight, vertex_id_t a, vertex_id_t b) {
        adj[a].push_back({weight, b});
        adj[b].push_back({weight, a});
    }

    void preprocess() {
        // TODO
    }

    std::vector<vertex_id_t> retrieve_path(
        const vertex_id_t src_id, 
        const vertex_id_t dst_id, 
        const std::vector<vertex_id_t>& parent, 
        const std::vector<dist_t>& dist
    ) {
        std::vector<vertex_id_t> path;
        if (dist[dst_id] != INF || src_id == dst_id) {
            for (int v = dst_id; v != src_id; v = parent[v]) {
                path.push_back(v);
            }
            path.push_back(src_id);
            std::reverse(path.begin(), path.end());
        }
        return path;
    }

    std::pair<dist_t, std::vector<vertex_id_t>> dijkstra(vertex_id_t src_id, vertex_id_t dst_id) {
        std::vector<vertex_id_t> parent(0, vertices.size());
        std::vector<dist_t> dist(INF, vertices.size());
        std::priority_queue<pdv, std::vector<pdv>, std::greater<pdv>> pq;

        vertex dst_vertex = vertices[dst_id];

        dist[src_id] = 0;
        pq.push({0, src_id});

        while (!pq.empty()) {
            auto [d_v, v] = pq.top();
            pq.pop();

            if (d_v != dist[v])
                continue; // pair is outdated

            for (const edge& e : adj[v]) {
                vertex to_vertex = vertices[e.to];
                if (e.is_reachable_within_shortest_path(dst_vertex.coarse_region_id)) {
                    if ((to_vertex.coarse_region_id != dst_vertex.coarse_region_id) || 
                        (to_vertex.coarse_region_id == dst_vertex.coarse_region_id && 
                            to_vertex.fine_region_id == dst_vertex.fine_region_id
                        )
                    ) {
                    if (dist[v] + e.len < dist[e.to]) {
                        dist[e.to] = d_v + e.len;
                        parent[e.to] = v;
                        pq.push({dist[e.to], e.to});
                    }
                }
            }
        }
        }
        
        auto path = retrieve_path(src_id, dst_id, parent, dist);
        return {dist[dst_id], path};
    }
};

int main() {
    std::ios_base::sync_with_stdio(0); 
    std::cin.tie(0);

    vertex_id_t n;
    edge_id_t m;
    std::cin >> n >> m;
    graph g(n);
    while (m--) {
        vertex_id_t a, b;
        dist_t w;
        std::cin >> a >> b >> w;
        g.add_edge(w, a, b);
    }

    vertex_id_t src, dst;
    std::cin >> src >> dst;

    auto res = g.dijkstra(src, dst);

    std::cout << res.first << "\n";
    for (auto v : res.second) {
        std::cout << v << " ";
    }
    std::cout << "\n";

    return 0;
}