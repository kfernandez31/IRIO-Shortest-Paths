
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "dijkstra.grpc.pb.h"

using dijkstra::EdgeLen;
using dijkstra::LenQuery;
using dijkstra::LenReply;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

class DijkstraWorker
{
public:
    DijkstraWorker(std::shared_ptr<Channel> channel)
        : stub_(DijkstraMainWorker::NewStub(channel))
    {
        SectorReply sector = stub_->hello_and_get_sector(address));
        this.sector_num = sector;
    }
    void run(std::string address)
    {
        ClientContext context;
        Job job;
        std::unique_ptr<ClientReader<Job>> job_reader(stub_->job_stream(&context));
        while(job_reader->Read(&job, address))
        {
            if job.type_of_job == 0 {
                this.run_dijkstra(job.start_edge, job.end_edge, job.end_edge_region);
            }
        }
    }
    void run_dijkstra(int start_edge, int end_edge, int ind_edge_region){

    }

private:
    std::unique_ptr<EdgeLen::Stub> stub_;
    int32_t sector_num;
    priority_queue<>
};

int main(int argc, char **argv)
{
    std::string main_server_location;
    main_server_location = "localhost:50051";

    std::string my_location{argv[1]};
    DijkstraClient client(
        grpc::CreateChannel(main_server_location, grpc::InsecureChannelCredentials()));

    std::thread client_thread(client.run, my_location);
    return 0;
}
