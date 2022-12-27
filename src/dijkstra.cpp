#include <bits/stdc++.h>
using namespace std;

/**
Just a playground for writing (pseudo)code.
*/

#define INF LLONG_MAX
#define NUM_COARSE_PARTITIONS 25
#define NUM_FINE_PARTITIONS 9

typedef size_t vertex_id_t;
typedef size_t region_id_t;
typedef size_t edge_id_t;
typedef size_t dist_t;

using plli = std::pair<dist_t, edge_id_t>;

const int maxN = 5e5;
int n, m;
std::vector<pair<int, int>> adj[maxN + 1];

struct vertex {
    vertex_id_t id;
    edge_id_t coarse_region, fine_region;
};

struct edge {
    dist_t weight;
    vertex_id_t to;

    std::bitset<NUM_COARSE_PARTITIONS> coarse_mask;
    std::bitset<NUM_FINE_PARTITIONS> fine_mask;
    
    bool is_reachable_within_shortest(const region_id_t region, const bool coarse = true) const {
        if (coarse) {
            return coarse_mask.test(region);
        } else {
            return fine_mask.test(region);
        }
    }
};

struct Graph {

    
};

std::vector<vertex_id_t> retrieve_path(
    const vertex_id_t src, 
    const vertex_id_t dst, 
    const std::vector<vertex_id_t>& parent, 
    const std::vector<dist_t>& dist
) {
    std::vector<vertex_id_t> path;
    if (dist[dst] != INF || src == dst) {
        for (int v = dst; v != src; v = parent[v]) {
            path.push_back(v);
        }
        path.push_back(src);
        std::reverse(path.begin(), path.end());
    }
    return path;
}

std::pair<dist_t, std::vector<edge_id_t>> dijkstra(edge_id_t src, edge_id_t dst) {
    std::vector<edge_id_t> parent(0, n + 1);
    std::vector<dist_t> dist(INF, n + 1);
    std::priority_queue<plli, std::vector<plli>, std::greater<plli>> pq;

    for (edge_id_t i = 1; i <= n; i++) {
        dist[i] = (i != src) * INF;
        parent[i] = 0;
    }
    pq.push({0, src});

    while (!pq.empty()) {
        auto [d_v, v] = pq.top();
        pq.pop();

        if (d_v != dist[v])
            continue; // pair is outdated

        for (auto [to, len] : adj[v]) {
            if (dist[v] + len < dist[to]) {
                dist[to] = d_v + len;
                parent[to] = v;
                pq.push({dist[to], to});
            }
        }
    }
    
    auto path = retrieve_path(src, dst, parent, dist);
    return {dist[dst], path};
}

int main() {
    ios_base::sync_with_stdio(0); cin.tie(0);

    cin >> n >> m;
    while (m--) {
        int a, b, w;
        cin >> a >> b >> w;
        adj[a].push_back({w, b});
        adj[b].push_back({w, a});
    }

    vertex_id_t src, dst;
    cin >> src >> dst;

    auto res = dijkstra(src, dst);

    cout << res.first << "\n";
    for (auto v : res.second) {
        cout << v << " ";
    }
    cout << "\n";

    return 0;
}