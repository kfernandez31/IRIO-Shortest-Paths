#include "common.hh"
#include <memory>
#include <map>
#include <iostream>

std::map<vertex_id_t, std::shared_ptr<Vertex>> load_graph(region_id_t region_num)
{
    if (region_num == 0)
    {
        auto v0 = std::make_shared<Vertex>(0, 0, std::vector<Edge>{});
        auto v1 = std::make_shared<Vertex>(1, 0, std::vector<Edge>{});
        auto v2 = std::make_shared<Vertex>(2, 0, std::vector<Edge>{});
        auto e1 = Edge(1, 1, 0, {1, 1});
        auto e2 = Edge(10, 2, 0, {1, 1});
        auto e3 = Edge(10, 3, 1, {1, 1});
        auto e4 = Edge(2137, 5, 1, {1, 1});
        
        v0->edges.push_back(e1);
        v1->edges.push_back(e2);
        v2->edges.push_back(e3);
        v2->edges.push_back(e4);

        std::map<vertex_id_t, std::shared_ptr<Vertex>> m;
        m[0] = v0;
        m[1] = v1;
        m[2] = v2;

        return std::move(m);
    }
    else
    {
        auto v3 = std::make_shared<Vertex>(3, 1, std::vector<Edge>{});
        auto v4 = std::make_shared<Vertex>(4, 1, std::vector<Edge>{});
        auto v5 = std::make_shared<Vertex>(5, 1, std::vector<Edge>{});
        auto e5 = Edge(10, 4, 1, {1, 1});
        auto e6 = Edge(1, 1, 0,{1,1});
        v3->edges.push_back(e5);
        v3->edges.push_back(e6);
        std::map<vertex_id_t, std::shared_ptr<Vertex>> m;
        m[3] = v3;
        m[4] = v4;
        m[5] = v5;
        return std::move(m);
    }
}

std::map<region_id_t, std::vector<region_id_t>> load_region_borders()
{
    auto m = std::map<region_id_t, std::vector<region_id_t>>();
    m[0] = std::vector<region_id_t>();
    m[0].push_back(1);
    m[1] = std::vector<region_id_t>();
    m[1].push_back(0);
    return m;
}