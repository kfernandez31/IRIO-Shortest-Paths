#include <optional>

#include <sw/redis++/redis++.h>

#include <grpcpp/support/client_interceptor.h>

#include "shortestpaths.grpc.pb.h"

using namespace grpc::experimental;
using namespace shortestpaths;

const std::string redis_host = "127.0.0.1";
const int redis_port = 7777;

static inline std::string to_redis_key(int src, int dst) {
    return std::to_string(std::min(src, dst)) + "-" + std::to_string(std::max(src, dst));
}

std::optional<int> CachingInterceptor::get_in_cache(int src, int dst) {
    auto val = redis_client_.get(to_redis_key(src, dst);
    return !val? {} : std::stoi(*val));
}

void CachingInterceptor::set_in_cache(int source, int destination, int distance) {
    redis_client_.set(to_redis_key(source, destination), distance);
}

CachingInterceptor::CachingInterceptor(ClientRpcInfo* info) {
    ConnectionOptions opts;
    opts.host = redis_host;
    opts.port = redis_port;
    redis_client_ = sw::redis::Redis(opts);
}

void CachingInterceptor::Intercept(InterceptorBatchMethods* methods) override {
    bool hijack = false;

    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
        // Hijack all calls
        hijack = true;
        // Create a stream on which this interceptor can make requests
        stub_ = Router::NewStub(methods->GetInterceptedChannel());
        stream_ = stub_->GetValues(&context_);
    }
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_SEND_MESSAGE)) {
        // We know that clients perform a Read and a Write in a loop, so we don't
        // need to maintain a list of the responses.
        int source, destination;
        const ShortestPathRequest* req_msg = static_cast<const ShortestPathRequest*>(methods->GetSendMessage());
        if (req_msg != nullptr) {
            source = req_msg->source();
            destination = req_msg->destination();
        } else {
            // The non-serialized form would not be available in certain scenarios,
            // so add a fallback
            ShortestPathRequest req_msg;
            auto* buffer = methods->GetSerializedSendMessage();
            auto copied_buffer = *buffer;
            GPR_ASSERT(
                grpc::SerializationTraits<ShortestPathRequest>::Deserialize(
                    &copied_buffer, &req_msg)
                    .ok());
            source = req_msg->source();
            destination = req_msg->destination();
        }

        // Check if the key is present in the map
        auto search = get_in_cache(source, destination);
        if (search) {
            std::cout << "Key " << "(" << source << ", " << destination << ") found in cache";
            response_ = *search;
        } else {
            std::cout << "Key " << "(" << source << ", " << destination << ") not found in cache";
            // Key was not found in the cache, so make a request
            ShortestPathRequest req;
            req.set_source(source);
            req.set_source(destination);
            stream_->Write(req);
            ShortestPathResponse resp;
            stream_->Read(&resp);
            // Insert the pair in the cache for future requests
            insert_to_cache(source, destination, resp.distance());
        }
    }
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_SEND_CLOSE)) {
        stream_->WritesDone();
    }
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_RECV_MESSAGE)) {
        ShortestPathResponse* resp =
            static_cast<ShortestPathResponse*>(methods->GetRecvMessage());
        resp->set_value(response_);
    }
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_RECV_STATUS)) {
        auto* status = methods->GetRecvStatus();
        *status = grpc::Status::OK;
    }
    // One of Hijack or Proceed always needs to be called to make progress.
    if (hijack) {
        // Hijack is called only once when PRE_SEND_INITIAL_METADATA is present in
        // the hook points
        methods->Hijack();
    } else {
        // Proceed is an indicator that the interceptor is done intercepting the
        // batch.
        methods->Proceed();
    }
}

Interceptor* CachingInterceptorFactory::CreateClientInterceptor(ClientRpcInfo* info) override {
    return new CachingInterceptor(info);
}
