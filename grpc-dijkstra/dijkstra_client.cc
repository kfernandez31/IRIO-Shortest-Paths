#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "dijkstra_common.hh"
#include "dijkstra.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using dijkstra::RegionProcessor;
using dijkstra::DijkstraQuery;
using dijkstra::DijkstraReply;

class DijkstraClient {
public:
  DijkstraClient(
    std::map<region_id_t, std::shared_ptr<Channel>>& channels
  ) {
    for (region_id_t i = 0; i < NUM_REGIONS; i++) {
      processors[i] = RegionProcessor::NewStub(channels[i]);
    }
  }

  void GetShortestPath(const vertex_id_t src, const vertex_id_t dst) {
    DijkstraQuery query;
    query.set_is_first_processor(true);
    query.set_src_vertex(src);
    query.set_dst_vertex(dst);
    DijkstraReply reply;
    ClientContext ctx;
    Status status = processors[get_region(src)]->Dijkstra(&ctx, query, &reply);
    if (!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
    } else {
      std::string delimeter = " --> ";
      auto dist = (dist_t) reply.dist();
      std::cout << "Shortest path " << src << " --- " << dst << ":\n"; 
      if (dist == INFTY) {
        std::cout << "  NONE!";
      } else {
        std::cout<< "  distance: " << dist << std::endl;
        std::cout << "path: " << src;
        for (int i = 1; i < reply.path_size(); i++) {
          std::cout << delimeter << reply.path(i);
        }
        std::cout << std::endl;
      }
    }
  }

private:
  std::map<region_id_t, std::unique_ptr<RegionProcessor::Stub>> processors;
};

int main(int argc, char** argv) {
  std::map<region_id_t, Channel> channels; // a routing map
  for (region_id_t i = 0; i < NUM_REGIONS; i++) {
    auto addr = server_addr + ":" + std::to_string(base_port + i);
    channels[i] = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
  }

  DijkstraClient client(channels);

  for (vertex_id_t src = 0; src <= 8; src++) {
    for (vertex_id_t dst = 0; dst <= 8; dst++) {
      client.GetShortestPath(src, dst);
    }
  }

  return 0;
}
