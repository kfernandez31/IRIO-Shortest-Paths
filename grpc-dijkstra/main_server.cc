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
    std::cout << "BEFORE MUT" << std::endl;
    std::unique_lock<std::mutex> lock(*mutex_);
    std::cout << "AFTER MUT" << std::endl;
    queries_->push(*client_query);
    std::cout << "PUSHED QUERY TO QUERY" << std::endl;
    
    if (*phase_ == MainComputationPhase::WAIT_FOR_QUERY) {
        *phase_ = MainComputationPhase::SEND_QUERY;
        current_query_ = std::make_shared<ClientQuery>(queries_->front());
        queries_->pop();
    }
    std::cout << "GOT NEW CLIENT QUERY" <<std::endl;
    return Status::OK;
}
                                                                            // dostajemy adres klienta            odsyłamy numer regionu
Status ShortestPathsMainServer::hello_and_get_region(ServerContext *context, const HelloRequest *hello_request, RegionReply *region_reply) {
    region_id_t region = worker_stubs_->size();
    region_reply->set_region_num(region);

    auto channel = grpc::CreateChannel(hello_request->addr(), grpc::InsecureChannelCredentials());
    auto stub = shortestpaths::ShortestPathsWorkerService::NewStub(channel);
    (*worker_stubs_)[region] = std::make_pair(std::move(stub), hello_request->addr());
    if(worker_stubs_->size() == *region_number_ )
    {
        *phase_ = MainComputationPhase::WAIT_FOR_QUERY;
        std::cout << "WE HAVE FULL CREW LETS WAIT FOR QUERY"<<std::endl;
    }

    std::cout << "ASSIGNED REGION " <<region << " TO :" << hello_request->addr() << std::endl;

    return Status::OK;
}


Status ShortestPathsMainServer::path_found(ServerContext *context, ServerReader<ContinueJobs> *HelloRequest, Ok *ok_reply) {
   return Status::OK;
}

// //Status ShortestPathsMainServer::request_neighbour_region_addr() {

// }

Status ShortestPathsMainServer::end_of_local_phase(ServerContext *context, const LocalPhaseEnd *local_phase_request, Ok *ok_reply) {
    std::cout<<"End of local dijkstra phase for one region" <<std::endl;
    
    *ended_phase_counter_ += 1;
    *anything_to_send_ |= local_phase_request->anything_to_send();
    std::cout<<local_phase_request->anything_to_send()<<std::endl;
    std::cout<<*ended_phase_counter_ << "  " << *region_number_<<std::endl;
    if (*ended_phase_counter_ == *region_number_) {
        *ended_phase_counter_ = 0;
       
        std::unique_lock<std::mutex> lock(*mutex_);
        
        std::cout << "GOT MUTEX END OF LOCAL PHASE" << std::endl;
        

        if (!*anything_to_send_) {
            std::cout << "Optimal solution found, beginning retrieving" << std::endl;
            *phase_ = MainComputationPhase::RETRIEVE_PATH_MAIN;
        }
        else {
            *phase_ = MainComputationPhase::EXCHANGE_PHASE_MAIN;
        }
        *anything_to_send_ = false;
    }

    return Status::OK;
}


Status ShortestPathsMainServer::end_of_exchange_phase(ServerContext *context, const ExchangePhaseEnd *exchange_phase_end, Ok *ok_reply) {
    *ended_phase_counter_ += 1;

    if (ended_phase_counter_ == region_number_) {
        ended_phase_counter_ = 0;
       
        std::unique_lock<std::mutex> lock(*mutex_);
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
    std::vector<std::pair<dist_t, vertex_id_t>> retrieved_path;
    retrieved_path.emplace_back(current_vertex, 0);
    auto total_distance = 0;
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
            total_distance += next_distance;
            retrieved_path.emplace_back(next_vertex,next_distance);
        }
    }

    std::cout << "WHOLE PATH RETRIEVED " << total_distance << std::endl;
    for (auto const v: retrieved_path) {
        std::cout << v.first << " " << v.second << std::endl;
    }

 
    
    for(const auto& worker: *worker_stubs_){
        ClientContext context;
        Ok ok,ok2;
        worker.second.first->end_of_query(&context, ok, &ok2);
    }

    if (queries_->empty()){
        *phase_ = MainComputationPhase::WAIT_FOR_QUERY;
    }
    else {
        *phase_ = MainComputationPhase::SEND_QUERY;
        current_query_ = std::make_shared<ClientQuery>(queries_->front());
        queries_->pop();
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------
void ShortestPathsMainServer::run() {
    bool end = false;
    auto border_info = load_region_borders();
    while (!end)
    {
        std::unique_lock<std::mutex> lock(*mutex_); //@todo 

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
                        auto info = new_job.add_neighbours();
                        info->set_address((*worker_stubs_)[border].second);
                        info->set_region_number(border);
                    }
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
            case MainComputationPhase::WAIT_FOR_PATH_RETRIEVAL_MAIN: //todo
                break;
            case MainComputationPhase::BROADCAST_END_OF_QUERY: //todo
                break;
        }
        lock.unlock();
        // may need to free lock if RAII does not
    }
}
