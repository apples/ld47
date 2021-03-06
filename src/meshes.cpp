#include "meshes.hpp"

auto get_sprite_mesh() -> sushi::mesh_group {
    sushi::mesh_group_builder mb;
    mb.enable(sushi::attrib_location::POSITION);
    mb.enable(sushi::attrib_location::TEXCOORD);
    mb.enable(sushi::attrib_location::NORMAL);

    mb.mesh("sprite");

    auto left = 0.f;
    auto right = 1.f;
    auto bottom = 0.f;
    auto top = 1.f;

    auto bottomleft = mb.vertex().position({left, bottom, 0}).texcoord({0, 1}).normal({0, 1, 0}).get();
    auto topleft = mb.vertex().position({left, top, 0}).texcoord({0, 0}).normal({0, 1, 0}).get();
    auto bottomright = mb.vertex().position({right, bottom, 0}).texcoord({1, 1}).normal({0, 1, 0}).get();
    auto topright = mb.vertex().position({right, top, 0}).texcoord({1, 0}).normal({0, 1, 0}).get();

    mb.tri(bottomleft, topleft, topright);
    mb.tri(topright, bottomright, bottomleft);

    return mb.get();
}
