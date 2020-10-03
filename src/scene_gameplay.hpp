#pragma once

#include "ember/box2d_helpers.hpp"
#include "ember/camera.hpp"
#include "ember/entities.hpp"
#include "ember/scene.hpp"

#include <sushi/sushi.hpp>
#include <sol.hpp>

#include <cmath>
#include <memory>
#include <tuple>
#include <vector>
#include <optional>

struct board_tile {
    int r = 0;
    int c = 0;
    std::optional<ember::database::ent_id> occupant;
};

class scene_gameplay final : public ember::scene {
public:
    scene_gameplay(ember::engine& eng, scene* prev);

    virtual void init() override;
    virtual void tick(float delta) override;
    virtual void render() override;
    virtual auto handle_game_input(const SDL_Event& event) -> bool override;
    virtual auto render_gui() -> sol::table override;

    board_tile& tile_at(int r, int c);

private:
    ember::camera::orthographic camera;
    ember::database entities;
    sol::table gui_state;
    sushi::mesh_group sprite_mesh;
    std::vector<ember::database::ent_id> destroy_queue;

    std::vector<board_tile> tiles;
    int num_rows;
    int num_cols;
};
