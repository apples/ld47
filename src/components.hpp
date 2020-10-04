#pragma once

#include "character.hpp"
#include "movement.hpp"

#include "ember/component_common.hpp"
#include "ember/entities.hpp"
#include "ember/net_id.hpp"
#include "ember/ez3d.hpp"

#include <sushi/sushi.hpp>
#include <box2d/box2d.h>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <memory>

#define EMBER_REFLECTION_ENABLE_REGISTRY
#include "ember/reflection_start.hpp"

namespace component {

/** Actor script used for various events */
struct script {
    std::string name; /** Script name within the 'actors.' namespace */
};
REFLECT(script, (name))

/** World transform */
struct transform : sushi::transform {};
REFLECT(transform, (pos)(rot)(scl))

struct sprite {
    std::string texture;
    glm::vec2 size = {1, 1};
    std::vector<int> frames;
    float time = 0;
};
REFLECT(sprite, (texture)(size)(frames)(time))

struct character_ref {
    enum action {
        PLAY,
        PAUSE,
        ATTACK
    };

    character* c = nullptr;
    movement_card* m = nullptr;
    std::optional<glm::ivec2> board_pos;
    action a = PLAY;
    int move_index = 0;
    bool player_controlled = false;
};
REFLECT(character_ref, (c)(m)(board_pos)(a)(player_controlled))

struct locomotion {
    glm::vec3 target = {0, 0, 0};
    float duration = 0;
};
REFLECT(locomotion, (target)(duration));

} // namespace component

#include "ember/reflection_end.hpp"
