#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"


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

using namespace shortestpaths;
using namespace std;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;
using grpc::ClientWriter;


int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "Usage: ./main port" << std::endl;
    }

    auto main_server_address = std::string(argv[1]);
   
    auto channel = grpc::CreateChannel(main_server_address, grpc::InsecureChannelCredentials());
    auto stub = shortestpaths::ShortestPathsMainService::NewStub(channel);
    ClientContext context;
    Ok ok;
    ClientQuery query;
    query.set_start_vertex(3460511791);
    query.set_end_vertex(1204288441);
    query.set_start_vertex_region(2);
    query.set_end_vertex_region(13);
    
    stub->client_query(&context, query, &ok);
    std::cout << "SENT QUERY" <<std::endl;


    return 0;


}