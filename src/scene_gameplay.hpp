#pragma once

#include "character.hpp"
#include "movement.hpp"
#include "components.hpp"

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
    glm::vec2 center;
};

struct movement_card_item {
    movement_card* data;
    glm::vec2 pos;
    glm::vec2 size;
    bool visible;
    bool pickable;
};

struct player_character_card {
    character base;
    glm::vec2 pos;
    glm::vec2 size;
};

enum class turn {
    SUMMON,
    SET_ACTIONS,
    AUTOPLAYER,
    ENEMY_SPAWN,
    ENEMY_MOVE,
};

struct spawn_result {
    ember::database::ent_id eid;
    component::character_ref* cref;
    component::sprite* sprite;
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

    void next_turn();

    auto spawn_entity(int r, int c) -> spawn_result;

private:
    ember::camera::orthographic camera;
    ember::database entities;
    sol::table gui_state;
    sushi::mesh_group sprite_mesh;
    std::vector<ember::database::ent_id> destroy_queue;

    std::vector<board_tile> tiles;
    int num_rows;
    int num_cols;
    glm::vec2 board_tile_size;
    sushi::mesh_group board_mesh;
    glm::vec2 board_pos;

    std::vector<player_character_card> player_characters;

    std::vector<movement_card> movement_cards;

    std::vector<movement_card_item> available_movement_cards;
    movement_card_item* picked_card;

    turn current_turn;
};
