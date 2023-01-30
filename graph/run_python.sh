#!/bin/bash

./prepare_nodes.py $1
./prepare_edges.py
./prepare_direct_connections.py
./prepare_optimal_connections.py
./csv_to_postgres.py $2 $3 $4 $5 $6
