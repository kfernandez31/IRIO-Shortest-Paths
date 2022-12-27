Short example of Dijkstra using grpc.

Client and server both know the layout of the graph, but only the server knows the weights of the edges.
Each time a client wants to see an edge weight it makes grpc request to server.
