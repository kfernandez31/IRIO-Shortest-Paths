#include "client.hh"

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

ShortestPathClient::ShortestPathClient(std::shared_ptr<Channel> channel)
    : stub_(Router::NewStub(channel)) {}

region_id_t ShortestPathClient::get_region(const vertex_id_t vertex) {
    ClientContext context;
    RegionQuery query;
    RegionReply reply;

    query.set_vertex(vertex);

    stub_->get_region(&context, request, &reply);

    return reply.region();
}

dist_t ShortestPathClient::get_shortest_path(const vertex_id_t start, const vertex_id_t end) {
    ClientContext context;

    auto start_region = get_region(start);
    auto end_region = get_region(end);

    ClientContext context;
    Client Query query;
    Ok reply;

    query.set_start_vertex(start);
    query.set_end_vertex(end);
    query.set_start_vertex_region(start_region);
    query.set_end_vertex_region(end_region);

    stub_->client_query(&context, request, &reply);

    return reply.distance();
}
