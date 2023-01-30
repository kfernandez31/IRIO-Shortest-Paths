1. Get `nodes.csv` and `edges.csv` from the `.osm.pbf` file:
```
cargo install osm4routing
osm4routing2 monaco-latest.osm.pbf
```

2. Obtain the `_out.csv` files by running the following Python scripts in this order: 

```
./prepare_nodes.py <clusters>
./prepare_edges.py
./prepare_direct_connections.py
./prepare_optimal_connections.py
```

3. Upload data from `_out.csv` files to Postgres:
```
./csv_to_postgres.py <db> <user> <passwd> <host> <port>
```

You can also run all of the above with the following:
```
./run_python.sh <clusters> <db> <user> <passwd> <host> <port>
```