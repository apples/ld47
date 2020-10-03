#pragma once

#include <sushi/sushi.hpp>

sushi::mesh_group make_board_mesh(int rows, int cols, glm::vec2 wh, glm::vec2 uv1, glm::vec2 uv2) {
    sushi::mesh_group_builder g;

    g.enable(sushi::attrib_location::POSITION);
    g.enable(sushi::attrib_location::NORMAL);
    g.enable(sushi::attrib_location::TEXCOORD);

    g.mesh("board");

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto bl = glm::vec2{c, r} * wh;
            auto ur = bl + wh;

            auto vbl = g.vertex()
                .position({bl, 0})
                .texcoord(uv1)
                .normal({0, 0, 1})
                .get();

            auto vur = g.vertex()
                .position({ur, 0})
                .texcoord(uv2)
                .normal({0, 0, 1})
                .get();

            auto vbr = g.vertex()
                .position({ur.x, bl.y, 0})
                .texcoord({uv2.x, uv1.y})
                .normal({0, 0, 1})
                .get();

            auto vul = g.vertex()
                .position({bl.x, ur.y, 0})
                .texcoord({uv1.x, uv2.y})
                .normal({0, 0, 1})
                .get();
            
            g.tri(vbl, vul, vur);
            g.tri(vur, vbr, vbl);
        }
    }

    return g.get();
}
