#!/bin/python3

import pandas as pd

edges_df = pd.read_csv('edges.csv')

out_df = pd.DataFrame([], columns=['source', 'target', 'length'])

for _, row in edges_df.iterrows():
    source, target, length = row['source'], row['target'], row['length']
    longer = edges_df[(edges_df['length'] > length) & ((edges_df['source'] == source) & (edges_df['target'] == target)) | ((edges_df['source'] == target) & (edges_df['target'] == source))]
    if (len(longer) == 0):
        new_row = {'source': int(min(source, target)), 'target': int(max(source, target)), 'length': int(length)}
        out_df.loc[len(out_df.index)] = new_row

out_df.drop_duplicates().to_csv('edges_out.csv', index=False)
