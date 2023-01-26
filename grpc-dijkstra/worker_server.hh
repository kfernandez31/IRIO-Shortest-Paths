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
        std::shared_ptr<WorkerState> worker_state,
        std::string addr
    ): worker_state_(worker_state), addr_(addr){}

    Status send_jobs_to_neighbors(ServerContext *context, const ContinueJobs *continue_jobs, Ok *ok_reply);

    Status send_path(ServerContext *context, const RetVertex *ret_vertex, Path *path_reply);

    Status begin_new_query(ServerContext *context, const NewJob *new_job, Ok *ok_reply);

    Status begin_next_round(ServerContext *context, const Ok *next_round_signal, Ok *ok_reply);
    
    Status end_of_query(ServerContext *context, const Ok *end_of_query_signal, Ok *ok_reply);

    void run();

private:
    std::shared_ptr<WorkerState> worker_state_;
    std::string addr_;
    std::map<region_id_t, bool> received_jobs_from_neighbours_;
    size_t received_jobs_from_neighbours_counter_;
    size_t neighbours_number_;
};