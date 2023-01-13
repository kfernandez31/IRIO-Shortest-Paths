#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include <grpcpp/grpcpp.h>

#include "shortestpaths.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using shortestpaths::Router;
using shortestpaths::ShortestPathRequest;
using shortestpaths::ShortestPathResponse;

static std::map<std::pair<int, int>, int> storage = {
    {std::make_pair(1, 1), 0},
    {std::make_pair(1, 2), 1},
    {std::make_pair(1, 3), 2},
    {std::make_pair(1, 4), 3},
    {std::make_pair(1, 5), 4},
};

static int get_value_from_storage(int source, int destination) {
    return storage[std::make_pair(source, destination)];
}

// Logic and data behind the server's behavior.
class RouterServiceImpl final : public Router::Service {
    Status GetShortestPaths(ServerContext* context, ServerReaderWriter<ShortestPathResponse, ShortestPathRequest>* stream) override {
        ShortestPathRequest request;
        while (stream->Read(&request)) {
            ShortestPathResponse response;
            response.set_distance(get_value_from_storage(request.source(), request.destination()));
            stream->Write(response);
        }
        return Status::OK;
    }
};

static void RunServer() {
    std::string server_address("0.0.0.0:50051");
    RouterServiceImpl service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials()); //TODO
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case, it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
