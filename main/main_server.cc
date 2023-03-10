#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpc/grpc.h>
#include "shortestpaths.grpc.pb.h"

#include "main_server.hh"
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

Status ShortestPathsMainServer::client_query(ServerContext *context, const ClientQuery *client_query, Ok *ok_reply) {
    std::unique_lock<std::mutex> lock(*mutex_);
    queries_->push(*client_query);
    
    if (*phase_ == MainComputationPhase::WAIT_FOR_QUERY) {
        this->check_queries();
    }
    std::cout << "GOT NEW CLIENT QUERY" <<std::endl;
    return Status::OK;
}
                                                                            // dostajemy adres klienta            odsyłamy numer regionu
Status ShortestPathsMainServer::hello_and_get_region(ServerContext *context, const HelloRequest *hello_request, RegionReply *region_reply) {
    std::unique_lock<std::mutex> lock(*mutex_);
    region_id_t region = worker_stubs_->size();
    region_reply->set_region_num(region);

    auto channel = grpc::CreateChannel(hello_request->addr(), grpc::InsecureChannelCredentials());
    auto stub = shortestpaths::ShortestPathsWorkerService::NewStub(channel);
    std::cout << hello_request->addr() << std::endl;
    (*worker_stubs_)[region] = std::make_pair(std::move(stub), hello_request->addr());
    if(worker_stubs_->size() == *region_number_ )
    {
        this->check_queries();
        std::cout << "WE HAVE FULL CREW LETS WAIT FOR QUERY"<<std::endl;
    }

    std::cout << "ASSIGNED REGION " <<region << " TO :" << hello_request->addr() << std::endl;

    return Status::OK;
}

void ShortestPathsMainServer::check_queries()
{    
    if (queries_->empty()){
        *phase_ = MainComputationPhase::WAIT_FOR_QUERY;
    }
    else {
        *phase_ = MainComputationPhase::SEND_QUERY;
        (*notification_counter_)++;
        cond_->notify_one();
        current_query_ = std::make_shared<ClientQuery>(queries_->front());
        queries_->pop();
    }
}


Status ShortestPathsMainServer::end_of_local_phase(ServerContext *context, const LocalPhaseEnd *local_phase_request, Ok *ok_reply) {
    std::unique_lock<std::mutex> lock(*mutex_);
    *ended_phase_counter_ += 1;
    *anything_to_send_ |= local_phase_request->anything_to_send();
    std::cout<<local_phase_request->anything_to_send()<<std::endl;
    std::cout<<*ended_phase_counter_ << "  " << *region_number_ << " " << context->peer() <<std::endl;
    if (*ended_phase_counter_ == *region_number_) {
        *ended_phase_counter_ = 0;   
        if (!*anything_to_send_) {
            std::cout << "OPTIMAL SOLUTION FOUND" << std::endl;
            *phase_ = MainComputationPhase::RETRIEVE_PATH_MAIN;
            (*notification_counter_)++;
            cond_->notify_one();
        }
        else {
            *phase_ = MainComputationPhase::EXCHANGE_PHASE_MAIN;
            (*notification_counter_)++;
            cond_->notify_one();
        }
        *anything_to_send_ = false;
    }

    return Status::OK;
}


Status ShortestPathsMainServer::end_of_exchange_phase(ServerContext *context, const ExchangePhaseEnd *exchange_phase_end, Ok *ok_reply) {
    std::unique_lock<std::mutex> lock(*mutex_);
    
    *ended_phase_counter_ += 1;

    if (*ended_phase_counter_ == *region_number_) {
        ended_phase_counter_ = 0;
        ClientContext context;
        Ok ok,ok2;
        for(const auto& worker: *worker_stubs_){
            worker.second.first->begin_next_round(&context, ok, &ok2);
        }
        *phase_ = MainComputationPhase::WAIT_FOR_EXCHANGE;
    }

    return Status::OK;
}

