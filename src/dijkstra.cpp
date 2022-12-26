#include <bits/stdc++.h>
using namespace std;

/**
Just a playground for writing (pseudo)code.
*/

#define INF LLONG_MAX
#define NUM_COARSE_PARTITIONS 25
#define NUM_FINE_PARTITIONS 9

using plli = std::pair<long long, int>;

const int maxN = 5e5;
int n, m;
std::vector<pair<int, int>> adj[maxN + 1];

struct vertex {
    size_t coarse_region, fine_region;
    std::bitset<NUM_COARSE_PARTITIONS> coarse_mask;
    std::bitset<NUM_FINE_PARTITIONS> fine_mask;

    bool is_reachable(size_t region, bool coarse = true) const {
        if (coarse) {
            return coarse_mask.test(region);
        } else {
            return fine_mask.test(region);
        }
    }
};

struct Graph {

    
};

std::pair<long long, std::vector<int>> dijkstra(int src, int dest) {
    std::vector<int> parent(0, n + 1);
    std::vector<long long> dist(INF, n + 1);
    std::priority_queue<plli, std::vector<plli>, std::greater<plli>> pq;

    for (int i = 1; i <= n; i++) {
        dist[i] = (i != src) * INF;
        parent[i] = 0;
    }
    pq.push({0, src});

    while (!pq.empty()) {
        auto [d_v, v] = pq.top();
        pq.pop();

        if (d_v != dist[v])
            continue; // TODO: why?

        for (auto [to, len] : adj[v]) {
            if (dist[v] + len < dist[to]) {
                dist[to] = d_v + len;
                parent[to] = v;
                pq.push({dist[to], to});
            }
        }
    }

    std::vector<int> path;
    if (dist[dest] != INF) {
        for (int v = dest; v != src; v = parent[v]) {
            path.push_back(v);
        }
        path.push_back(src);
        std::reverse(path.begin(), path.end());
    }

    return {dist[dest], path};
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

    int src, dest;
    cin >> src >> dest;

    auto res = dijkstra(src, dest);

    cout << res.first << "\n";
    for (int v : res.second) {
        cout << v << " ";
    }
    cout << "\n";

    return 0;
}