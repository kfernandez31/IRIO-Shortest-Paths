#!/bin/python3

import sys
import os
import pandas as pd
from sklearn.cluster import KMeans

def print_usage():
    print('Usage: ' + os.path.basename(sys.argv[0]) + ' <clusters=[1..20]>')

if (len(sys.argv) != 2):
    print_usage()
    sys.exit(1)

n_clusters = int(sys.argv[1])

out_df = pd.read_csv('nodes.csv')

coords = out_df[['lon', 'lat']].to_numpy()
kmeans = KMeans(n_clusters=n_clusters, random_state=0, n_init="auto").fit(coords)
out_df['region'] = kmeans.labels_

out_df.to_csv('nodes_out.csv', index=False)

#Bonus: uncomment to see the map's plot

# import matplotlib.pyplot as plt
# import seaborn as sns

# plt.style.use("fivethirtyeight")
# plt.figure(figsize=(8, 8))

# scat = sns.scatterplot(
#     data=out_df,
#     x="lon",
#     y="lat",
#     hue="region",
#     palette="Set2",
# )

# scat.set_title(
#     "Clustering results from nodes.csv"
# )
# plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)

# plt.show()