void ShortestPathsMainServer::retrieve_path_main(){
    auto current_region = current_query_->end_vertex_region();
    auto current_vertex = current_query_->end_vertex();
    auto client_address = current_query_->client_address();
    std::vector<std::pair<dist_t, vertex_id_t>> retrieved_path;
    retrieved_path.emplace_back(current_vertex, 0);
    auto end = false;
    while (!end) {
        ClientContext context;
        RetVertex ret_vertex;
        Path path;
        ret_vertex.set_vertex_id(current_vertex);
        (*worker_stubs_)[current_region].first->send_path(&context, ret_vertex, &path);

        for (auto path_vert : path.path_vericies()) {
            auto next_region = path_vert.next_region_id();
            auto next_vertex = path_vert.vertex();
            auto next_distance = path_vert.distance();

            if (next_vertex == current_vertex) {
                end = true;
            }

            current_region = next_region;
            current_vertex = next_vertex;
            retrieved_path.emplace_back(next_vertex,next_distance);
        }
    }

    auto channel = grpc::CreateChannel(client_address, grpc::InsecureChannelCredentials());
    auto stub = shortestpaths::ClientService::NewStub(channel);

    ResultVector result_vector;
    result_vector.set_client_number(current_query_->client_number());
    result_vector.set_total_distance(retrieved_path[1].second);
    for (size_t i=0;i<retrieved_path.size()-1;++i) {
        ResultVertex* res_vertex = result_vector.add_verticies();
        res_vertex->set_vertex_id(retrieved_path[i].first);
        res_vertex->set_distance(retrieved_path[i+1].second);
    }

    ClientContext context;
    Ok ok;
    stub->send_result(&context, result_vector, &ok);


    std::cout << "WHOLE PATH RETRIEVED LENGTH: " << retrieved_path[1].second << std::endl;
    for (auto const v: retrieved_path) {
        std::cout << v.first << " " << v.second << std::endl;
    }
    for(const auto& worker: *worker_stubs_){
        ClientContext context;
        Ok ok,ok2;
        worker.second.first->end_of_query(&context, ok, &ok2);
    }
    this->check_queries();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
void ShortestPathsMainServer::run(std::string db_address) {
    bool end = false;
    auto border_info = load_region_borders(db_address);
    while (!end)
    {
        std::unique_lock<std::mutex> lock(*mutex_);
        while (*notification_counter_ < 1) {
            cond_->wait(lock);
        }
        *notification_counter_=0;
        switch (*phase_)
        {
            case MainComputationPhase::STARTUP:
                break;
            case MainComputationPhase::WAIT_FOR_QUERY:
                break;
            case MainComputationPhase::SEND_QUERY:
                for(const auto& worker: *worker_stubs_){
                    ClientContext context;
                    Ok ok;
                    auto start_vertex = current_query_->start_vertex();
                    auto start_vertex_region = current_query_->start_vertex_region();
                    auto end_vertex = current_query_->end_vertex();
                    auto end_vertex_region = current_query_->end_vertex_region();
                    NewJob new_job;
                    new_job.set_start_vertex(start_vertex);
                    new_job.set_end_vertex(end_vertex);
                    new_job.set_end_vertex_region(end_vertex_region);
                    new_job.set_is_first(start_vertex_region == worker.first);
                    for (auto border : border_info[worker.first]) {
                        std::cout << "ADDING BORDER BETWEEN REGIONS: " << worker.first << " " << border << std::endl;
                        auto info = new_job.add_neighbours();
                        info->set_address((*worker_stubs_)[border].second);
                        info->set_region_number(border);
                    }
                    std::cout << "ADDED ALL BORDERS FOR REGION: " << worker.first << std::endl;
                    worker.second.first->begin_new_query(&context, new_job, &ok);
                }
                *phase_ = MainComputationPhase::WAIT_FOR_EXCHANGE;
                break;
            case MainComputationPhase::WAIT_FOR_EXCHANGE:
                break;
            case MainComputationPhase::EXCHANGE_PHASE_MAIN:
                for(const auto& worker: *worker_stubs_){
                    ClientContext context;
                    Ok ok, ok2;
                    worker.second.first->begin_next_round(&context, ok, &ok2);
                }
                *phase_ = MainComputationPhase::WAIT_FOR_EXCHANGE;
                break;
            case MainComputationPhase::RETRIEVE_PATH_MAIN:
                this->retrieve_path_main();
                break;
        }
    }
}
