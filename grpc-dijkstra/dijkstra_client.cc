
#include <iostream>
#include <memory>
#include <string>
#include <queue>

#include <grpcpp/grpcpp.h>
#include "dijkstra.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using dijkstra::EdgeLen;
using dijkstra::LenQuery;
using dijkstra::LenReply;

class DijkstraClient {
 public:
  DijkstraClient(std::shared_ptr<Channel> channel)
      : stub_(EdgeLen::NewStub(channel)) {}


    int GetEdge(int v1, int v2) {
      LenQuery query;
      query.set_v1(v1);
      query.set_v2(v2);

      LenReply reply;

      ClientContext context;
      Status status = stub_->CheckLen(&context, query, &reply);

      if (status.ok()) {
        return reply.len();
      } 
      else {
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
        return -1;
      }
    }


    void FindShortest(int source, std::vector<std::vector<int>> edges) {
      int n = edges.size();
      bool visited[n];
      int dist[n];
      std::priority_queue<std::pair<int, int>> q;

      for (int i = 0; i < n; i++) {
        visited[i] = false;
        dist[i] = 100000;
        q.push(std::make_pair(dist[i], i));
      }
      dist[source] = 0;

      while (!q.empty()) {
         int current = q.top().second;
         q.pop();
         for (int i = 0; i < edges[current].size(); i++) {
            int v = edges[current][i];
            int weight = this->GetEdge(current, v);

            if (dist[v] > dist[current] + weight) {
                dist[v] = dist[current] + weight;
                q.push(std::make_pair(dist[v], v));
            }
         }
      }

      for (int i = 0; i< n; i++) {
        std::cout << "Vertex: " << i << " distance from " << source << " is: " << dist[i] << std::endl;
      }
    }

 private:
  std::unique_ptr<EdgeLen::Stub> stub_;
  std::set<int, int> edges;
};

int main(int argc, char** argv) {
  std::string target_str;
  target_str = "localhost:50051";

  std::vector<std::vector<int>> edges{{1}, {0, 2, 3}, {1, 3, 4}, {1, 2}, {2}};

  DijkstraClient client(
    grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials())
  );

  client.FindShortest(0, edges);
  return 0;
}
