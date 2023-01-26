#!/bin/python3

import sys
import pandas as pd
from collections import defaultdict
import heapq

nodes_df = pd.read_csv('nodes_out.csv')
edges_df = pd.read_csv('edges_out.csv')

adj = defaultdict(dict)
region = {}
connections = defaultdict(set)

def init_graph():
    global adj, region
    for _, row in edges_df.iterrows():
        source, target, length = row['source'], row['target'], row['length']
        adj[source][target] = length
        adj[target][source] = length

    for _, row in nodes_df.iterrows():
        region[row['id']] = row['region']

def get_path(parent, src, dst):
    path = []
    if (parent[dst] != -1):
        v = dst
        while v != src:
            path.append(v)
            v = parent[v]
        path.append(src)
    return path

def dijkstra(src):
    dist = defaultdict(lambda: sys.maxsize)
    parent = defaultdict(lambda: -1)
    pq = [(0, src)]

    dist[src] = 0
    heapq.heappush(pq, (0, src))
    while (len(pq) > 0):
        d_cur, cur = heapq.heappop(pq)

        if d_cur > dist[cur]:
            continue

        for next in adj[cur].keys():
            if dist[next] > dist[cur] + adj[cur][next]:
                dist[next] = dist[cur] + adj[cur][next]
                heapq.heappush(pq, (dist[next], next))
                parent[next] = cur

    for dst in adj.keys():
        path = get_path(parent, src, dst)
        acc = set()
        prev = -1
        for v in path:
            if prev != -1:
                connections[v, prev].update(acc)
            acc.add(region[v])
            prev = v

init_graph()
for v in adj.keys():
    dijkstra(v)

out_df = pd.DataFrame([], columns=['source', 'target', 'region'])

for source, target in connections.keys():
    for region in connections[source, target]:
        new_row = {'source': int(source), 'target': int(target), 'region': int(region)}
        out_df.loc[len(out_df.index)] = new_row

out_df.drop_duplicates().to_csv('optimal_connections_out.csv', index=False)
