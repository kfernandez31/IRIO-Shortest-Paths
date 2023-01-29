#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"

#include "common.hh"
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <tuple>

using namespace shortestpaths;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;
using grpc::ClientWriter;


class ShortestPathsWorkerClient {
public:
    ShortestPathsWorkerClient(
        const std::shared_ptr<Channel> channel,
        std::shared_ptr<WorkerState> worker_state,
        const std::string& main_address,
        const std::string& own_address
    );

    void run();
    void compute_shortest_paths();
    void compute_phase();

private:
    void post_request_cleanup();
    void send_to_neighbors();
    dist_t get_distance(const vertex_id_t vertex);
    bool dijkstra_within_region();
    void send_end_of_phase_to_main(bool anything_to_send);
    void send_end_of_echange_to_main();

    
    
    std::shared_ptr<WorkerState> worker_state_;

    std::shared_ptr<ShortestPathsMainService::Stub> stub_;
    //std::shared_ptr<std::map<region_id_t, std::shared_ptr<ShortestPathsWorkerService::Stub>>> neighbors_;
    
    //TODO: are these needed?
    std::string address_;
    std::string main_address_;

    region_id_t region_;
    //std::shared_ptr<std::map<vertex_id_t, std::shared_ptr<Vertex>>> my_vertices_;

    //vertex_id_t destination_;
    //region_id_t destination_region_;


    std::map<region_id_t, std::vector<ContinueJob>> neighbors_jobs_;

    //std::shared_ptr<std::atomic_int> if_get_path_then_vertex_id;
    //std::shared_ptr<std::atomic_int> my_turn_in_end; // 0 if waiting for main, 1 if to get path, 2 if to clean
};



