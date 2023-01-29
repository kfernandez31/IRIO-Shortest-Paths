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

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cout << "Usage: ./worker main_server_address worker_address" << std::endl;
    }

    auto main_server_address = std::string(argv[1]);
    auto worker_address = std::string(argv[2]);
    auto send_address = std::string(argv[3]);

    std::cout << "GRAPH ACQUIRED" << std::endl;

    auto worker_state = std::make_shared<WorkerState>(); 
    worker_state->phase_ = WorkerComputationPhase::AWAIT_MAIN;

    std::cout << "CREATED WORKER STATE" << std::endl;

    
    auto worker_client = ShortestPathsWorkerClient(
        grpc::CreateChannel(main_server_address, grpc::InsecureChannelCredentials()), //TODO: do we want authentication?
        worker_state,
        main_server_address, 
        worker_address
    );

    auto worker_server = std::make_shared<ShortestPathsWorkerServer>(worker_state, send_address);

    auto client_thread = std::thread(&ShortestPathsWorkerClient::run, &worker_client);
    worker_server->run();

    return 0;
}
