#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"

#include "common.hh"

#include <map>

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


class ShortestPathsWorkerServer final : public ShortestPathsWorkerService::Service {
public:
    ShortestPathsWorkerServer(
        std::shared_ptr<std::mutex> pq_mutex,
        std::shared_ptr<maxheap_t> pq,
        std::shared_ptr<WorkerComputationPhase> phase,
        std::string addr
    ): phase_mutex_(pq_mutex), pq_(pq), addr_(addr), phase_(phase){}

    Status send_jobs_to_neighbors(ServerContext *context, const ContinueJobs *continue_jobs, Ok *ok_reply);

    Status send_path(ServerContext *context, const ServerReader<PathVert> *path_stream, Ok *ok_reply);

    Status begin_new_query(ServerContext *context, const NewJob *new_job, Ok *ok_reply);

    Status begin_next_round(ServerContext *context, const Ok *next_round_signal, Ok *ok_reply);
    // Status hello_and_get_region(ServerContext *context, const HelloRequest *read_stream, RegionReply *reply);

    // Status request_job(ServerContext *context, const Address *read_stream, NewJob *reply);

    // Status path_found(ServerContext *context, const ServerReader<PathVert> *read_stream, Ok *reply);

    // Status end_of_local(ServerContext *context,const LocalPhaseEnd *read_stream, Ok *reply);

    void run();

private:
    std::shared_ptr<std::mutex> phase_mutex_;
    std::shared_ptr<maxheap_t> pq_;
    std::string addr_;
    std::shared_ptr<WorkerComputationPhase> phase_;
    std::map<region_id_t, bool> received_jobs_from_neighbours_;
    size_t received_jobs_from_neighbours_counter_;
    size_t neighbours_number_;
};