#include "client.hh"

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cout << "Usage: ./" << argv[0] << "address source destination" << std::endl;
        exit(1);
    }

    auto main_server_address = std::string(argv[1]);
    auto source = std::stoi(argv[2]);
    auto destination = std::stoi(argv[3]);

    auto channel = grpc::CreateChannel(main_server_address, grpc::InsecureChannelCredentials());
    auto stub = shortestpaths::ShortestPathsMainService::NewStub(channel);
    auto client = Client(stub, channel);

    auto result = client.get_shortest_path();
    std::cout << "Shortest path: " << result << std::endl;

    return 0;
}