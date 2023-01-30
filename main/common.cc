#include "common.hh"
#include <memory>
#include <map>
#include <iostream>

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

std::map<vertex_id_t, std::shared_ptr<Vertex>> load_graph(std::string addr, region_id_t region_num)
{
    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    auto stub = shortestpaths::DBConnector::NewStub(channel);
    ClientContext client_context;
    RegionId reg_id;
    reg_id.set_region_id(region_num);
    RegionInfo regios_info;
    stub->get_region_info(&client_context, reg_id, &regios_info);
    std::map<vertex_id_t, std::shared_ptr<Vertex>> ret_map;
    for (auto const &vertex : regios_info.verticies())
    {
        auto v = vertex.vertex_id();
        ret_map[v] = std::make_shared<Vertex>(v, region_num, std::vector<Edge>{});
    }
    for (auto const &edge : regios_info.edges())
    {
        auto v_s = edge.start_vertex_id();
        auto v_e = edge.end_vertex_id();
        auto e_len = edge.weight();
        auto v_e_reg = edge.end_vertex_region();
        std::vector<bool> mask{};

        for (auto const &mas : edge.mask())
        {
            mask.push_back(mas.is_optimal());
        }
        auto e = Edge(e_len, v_e, v_e_reg, mask);
        ret_map[v_s]->edges.push_back(e);
    }

    return ret_map;
}

std::map<region_id_t, std::vector<region_id_t>> load_region_borders(std::string addr)
{
    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    auto stub = shortestpaths::DBConnector::NewStub(channel);
    ClientContext client_context;
    Ok ok;
    RegionIds region_borders;

    stub->get_region_neighbours(&client_context, ok, &region_borders);

    auto m = std::map<region_id_t, std::vector<region_id_t>>();

    for (auto const &border : region_borders.region_border())
    {
        auto r1 = border.region_id1();
        auto r2 = border.region_id2();
        if (m.find(r1) == m.end())
        {
            m[r1] = std::vector<region_id_t>();
        }

        m[r1].push_back(r2);
    }

    return m;
}