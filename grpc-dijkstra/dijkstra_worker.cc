#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <bitset>
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "dijkstra.grpc.pb.h"

using namespace dijkstra;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;
using grpc::ClientWriter;

class Edge {
public:
    std::vector<bool> mask;
    std::shared_ptr<Vertex> destination;
    int64_t length;
    
};

class Vertex {
public:

    int id;
    int region;
    //todo compa
    std::vector<Edge> edges;
    bool operator() (Vertex x, Vertex y){
        return x.id < y.id;
    }
    bool operator() (Vertex x, int y){
        return x.id < y;
    }
    class intCmp
    {
    public:
        using is_transparent = std::true_type;
        bool operator()(const int *a, const Vertex &b) const
        {
            return *a < b.id;
        }
        bool operator()(const Vertex &a, const int *b) const
        {
            return a.id < *b;
        }
        bool operator()(const Vertex &a, const Vertex &b) const
        {
            return a.id < b.id;
        }
    };
};

using vertex_path_info_t = std::tuple<int64_t, std::shared_ptr<Vertex>, std::shared_ptr<Vertex>>;

class DijkstraWorker
{
public:
    DijkstraWorker(std::shared_ptr<Channel> channel,
                   std::shared_ptr<std::mutex> queue_mut,
                   std::shared_ptr<std::priority_queue<int>> dij_queue,
                   std::string main_address,
                   std::string self_address)
        : stub_(DijkstraMainWorker::NewStub(channel)), queue_mut(queue_mut), dij_queue(dij_queue)
    {
        ClientContext context;
        SectorReply sector;
        this->my_address.set_v1(self_address);
        Status status = stub_->hello_and_get_sector(&context, this->my_address, &sector);
        this->sector_num = sector.sector_num();
        this->load_map();
        this->clear_distances();
    }

    void clear_distances() {
        for (auto const& vert: this->my_vertexes) {
            this->dijkstra_res[vert] = std::make_pair(std::numeric_limits<int64_t>::max(), nullptr);
        }
#ifdef DEBUG
        if (!dij_queue->empty())
        {
            std::cerr<<"Priority Queue not empty after finishing request full\n";
        }
#endif
    }

    void load_map()
    {
        
    }

    void run(std::string address)
    {

        ClientContext context;
        Job job;
        std::unique_ptr<ClientReader<Job>> job_reader(stub_->job_stream(&context, this->my_address));
        while (job_reader->Read(&job))
        {
            if (job.type_of_job() == 0)
            {
                this->start_dijkstra(job.start_vertex(), job.end_vertex(), job.end_vertex_region(), job.job_id());
            }else
            {
                this->continue_dijkstra(job.end_vertex(), job.end_vertex_region(), job.job_id()); //@tODO
            }
        }
    }

    std::shared_ptr<Vertex> get_vertex_by_id(int vertex_id) {
        return *this->my_vertexes.find(vertex_id);
    }

    void start_dijkstra(int start_vertex_id, int end_vertex_id, int end_vertex_region, int job_id)
    {
        auto start_vertex = get_vertex_by_id(start_vertex_id);
        this->queue_mut->lock();
        this->dij_queue->emplace(0, start_vertex, nullptr);
        this->run_dijkstra();
        this->destination = end_vertex_id;
        this->destination_region = end_vertex_region;
        
    }
    
    void continue_dijkstra(int end_vertex_id, int end_vertex_region, int job_id) {
        this->destination = end_vertex_id;
        this->destination_region = end_vertex_region;
    }

    void enter_new_region(int64_t distance, std::shared_ptr<Vertex> vertex, std::shared_ptr<Vertex> father) {
        (*this->to_send)[vertex->region].emplace_back(distance, vertex, father);
    }

    void send_path(int region, std::vector<std::pair<int64_t, std::shared_ptr<Vertex>>> path, int distance) {
        ClientContext context;
        Ok ok;
        std::unique_ptr<ClientWriter<PathVert>> writer(this->neighbour_stubs[region]->send_path(&context, &ok));

        for (int i = 0; i < path.size();++i) {
            PathVert msg;
            msg.set_islastvert(i == path.size());
            msg.set_distance(path[i].first);
            msg.set_ver_id(path[i].second->id);
            writer->Write(msg);
        }
    }
    
    void send_path_back_to_main(std::vector<std::pair<int64_t, std::shared_ptr<Vertex>>>path) {
        ClientContext context;
        Ok ok;
        std::unique_ptr<ClientWriter<PathVert>> writer(this->stub_->path_found(&context, &ok));

        for (int i = 0; i < path.size();++i) {
            PathVert msg;
            msg.set_islastvert(i == path.size());
            msg.set_distance(path[i].first);
            msg.set_ver_id(path[i].second->id);
            writer->Write(msg);
        }
    }

    void retrieve_path(int64_t distance) {
        auto last_vertex = this->get_vertex_by_id(this->if_get_path_then_vertex_id->load());
        std::vector<std::shared_ptr<Vertex>>path{last_vertex};
        auto last_vertex_res = this->dijkstra_res[last_vertex].first;
        while(true){
                  auto father = this->dijkstra_res.find(last_vertex);
                  //may error check find end, nullptr
                  if(father->second.second == nullptr){
                    this->send_path_back_to_main(path);
                  }
                  //tochange_names
                  path.push_back(father->second.second);
                  if(father->second.second->region != this->sector_num){
                     this->send_path(father->second.second->region, path,distance);
                     break;
                  }
                  last_vertex = father->second.second; 
        }
    }
    
