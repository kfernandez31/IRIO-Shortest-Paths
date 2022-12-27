#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "dijkstra.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using dijkstra::EdgeLen;
using dijkstra::LenQuery;
using dijkstra::LenReply;


// Logic and data behind the server's behavior.
class EdgeLenServiceImpl final : public EdgeLen::Service {

  std::map<std::tuple<int,int>, int> edges; 
  public:
    EdgeLenServiceImpl(std::map<std::tuple<int,int>, int> edges) {
      this->edges = edges;
    }


  Status CheckLen(ServerContext* context, const LenQuery* query,
                  LenReply* reply) override {
    std::cout << "in here" << query->v1() << query->v2() << std::endl;
    int v1 = (int) query->v1();
    int v2 = (int) query->v2();
    int len = edges[std::make_tuple(v1, v2)];
    reply->set_len(len);
    std::cout << v1 << " " << v2 << " " << len << std::endl;
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");

  std::map<std::tuple<int,int>, int> example_edges{{std::make_tuple(0,1), 1},
                                                   {std::make_tuple(1,2), 10},
                                                   {std::make_tuple(1,3), 2},
                                                   {std::make_tuple(2,3), 3},
                                                   {std::make_tuple(2,4), 5},
                                                   {std::make_tuple(1,0), 1},
                                                   {std::make_tuple(2,1), 10},
                                                   {std::make_tuple(3,1), 2},
                                                   {std::make_tuple(3,2), 3},
                                                   {std::make_tuple(4,2), 5}};
  EdgeLenServiceImpl service(example_edges);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
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
