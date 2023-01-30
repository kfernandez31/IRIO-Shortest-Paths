#include <optional>

#include <sw/redis++/redis++.h>

#include <grpcpp/support/client_interceptor.h>

#include "shortestpaths.grpc.pb.h"

class CachingInterceptor : public grpc::experimental::Interceptor {
public:
    CachingInterceptor(grpc::experimental::ClientRpcInfo* info);
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;
private:
    std::optional<dist_t> get_in_cache(vertex_id_t source, vertex_id_t destination);
    void set_in_cache(vertex_id_t source, vertex_id_t destination, dist_t distance);
    grpc::ClientContext context_;
    std::unique_ptr<shortestpaths::Router::Stub> stub_;
    std::unique_ptr<grpc::ClientReaderWriter<shortestpaths::ShortestPathRequest, shortestpaths::ShortestPathResponse>> stream_;
    std::unique_ptr<sw::redis::Redis> redis_client_;
    dist_t response_;
};

class CachingInterceptorFactory : public grpc::experimental::ClientInterceptorFactoryInterface {
public:
    grpc::experimental::Interceptor* CreateClientInterceptor(grpc::experimental::ClientRpcInfo* info) override;
};
