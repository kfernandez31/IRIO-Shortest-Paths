#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "caching_interceptor.hh"

#include <grpcpp/grpcpp.h>

#include "shortestpaths.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using shortestpaths::Router;
using shortestpaths::ShortestPathRequest;
using shortestpaths::ShortestPathResponse;

class ShortestPathClient {
public:
    ShortestPathClient(std::shared_ptr<Channel> channel)
        : stub_(Router::NewStub(channel)) {}

    void GetShortestPaths(const std::vector<std::pair<int, int>>& payloads) {
        ClientContext context;
        auto stream = stub_->GetShortestPaths(&context);

        for (const auto& payload : payloads) {
            ShortestPathRequest request;
            request.set_source(payload.first);
            request.set_destination(payload.second);
            stream->Write(request);

            ShortestPathResponse response; 
            stream->Read(&response);
            std::cout << "(" << payload.first << ", " << payload.second << ") : " << response.distance() << "\n";
        }

        stream->WritesDone();
        Status status = stream->Finish();
        if (!status.ok()) {
            std::cout << "Error when closing stream: " << status.error_code() << ": " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<Router::Stub> stub_;
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  // In this example, we are using a cache which has been added in as an
  // interceptor.
    grpc::ChannelArguments args;
    std::vector<std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>> interceptor_creators;
    interceptor_creators
        .push_back(std::unique_ptr<CachingInterceptorFactory>(new CachingInterceptorFactory()));
    auto channel = grpc::experimental::CreateCustomChannelWithInterceptors(
        "localhost:50051", grpc::InsecureChannelCredentials(), args,
        std::move(interceptor_creators)
    );
    ShortestPathClient client(channel);

    std::vector<std::pair<int, int>> payloads = {{1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}};
    client.GetShortestPaths(payloads);

    return 0;
}
