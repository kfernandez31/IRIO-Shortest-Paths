#include <thread>
#include <memory>
#include <queue>
#include <algorithm>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "dijkstra_common.hh"
#include "dijkstra.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ServerContext;
using grpc::Status;
using dijkstra::RegionProcessor;
using dijkstra::DijkstraQuery;
using dijkstra::DijkstraReply;

using pdv_t = std::pair<dist_t, vertex_id_t>; //TODO: a more verbose struct for this ()
using graph_t = std::vector<std::vector<pdv_t>>;
using maxheap_t = std::priority_queue<pdv_t, std::vector<pdv_t>, std::greater<pdv_t>>;

//TODO: a non-blocking approach on the computed path

class RegionProcessorServiceImpl final : public RegionProcessor::Service {
public:
  region_id_t region;

  RegionProcessorServiceImpl(
    std::map<region_id_t, std::shared_ptr<Channel>>& channels,
    region_id_t region, 
    std::shared_ptr<graph_t> graph,
    std::shared_ptr<maxheap_t> pq,
    std::shared_ptr<std::vector<vertex_id_t>> parents,
    std::shared_ptr<std::vector<dist_t>> dist
  ): region(region), graph(graph), pq(pq), parents(parents), dist(dist) {
      for (region_id_t i = 0; i < NUM_REGIONS; i++) {
        if (i != region) { // We don't need a channel to ourselves
          processors[region] = RegionProcessor::NewStub(channels[region]);
        }
      }
    }

  // TODO: derefs
  void try_relax(const pdv_t cur, const pdv_t edge) {
    if ((*dist)[cur.second] + edge.first < (*dist)[edge.second]) {
      (*dist)[edge.second] = cur.first + edge.first;
      (*parents)[edge.second] = cur.second;
      (*pq).push({(*dist)[edge.second], edge.second});
    }
  }

  // TODO: derefs
  path_t retrieve_path(const vertex_id_t src, const vertex_id_t dst) const {
    std::vector<vertex_id_t> path;
    if ((*dist)[dst] != INFTY || src == dst) {
      for (vertex_id_t v = dst; v != src; v = (*parents)[v]) {
        path.push_back(v);
      }
      path.push_back(src);
      std::reverse(path.begin(), path.end()); //TODO: make this unnecessary
    }
    return path;
  }

  Status Dijkstra(
    ServerContext* context, 
    const DijkstraQuery* query, 
    DijkstraReply* reply) 
  override {
    auto is_first_processor = (bool) query->is_first_processor();
    auto src = (vertex_id_t) query->src_vertex();
    auto dst = (vertex_id_t) query->dst_vertex();
    std::cout << "[Worker #" << region << "]: starting Dijkstra..." << std::endl;

    while (!pq->empty()) {
      auto top = pq->top();
      auto [d_v, v] = top;
      pq->pop();

      if (d_v != (*dist)[v])
        continue; // pair is outdated
        
      if (region == get_region(v)) {
        for (const auto& edge : (*graph)[v]) {
          pq->pop();
          try_relax(top, edge);
        }
      } else {
        DijkstraQuery q;
        q.set_is_first_processor(false);
        q.set_src_vertex(v);
        q.set_dst_vertex(dst);
        DijkstraReply r;
        ClientContext ctx;
        Status status = processors[region]->Dijkstra(&ctx, q, &r);
        if (!status.ok()) {
          std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
          return status;
        }
      }
    }

    if (is_first_processor) {
      path_t path = retrieve_path(src, dst);
      dist_t dist = (*dist)[(size_t)dst];
      for (vertex_id_t v : path) {
        reply->add_path(v);
      }
      reply->set_dist(dist);
    }
    return Status::OK;
  }

private:
  std::map<region_id_t, std::unique_ptr<RegionProcessor::Stub>> processors;
  std::shared_ptr<graph_t> graph;
  std::shared_ptr<maxheap_t> pq;
  std::shared_ptr<std::vector<vertex_id_t>> parents;
  std::shared_ptr<std::vector<dist_t>> dist;
};

void RunWorker(RegionProcessorServiceImpl service, std::string addr) {
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Worker #" << service.region <<  " listening on " << addr << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  auto graph = std::make_shared<graph_t>({
    {{4, 1}, {8, 7}}, 
    {{11, 7},  {8, 2}, {4, 0}}, 
    {{8, 1}, {2, 8}, {4, 5}},
    {{7, 2}, {14, 5}, {9, 4}},
    {{9, 3}, {10, 5}},
    {{2, 6}, {14, 3}, {10, 4}},
    {{1, 7}, {6, 8}, {2, 5}},
    {{8, 0}, {11, 1}, {7, 8}},
    {{2, 2}, {7, 7}, {6, 6}}
  });
  auto pq = std::make_shared<maxheap_t>();
  auto parents = std::make_shared<std::vector<vertex_id_t>>(NUM_REGIONS, -1);
  auto dist = std::make_shared<std::vector<dist_t>>(NUM_REGIONS, INFTY);
    
  std::map<region_id_t, Channel>> channels; // a routing map
  for (region_id_t i = 0; i < NUM_REGIONS; i++) {
    auto addr = server_addr + ":" + std::to_string(base_port + i);
    channels[i] = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
  }

  std::vector<std::thread> workers;
  for (region_id_t i = 0; i < NUM_REGIONS; i++) {
    auto service = RegionProcessorServiceImpl(channels, region, graph, pq, parents, dist);
    workers.push_back(RunWorker, std::move(service), addresses[i]);
  }

  for (auto& worker : workers) {
    worker.join();
  }

  return 0;
}
