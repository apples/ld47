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
#include <random>

struct board_tile {
    int r = 0;
    int c = 0;
    std::optional<ember::database::ent_id> occupant;
    glm::vec2 center;
    bool player_threatens = false;
    bool enemy_spawning = false;
    int spawn_enemy_id = 0;
    int spawn_move_id = 0;
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
    bool deployed;
    bool dead;
};

struct enemy_character {
    character base;
    std::vector<std::string> moves;
    int weight = 0;
};

enum class turn {
    SUMMON,
    SET_ACTIONS,
    AUTOPLAYER,
    ENEMY_SPAWN,
    ENEMY_MOVE,
    ATTACK,
    ENEMY_ATTACK,
    RETURN
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

    board_tile& tile_at(glm::ivec2 v) { return tile_at(v.y, v.x); };

    void next_turn(bool force = false);

    auto spawn_entity(int r, int c) -> spawn_result;

    void move_units(bool player_controlled);

    void enemy_ai();

    void enter_turn(turn t);

    void do_attacks(bool player_controlled);

    void return_units();
    
    void spawn_enemy();

    bool damage(ember::database::ent_id eid, component::character_ref& cref, int dmg);

    struct stats {
        int enemies_spawned;
        int turn_count;
        int dagrons_defeated;
    };

    auto get_stats() const { return stats{ enemies_spawned, turn_count, dagrons_defeated }; };

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
    std::vector<enemy_character> enemy_characters;

    std::vector<movement_card> movement_cards;
    std::vector<movement_card> enemy_movement_cards;

    std::vector<movement_card_item> available_movement_cards;
    movement_card_item* picked_card;

    turn current_turn;

    glm::ivec2 player_start_point;

    std::mt19937 rng;

    int enemies_spawned;
    int turn_count;
    int dagrons_defeated;
};
