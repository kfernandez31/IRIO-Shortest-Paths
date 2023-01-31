#!/bin/python3

import sys
import pandas as pd
from collections import defaultdict

nodes_df = pd.read_csv('nodes_out.csv')
edges_df = pd.read_csv('edges_out.csv')

adj = defaultdict(set)
region = {}
connections = set()

def init_graph():
    global adj, region
    for _, row in edges_df.iterrows():
        source, target = row['source'], row['target']
        adj[source].add(target)
        adj[target].add(source)

    for _, row in nodes_df.iterrows():
        region[row['id']] = row['region']

def get_neighbors():
    for v in adj.keys():
        for w in adj[v]:
            if region[v] != region[w]:
                connections.add((region[v], region[w]))

init_graph()
get_neighbors()

out_df = pd.DataFrame([], columns=['region_1', 'region_2'])

for reg1, reg2 in connections:
    new_row = {'region_1': int(reg1), 'region_2': int(reg2)}
    out_df.loc[len(out_df.index)] = new_row

out_df.drop_duplicates().to_csv('direct_connections_out.csv', index=False)
