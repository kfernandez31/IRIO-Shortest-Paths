
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
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
using grpc::Status;
using grpc::Server;
using grpc::ServerReader;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class DijkstraWorker 
{
public:
    DijkstraWorker(std::shared_ptr<Channel> channel, 
                   std::shared_ptr<std::mutex> queue_mut, 
                   std::shared_ptr<std::priority_queue<int>> dij_queue,
                   std::string main_address)
                : stub_(DijkstraMainWorker::NewStub(channel)), queue_mut(queue_mut), dij_queue(dij_queue)
    {
        
        //SectorReply sector = stub_->hello_and_get_sector(address));
        //sector_num = sector;
    }


    void run(std::string address)
    {
        
        ClientContext context;
        Job job;
        std::unique_ptr<ClientReader<Job>> job_reader(stub_->job_stream(&context));
        while(job_reader->Read(&job, address))
        {
            if job.type_of_job == 0 {
                this.start_dijkstra(job.start_edge, job.end_edge, job.end_edge_region);
            }
        }
    }
    void start_dijkstra(int start_edge, int end_edge, int ind_edge_region){

    }

private:
    std::unique_ptr<DijkstraMainWorker::Stub> stub_;
    int32_t sector_num;
    std::shared_ptr<std::mutex> queue_mut;
    std::shared_ptr<std::priority_queue<int>> dij_queue;
};



class DijkstraWorkerServer final: public DijkstraWorkerService::Service {
public:
    DijkstraWorkerServer(std::shared_ptr<std::mutex> queue_mut, 
                        std::shared_ptr<std::priority_queue<int>> dij_queue,
                        std::string addr) {
        this->queue_mut = queue_mut;
        this->dij_queue = dij_queue;
        this->addr = addr;
    }

    Status new_region_entry(ServerContext* context, ServerReader<Entry>* read_stream, JobEnd* reply) {
        Entry entry;
        while(read_stream->Read(&entry)){
            
        }
    }
    
    void RunServer() {
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
        grpc::CreateChannel(main_server_location, grpc::InsecureChannelCredentials()), queue_mut, dij_queue, main_server_location);
    
    DijkstraWorkerServer worker_server(queue_mut, dij_queue, my_location);
    
    std::thread client_thread(&DijkstraWorker::run, worker_client);
    worker_server.RunServer();
        
    return 0;
}
