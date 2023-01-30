#pragma once

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
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <tuple>
#include <bitset>

#include "caching_interceptor.hh"

#include <grpcpp/grpcpp.h>

#include "shortestpaths.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using shortestpaths::Router;
using shortestpaths::ShortestPathRequest;
using shortestpaths::ShortestPathResponse;

class ShortestPathClient {
public:
    ShortestPathClient(std::shared_ptr<Channel> channel);
    dist_t get_shortest_path(const vertex_id_t start, const vertex_id_t end) {
private:
    std::unique_ptr<ShortestPathsMainService::Stub> stub_;
    region_id_t get_region(const vertex_id_t vertex);
};
