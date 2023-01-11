#include <sw/redis++/redis++.h>
#include <thread>
#include <chrono>
#include <iostream>

using namespace sw::redis;

/*
g++ -std=c++20 -o redis_example redis_example.cpp ~/repos/redis-plus-plus/build/libredis++.a ~/repos/hiredis/libhiredis.a -pthread
*/

static inline std::string to_redis_key(int src, int dst) {
    return std::to_string(std::min(src, dst)) + "-" + std::to_string(std::max(src, dst));
}

int dijkstra(int src, int dst) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    return 69;
}

int main() {
    ConnectionOptions opts;
    opts.host = "127.0.0.1";
    opts.port = 7777;
    auto redis = Redis(opts);

    int src = 21, dst = 37;
    redis.set(to_redis_key(src, dst), std::to_string(42));
    auto val = redis.get(to_redis_key(src, dst));
    if (val) {
        std::cout << "Computed distance: " << *val << "\n";
    } else {
        std::cout << "Computed distance: " << dijkstra(src, dst) << "\n";
    }
}
