#include "caching_interceptor.hh"

using namespace sw::redis;
using namespace grpc::experimental;
using namespace shortestpaths;

const std::string redis_host = "127.0.0.1";
const int redis_port = 7777;

static inline std::string to_redis_key(vertex_id_t src, vertex_id_t dst) {
    return std::to_string(src) + "-" + std::to_string(dst);
}

static inline std::string to_redis_val(dist_t dist) {
    return std::to_string(dist);
}

std::optional<int> CachingInterceptor::get_in_cache(vertex_id_t src, vertex_id_t dst) {
    auto val = redis_client_->get(to_redis_key(src, dst));
    if (!val) {
        return std::nullopt;
    } else {
        return std::stoi(*val);
    }
}

void CachingInterceptor::set_in_cache(vertex_id_t source, vertex_id_t destination, dist_t distance) {
    redis_client_->set(to_redis_key(source, destination), to_redis_val(distance));
}

CachingInterceptor::CachingInterceptor(ClientRpcInfo* info) {
    sw::redis::ConnectionOptions opts;
    opts.host = redis_host;
    opts.port = redis_port;
    redis_client_ = std::make_unique<Redis>(opts);
}

void CachingInterceptor::Intercept(InterceptorBatchMethods* methods){
    bool hijack = false;

    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
        // Hijack all calls
        hijack = true;
        // Create a stream on which this interceptor can make requests
        stub_ = Router::NewStub(methods->GetInterceptedChannel());
        stream_ = stub_->GetShortestPaths(&context_);
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
                grpc::SerializationTraits<ShortestPathRequest>::Deserialize(&copied_buffer, &req_msg).ok()
            );
            source = req_msg.source();
            destination = req_msg.destination();
        }

        // Check if the key is present in the map
        auto search = get_in_cache(source, destination);
        if (search) {
            response_ = *search;
        } else {
            // Key was not found in the cache, so make a request
            ShortestPathRequest req;
            req.set_source(source);
            req.set_destination(destination);
            stream_->Write(req);
            ShortestPathResponse resp;
            stream_->Read(&resp);
            response_ = resp.distance();
            // Insert the pair in the cache for future requests
            set_in_cache(source, destination, response_);
        }
    }
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_SEND_CLOSE)) {
        stream_->WritesDone();
    }
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_RECV_MESSAGE)) {
        ShortestPathResponse* resp = static_cast<ShortestPathResponse*>(methods->GetRecvMessage());
        resp->set_distance(response_);
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

Interceptor* CachingInterceptorFactory::CreateClientInterceptor(ClientRpcInfo* info) {
    return new CachingInterceptor(info);
}
