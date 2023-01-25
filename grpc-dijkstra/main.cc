#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"

#include "main_client.hh"
#include "main_server.hh"
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
    if (argc != 1) {
        std::cout << "Usage: ./main" << std::endl;
    }

    auto main_server_address = std::string("localhost:50051");
   

  //  auto main_client = ShortestPathsMainClient(
   //     grpc::CreateChannel(main_server_address, grpc::InsecureChannelCredentials())
    //);

    auto main_server = std::make_shared<ShortestPathsMainServer>(1);

    auto client_thread = std::thread(&ShortestPathsMainServer::run, &*main_server);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    builder.AddListeningPort(main_server_address, grpc::InsecureServerCredentials()); //TODO: do we want authentication?
    builder.RegisterService(&*main_server);
    auto server = builder.BuildAndStart();

    std::cout << "Server listening on " << main_server_address << std::endl;

    server->Wait();

    return 0;
}