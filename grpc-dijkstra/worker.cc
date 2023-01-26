#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"

#include "worker_client.hh"
#include "worker_server.hh"
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
#include <bitset>
#include <optional>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cout << "Usage: ./worker main_server_address worker_address" << std::endl;
    }

    auto main_server_address = std::string("localhost:50051");
    auto worker_address = std::string(argv[2]);

    std::cout << "GRAPH ACQUIRED" << std::endl;

    auto queue_mut = std::make_shared<std::mutex>();
    auto pq = std::make_shared<maxheap_t>();
    auto phase = std::make_shared<WorkerComputationPhase>(WorkerComputationPhase::WAITING_FOR_QUERY);
    auto neigbors = std::make_shared<std::map<region_id_t, std::shared_ptr<ShortestPathsWorkerService::Stub>>>();
    auto parents = std::make_shared<std::map<vertex_id_t, std::pair<vertex_id_t, region_id_t>>>();
    auto distances = std::make_shared<std::map<vertex_id_t, dist_t>>();
    auto my_vertices = std::make_shared<std::map<vertex_id_t, std::shared_ptr<Vertex>>>();
    auto worker_client = ShortestPathsWorkerClient(
        grpc::CreateChannel(main_server_address, grpc::InsecureChannelCredentials()), //TODO: do we want authentication?
        queue_mut, 
        pq, 
        phase,
        neigbors,
        parents, 
        distances,
        my_vertices,
        main_server_address, 
        worker_address
    );

    auto worker_server = std::make_shared<ShortestPathsWorkerServer>(queue_mut, pq, phase, neigbors, parents, distances,my_vertices, worker_address);

    auto client_thread = std::thread(&ShortestPathsWorkerClient::run, &worker_client);
    worker_server->run();

    return 0;
}
