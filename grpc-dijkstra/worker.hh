

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"

#include "common.hh"

using namespace shortestpaths;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;
using grpc::ClientWriter;

using vertex_path_info_t = std::tuple<int64_t, std::shared_ptr<Vertex>, std::shared_ptr<Vertex>>; //TODO: why is this a tuple

class DijkstraPayload {
public:
    vertex_id_t parent_;
    vertex_id_t child_;
    dist_t distance_from_start_; //TODO: may be not needed
    dist_t edge_weight;
}

enum ComputationPhase {
    WAIT_FOR_JOBS,
    HANDLE_JOBS,
    WAIT_FOR_PATH_RETRIEVAL,
    RETRIEVE_PATH,
    STOP_WORK, //TODO: will this be ever used?
}

const region_id_t NUM_PARTITIONS = 42;

class Edge {
public:
    dist_t weight_;
    std::shared_ptr<Vertex> end_;

    Edge(const dist_t weight, const std::shared_ptr<Vertex>& end);

    bool reaches_optimally(const region_id_t region) const;
private:
    std::bitset<NUM_PARTITIONS> region_mask;
};

class Vertex {
public:
    vertex_id_t id;
    region_id_t region_;
    std::vector<Edge> edges;
};

class ShortestPathsWorker {
public:
    ShortestPathsWorker(
        const std::shared_ptr<Channel>& channel,
        const std::shared_ptr<std::mutex>& pq_mutex,
        const std::shared_ptr<maxheap_t>& pq,
        const std::string& main_address,
        const std::string& own_address,
    );

    void run();
    void compute_shortest_paths();
private:
    void post_request_cleanup();
    void send_jobs_to_neighbors();
    dist_t get_distance(const Vertex& vertex);
    void send_partial_path_results(const region_id_t region, const path_t& path, const dist_t distance);
    void retrieve_path(const dist_t distance);
    void dijkstra_within_region();

    std::unique_ptr<ShortestPathsMainWorker::Stub> stub_;
    std::map<region_id_t, std::unique_ptr<ShortestPathsWorkerService::Stub>> neighbors;
    
    //TODO: are these needed?
    std::string address_;
    std::string main_address_,

    region_id_t region_;
    std::set<Vertex> my_vertices_;

    std::shared_ptr<ComputationPhase> phase_;
    std::shared_ptr<std::mutex> pq_mutex_;
    std::shared_ptr<std::condition_variable> pq_cond_;

    std::shared_ptr<maxheap_t>> pq_; //todo: fix type
    std::map<vertex_id_t, vertex_id_t> parents_;
    std::map<vertex_id_t, dist_t> distances_; // TODO: do we maybe want this to be the same for all of a region's workers?
    std::set<std::vector<ContinueJob> neighbors_jobs_;

    std::shared_ptr<std::atomic_int> if_get_path_then_vertex_id;
    std::shared_ptr<std::atomic_int> my_turn_in_end; // 0 if waiting for main, 1 if to get path, 2 if to clean
};

class ShortestPathsWorkerServer final : public ShortestPathsWorkerService::Service {
public:
    ShortestPathsWorkerServer(
        std::shared_ptr<std::mutex> pq_mutex,
        std::shared_ptr<> pq,
        std::string addr
    ): pq_mutex_(pq_mutex), pq_(pq), addr_(addr) {}

    Status new_region_entry(ServerContext *context, ServerReader<Entry> *read_stream, JobEnd *reply) {
        Entry entry;
        while (read_stream->Read(&entry)) {
            //...
        }
    }

    void RunServer() {
        std::string server_address(addr);

        grpc::EnableDefaultHealthCheckService(true);
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials()); //TODO: do we want authentication?
        builder.RegisterService(this);
        auto server = std::make_unique<Server>(builder.BuildAndStart());

        std::cout << "Server listening on " << server_address << std::endl;

        server->Wait();
    }

private:
    std::shared_ptr<std::mutex> pq_mutex_;
    std::shared_ptr<std::priority_queue<int>> pq_;
    std::string addr_;
};


int main(int argc, char **argv) {
    if (argc != 3) {
        std::cout << "Usage: ./worker main_server_address worker_address" << std::endl;
    }

    auto main_server_address = std::string("localhost:50051");
    auto worker_address = std::string(argv[2]);

    auto queue_mut = std::make_shared<std::mutex>();
    auto pq = std::make_shared<maxheap_t>();

    auto worker_client = ShortestPathsWorker(
        grpc::CreateChannel(main_server_location, grpc::InsecureChannelCredentials()), //TODO: do we want authentication?
        queue_mut, 
        pq, 
        main_server_address, 
        worker_address
    );

    auto worker_server = ShortestPathsWorkerServer(queue_mut, pq, worker_address);

    auto client_thread = std::thread(&ShortestPathsWorker::run, worker_client);
    worker_server.RunServer();

    return 0;
}
