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
#include <condition_variable>
#include <memory>
#include <queue>

using namespace shortestpaths;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::Status;

class ShortestPathsMainServer final : public ShortestPathsMainService::Service
{
public:
    ShortestPathsMainServer(size_t region_number) {
        mutex_ = std::make_shared<std::mutex>();
        cond_ = std::make_shared<std::condition_variable>(); 
        phase_ = std::make_shared<MainComputationPhase>(MainComputationPhase::STARTUP);
        queries_ = std::make_shared<std::queue<ClientQuery>>();
        worker_stubs_ = std::make_shared<std::map<region_id_t, std::pair<std::unique_ptr<ShortestPathsWorkerService::Stub>, std::string>>>();
        region_number_ = std::make_shared<size_t>(region_number);
        ended_phase_counter_ = std::make_shared<size_t>(0);
        anything_to_send_ = std::make_shared<bool>(false);
        notification_counter_ = std::make_shared<size_t>(0);
    }

    Status client_query(ServerContext *context, const ClientQuery *client_query, Ok *ok_reply);
    Status hello_and_get_region(ServerContext *context, const HelloRequest *hello_request, RegionReply *ok_reply);
    Status path_found(ServerContext *context, ServerReader<ContinueJobs> *HelloRequest, Ok *ok_reply);
    Status end_of_local_phase(ServerContext *context, const LocalPhaseEnd *HelloRequest, Ok *ok_reply);
    Status end_of_exchange_phase(ServerContext *context, const ExchangePhaseEnd *HelloRequest, Ok *ok_reply);

    void check_queries();
    void run();
    void retrieve_path_main();
private:
    std::shared_ptr<std::mutex> mutex_;
    std::shared_ptr<std::condition_variable> cond_;
    std::shared_ptr<size_t> notification_counter_;
    std::shared_ptr<MainComputationPhase> phase_;
    std::shared_ptr<std::queue<ClientQuery>> queries_;
    std::shared_ptr<std::map<region_id_t, std::pair<std::unique_ptr<ShortestPathsWorkerService::Stub>, std::string>>> worker_stubs_; 
    std::shared_ptr<size_t> region_number_;
    std::shared_ptr<size_t> ended_phase_counter_;
    std::shared_ptr<bool> anything_to_send_;
    std::shared_ptr<ClientQuery> current_query_;
};