    void run_dijkstra() {
        while (true) {
            //raz serwer
            //raz client
            std::unique_lock<std::mutex> lock(*this->queue_mut);
                
            do {
                this->queue_cond->wait(lock);
            }while(this->my_turn==0); 
            
            if(this->end_of_job)
            {
                break;
            }
            while(!this->dij_queue->empty()) {
                auto next_v = this->dij_queue->top();
                this->dij_queue->pop();
                auto distance = std::get<0>(next_v);
                auto vert = std::get<1>(next_v);
                auto father = std::get<2>(next_v);
                
                if (vert->region != this->sector_num) {
                    this->enter_new_region(distance, vert, father);
                    continue;
                }
                
                auto curr = dijkstra_res[vert];
                auto curr_dist = curr.first;
                if (curr_dist > distance) {
                    dijkstra_res[vert] = std::make_pair(distance, father);
                    for (auto& edge: vert->edges) {
                        if (edge.mask[this->destination_region]) {
                            this->dij_queue->emplace(distance + edge.length, edge.destination, vert);
                        }
                    }
                }
            }
            this->my_turn = 0;
            this->queue_cond->notify_one();
            lock.unlock();

        }
        if(sector_num == destination_region)
        {
            this->if_get_path_then_vertex_id->store(this->destination);
            
            this->retrieve_path();
        }
        std::unique_lock<std::mutex> lock(*this->queue_mut);     
        while(true)
        {   
            do {
                this->queue_cond->wait(lock);
            }while(this->my_turn_in_end==0); 
            if(this->my_turn_in_end->load() == 1)
            {
                this->retrieve_path(0);
            }else
            {
                this->clear_distances();
                lock.unlock();
                break;
            }             
            this->my_turn_in_end = 0;
            this->queue_cond->notify_one();
            lock.unlock();
        }
    }

private:
    std::unique_ptr<DijkstraMainWorker::Stub> stub_;
    int32_t sector_num;
    Address my_address;
    std::shared_ptr<std::mutex> queue_mut;
    std::shared_ptr<std::condition_variable> queue_cond;
    std::shared_ptr<std::priority_queue<vertex_path_info_t>> dij_queue; // na razie jest max
    std::set<std::shared_ptr<Vertex>, Vertex::intCmp> my_vertexes;
    std::map<std::shared_ptr<Vertex>, std::pair<int64_t, std::shared_ptr<Vertex>>> dijkstra_res; 
    std::shared_ptr<std::map<int, std::vector<vertex_path_info_t>>> to_send;
        // to typedef int
    std::shared_ptr<std::atomic_bool> my_turn;
    std::shared_ptr<bool> end_of_job;
    int destination;
    int destination_region;
    std::shared_ptr<std::atomic_int> if_get_path_then_vertex_id;
    std::shared_ptr<std::atomic_int> my_turn_in_end; // 0 if waiting for main, 1 if to get path, 2 if to clean
    std::map<int, std::unique_ptr<DijkstraWorkerService::Stub>> neighbour_stubs;
};

class DijkstraWorkerServer final : public DijkstraWorkerService::Service
{
public:
    DijkstraWorkerServer(std::shared_ptr<std::mutex> queue_mut,
                         std::shared_ptr<std::priority_queue<int>> dij_queue,
                         std::string addr)
    {
        this->queue_mut = queue_mut;
        this->dij_queue = dij_queue;
        this->addr = addr;
    }

    Status new_region_entry(ServerContext *context, ServerReader<Entry> *read_stream, JobEnd *reply)
    {
        Entry entry;
        while (read_stream->Read(&entry))
        {
        }
    }

    void RunServer()
    {
        std::string server_address(this->addr);

        grpc::EnableDefaultHealthCheckService(true);
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *synchronous* service.
        builder.RegisterService(this);
        // Finally assemble the server.
        std::unique_ptr<Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;

        // Wait for the server to shutdown. Note that some other thread must be
        // responsible for shutting down the server for this call to ever return.
        server->Wait();
    }

private:
    std::shared_ptr<std::mutex> queue_mut;
    std::shared_ptr<std::priority_queue<int>> dij_queue;
    std::string addr;
};


int main(int argc, char **argv)
{
    std::string main_server_location;
    main_server_location = "localhost:50051";
    std::string my_location{argv[1]};
    std::shared_ptr<std::mutex> queue_mut{};
    std::shared_ptr<std::priority_queue<int>> dij_queue{};


    DijkstraWorker worker_client(
        grpc::CreateChannel(main_server_location, grpc::InsecureChannelCredentials()), queue_mut, dij_queue, main_server_location, my_location);

    DijkstraWorkerServer worker_server(queue_mut, dij_queue, my_location);

    std::thread client_thread(&DijkstraWorker::run, worker_client);
    worker_server.RunServer();

    return 0;
}
