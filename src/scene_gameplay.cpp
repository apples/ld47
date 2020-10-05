#include "scene_gameplay.hpp"

#include "scene_lose.hpp"
#include "scene_mainmenu.hpp"
#include "components.hpp"
#include "meshes.hpp"

#include "board_mesh.hpp"

#include "ember/camera.hpp"
#include "ember/engine.hpp"
#include "ember/vdom.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <sushi/sushi.hpp>

namespace { // static

bool is_in(glm::vec2 p, glm::vec2 a, glm::vec2 s) {
    return p.x >= a.x && p.x <= a.x + s.x && p.y >= a.y && p.y <= a.y + s.y;
}

}

// Scene constructor
// Initializes private members and loads any prerequisite assets (usually small ones!).
// Scene is not actually the current scene yet, so interactions with the engine state and rendering are forbidden.
scene_gameplay::scene_gameplay(ember::engine& engine, ember::scene* prev)
    : scene(engine),
      camera(),                             // Camera has a sane default constructor, it is tweaked below
      entities(),                           // Entity database has no constructor parameters
      gui_state{engine.lua.create_table()}, // Gui state is initialized to an empty Lua table
      sprite_mesh{get_sprite_mesh()},       // Sprite and tilemap meshes is created statically
      tiles(3*4),
      num_rows(4),
      num_cols(3),
      board_tile_size{4.f/3.f, 4.f/3.f},
      board_mesh(),
      board_pos{6, 3},
      player_characters(),
      enemy_characters(),
      movement_cards(load_movement_cards()),
      enemy_movement_cards(load_movement_cards("/data/enemyMovement.json")),
      available_movement_cards(),
      picked_card(nullptr),
      current_turn(turn::SUMMON),
      player_start_point{1, 0},
      rng{std::random_device{}()},
      enemies_spawned(0) {
    camera.height = 9; // Height of the camera viewport in world units
    camera.aspect_ratio = 16.f/9.f;
    camera.pos = {-camera.height/2.f * camera.aspect_ratio, -camera.height/2.f, -50};

    board_mesh = make_board_mesh(4, 3, board_tile_size, {0, .25}, {.25, 0});

    gui_state["hourglass"] = [this]{ next_turn(); };

    // Load player characters
    {
        auto j = nlohmann::json{};
        std::ifstream("data/characters.json") >> j;
        
        auto i = 0;
        for (auto& c : j.items()) {
            auto mh = c.value()["max_health"].get<int>();
            auto power = c.value()["power"].get<int>();
            auto portrait = c.value()["portrait"].get<std::string>();
            auto attacks = std::vector<attack_pattern>();

            for(auto& p : c.value()["attack_pattern"].items()) {
                attacks.push_back({
                    p.value()["x"].get<int>(),
                    p.value()["y"].get<int>()
                });
            }

            player_characters.push_back({
                {mh, mh, power, portrait, std::move(attacks), false},
                {4 + 2 * i, 0},
                {112.f / 64.f, 169.f / 64.f},
                false,
                false,
            });

            ++i;
        }
    }

    // Load enemy characters
    {
        auto j = nlohmann::json{};
        std::ifstream("data/enemies.json") >> j;
        
        for (auto& c : j.items()) {
            auto mh = c.value()["max_health"].get<int>();
            auto power = c.value()["power"].get<int>();
            auto portrait = c.value()["portrait"].get<std::string>();
            auto moves = c.value()["moves"].get<std::vector<std::string>>();
            auto weight = c.value()["random_weight"].get<int>();

            auto attacks = std::vector<attack_pattern>();

            for(auto& p : c.value()["attack_pattern"].items()) {
                attacks.push_back({
                    p.value()["x"].get<int>(),
                    p.value()["y"].get<int>()
                });
            }

            enemy_characters.push_back({
                {mh, mh, power, portrait, std::move(attacks)},
                std::move(moves),
                weight
            });
        }
    }

    // Load player movement cards
    {
        auto loc = glm::vec2{11, 3};

        for (auto& c : movement_cards) {
            available_movement_cards.push_back({
                &c,
                loc,
                {65.f/64.f, 86.f/64.f},
                true,
                true,
            });

            loc.x += 1.5;

            if (loc.x >= 15.5) {
                loc.x = 11;
                loc.y += 2;
            }
        }
    }

    // Initialize board
    for (int r = 0; r < num_rows; ++r) {
        for (int c = 0; c < num_cols; ++c) {
            tile_at(r, c) = {
                r,
                c,
                std::nullopt,
                board_pos + board_tile_size/2.f + board_tile_size * glm::vec2{c, r},
            };
        }
    }
}

// Scene initialization
// Distinct from construction, all private members should have valid values now.
// Used to initialize Lua state, initialize world, populate entities, etc.
// Basically "load the stage".
void scene_gameplay::init() {
    // We want scripts to have access to the entities and other things as a global variables, so they are set here.
    engine->lua["entities"] = std::ref(entities);
    engine->lua["queue_destroy"] = [this](ember::database::ent_id eid) {
        destroy_queue.push_back(eid);
    };

    engine->lua["tile_at"] = [this](int r, int c) { return std::ref(tile_at(r, c)); };

    // Call the "init" function in the "data/scripts/scenes/gameplay.lua" script, with no params.
    engine->call_script("scenes.gameplay", "init");

    engine->soloud.stopAll();
    engine->soloud.play(*engine->music_cache.get("gameplay"));
}

// Tick/update function
// Performs all necessary game processing based on the delta time.
// Updates gui_state as necessary.
// Basically does everything except rendering.
void scene_gameplay::tick(float delta) {
    // Scripting system
    engine->call_script("systems.scripting", "visit", delta);

    // Sprite system
    entities.visit([&](component::sprite& sprite) {
        sprite.time += delta;
    });

    // Locomotion system
    entities.visit([&](ember::database::ent_id eid, component::locomotion& loco, component::transform& tform) {
        if (loco.duration > 0) {
            auto d = loco.target - tform.pos;
            auto a = d * delta / loco.duration;

            tform.pos += a;
            loco.duration -= delta;

            if (loco.duration <= 0) {
                if (!loco.return_target) {
                    if (loco.bring_me_peace) {
                        entities.destroy_entity(eid);
                    } else {
                        tform.pos = loco.target;
                        entities.remove_component<component::locomotion>(eid);
                    }
                } else {
                    loco.return_duration += loco.duration;
                }
            }
        } else if (loco.return_target) {
            auto d = *loco.return_target - tform.pos;
            auto a = d * delta / loco.return_duration;

            tform.pos += a;
            loco.return_duration -= delta;

            if (loco.return_duration <= 0) {
                if (loco.bring_me_peace) {
                    entities.destroy_entity(eid);
                } else {
                    tform.pos = *loco.return_target;
                    entities.remove_component<component::locomotion>(eid);
                }
            }
        }
    });

    // Turn systems
    switch (current_turn) {
        case turn::AUTOPLAYER:
        case turn::ENEMY_MOVE:
        case turn::ATTACK:
            if (entities.count_components<component::locomotion>() == 0) {
                next_turn(true);
            }
            break;
    }

    // Dead entity cleanup
    std::sort(begin(destroy_queue), end(destroy_queue), [](auto& a, auto& b) {
        return a.get_index() < b.get_index();
    });
    destroy_queue.erase(std::unique(begin(destroy_queue), end(destroy_queue)), end(destroy_queue));
    for (const auto& eid : destroy_queue) {
        entities.destroy_entity(eid);
    }
    destroy_queue.clear();

    // Update UI props
    gui_state["turn"] = int(current_turn);

    // Check lose
    bool lost = true;
    for (auto& c : player_characters) {
        if (!c.dead) {
            lost = false;
            break;
        }
    }
    if (lost) {
        engine->queue_transition<scene_lose>();
    }
}

// Render function
// Notice the lack of delta time.
// Performs all world rendering, but not GUI.
// Responsible for setting up the OpenGL state, binding shaders, etc.
// Must be *fast*!
// The EZ3D helper can be used for efficient 3D rendering, currently there is no equivalent for 2D.
void scene_gameplay::render() {
    // Set up some OpenGL state. This is mostly copy-pasted and should be self-explanatory.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glEnable(GL_SAMPLE_COVERAGE);
    //glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    // Mouse pos
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    auto mouse = glm::vec2{16.f * mx / float(engine->display.width), 9.f * (1.f - my / float(engine->display.height))};

    // Bind the basic shader and set some defaults.
    engine->basic_shader.bind();
    engine->basic_shader.set_cam_forward(get_forward(camera));
    engine->basic_shader.set_tint({1, 1, 1, 1});
    engine->basic_shader.set_hue(0);
    engine->basic_shader.set_saturation(1);

    // Get projection and view matrices from camera.
    auto proj = get_proj(camera);
    auto view = get_view(camera);

    auto projview = proj * view;

    auto draw_sprite = [&](glm::vec3 pos, glm::vec2 size, const std::string& name, glm::vec2 uv1, glm::vec2 uv2) {
        auto modelmat = glm::mat4(1);
        modelmat = glm::translate(modelmat, pos);
        modelmat = glm::scale(modelmat, {size, 1});

        auto uvmat = glm::mat3({uv2.x - uv1.x, 0, 0}, {0, uv2.y - uv1.y, 0}, {uv1.x, uv1.y, 1});

        // Set matrix uniforms.
        engine->basic_shader.set_uvmat(uvmat);
        engine->basic_shader.set_normal_mat(glm::inverseTranspose(view * modelmat));
        engine->basic_shader.set_MVP(projview * modelmat);

        sushi::set_texture(0, *engine->texture_cache.get(name));
        sushi::draw_mesh(sprite_mesh);
    };

    // Render background
    {
        draw_sprite({0, 0, -10}, {16, 9}, "background", {0, 0}, {1, 1});
    }

    // Render board
    {
        auto modelmat = glm::mat4(1);
        modelmat = glm::translate(modelmat, {board_pos, 0});

        // Set matrix uniforms.
        engine->basic_shader.set_uvmat(glm::mat3(1.f));
        engine->basic_shader.set_normal_mat(glm::inverseTranspose(view * modelmat));
        engine->basic_shader.set_MVP(projview * modelmat);

        sushi::set_texture(0, *engine->texture_cache.get("board"));
        sushi::draw_mesh(board_mesh);
    }

    // Render board overlays
    {
        engine->basic_shader.set_tint({1, 1, 1, 0.5});
        for (int r = 0; r < num_rows; ++r) {
            for (int c = 0; c < num_cols; ++c) {
                auto& t = tile_at(r, c);
                if (t.enemy_spawning) {
                    draw_sprite(
                        glm::vec3{t.center + glm::vec2{-0.5, -0.5}, 5}, {1, 1}, "overlays", {0, 0.25}, {0.25, 0.5});
                }
            }
        }
        engine->basic_shader.set_tint({1, 1, 1, 1});
    }

    // Render character sheets
    {
        auto card_width_px = 112.f;
        auto card_height_px = 169.f;

        for (auto& c : player_characters) {
            if (c.dead) {
                engine->basic_shader.set_saturation(0);
                engine->basic_shader.set_tint({0.5, 0.5, 0.5, 0.5});
            }

            // Card
            draw_sprite({c.pos, 1}, c.size, "character_card2", {0, 0}, {card_width_px / 256.f, card_height_px / 256.f});

            // Portrait
            if (!c.dead) {
                if (c.deployed) {
                    engine->basic_shader.set_saturation(0);
                    engine->basic_shader.set_tint({0.5, 0.5, 0.5, 0.5});
                }
                draw_sprite(
                    {c.pos + glm::vec2{0, (card_height_px - 64.f) / 64.f}, 2}, {1, 1}, c.base.portrait, {0, 0}, {1, 1});
                if (c.deployed) {
                    engine->basic_shader.set_saturation(1);
                    engine->basic_shader.set_tint({1, 1, 1, 1});
                }
            }

            // Health
            for (int i = 0; i < c.base.max_health; ++i) {
                auto uv1 = glm::vec2{0.5f, 1.f/8.f};
                if (i < c.base.health) {
                    uv1.y = 0.f;
                }
                auto x = 67.f/64.f;
                auto y = (card_height_px - 22.f - 21.f*i)/64.f;
                draw_sprite(
                    {c.pos + glm::vec2{x, y}, 2}, {0.5, 0.5}, "character_card2", uv1, uv1 + glm::vec2{0.125, 0.125});
            }

            // Power
            for (int i = 0; i < c.base.max_health; ++i) {
                auto uv1 = glm::vec2{0.625f, 0.f};
                auto x = 87.f/64.f;
                auto y = (card_height_px - 22.f - 21.f*i)/64.f;
                draw_sprite(
                    {c.pos + glm::vec2{x, y}, 2}, {0.5, 0.5}, "character_card2", uv1, uv1 + glm::vec2{0.125, 0.125});
            }

            // Attacks
            for (attack_pattern pattern : c.base.attack_patterns) {
                auto uv1 = glm::vec2{0.5f, 4.f/8.f};
                auto x = (48.f + pattern.x * 21.f)/64.f;
                auto y = (45.f + pattern.y * 20.f)/64.f;
                draw_sprite(
                    {c.pos + glm::vec2{x, y}, 2}, {0.5, 0.5}, "character_card2", uv1, uv1 + glm::vec2{0.125, 0.125});
            }

            if (c.dead) {
                engine->basic_shader.set_saturation(1);
                engine->basic_shader.set_tint({1, 1, 1, 1});
            }
        }
    }

    auto render_line =
        [&](glm::vec3 a, glm::vec3 b, float thiccness) {
            auto dir = b-a;
            auto len = glm::length(dir);
            auto angle = glm::orientedAngle(glm::vec3{0, 1, 0}, glm::normalize(dir), glm::vec3{0, 0, 1});

            auto modelmat = glm::mat4(1);
            modelmat = glm::translate(modelmat, a);
            modelmat = glm::rotate(modelmat, angle, {0, 0, 1});
            modelmat = glm::scale(modelmat, {thiccness, len, 1});
            modelmat = glm::translate(modelmat, {-0.5, 0, 0});

            auto uv1 = glm::vec2{0.75, 0.25};
            auto uvd = glm::vec2{3.f/256.f, 13.f/256.f};
            auto uvmat = glm::mat3({uvd.x, 0, 0}, {0, uvd.y, 0}, {uv1.x, uv1.y, 1});

            // Set matrix uniforms.
            engine->basic_shader.set_uvmat(uvmat);
            engine->basic_shader.set_normal_mat(glm::inverseTranspose(view * modelmat));
            engine->basic_shader.set_MVP(projview * modelmat);

            sushi::set_texture(0, *engine->texture_cache.get("character_card2"));
            sushi::draw_mesh(sprite_mesh);
        };

    auto render_movement_card = [&](const glm::vec3& loc, const glm::vec2& size, const movement_card& c) {
        draw_sprite(loc, size, "character_card2", {0.f, 170.f / 256.f}, {65.f / 256.f, 1.f});

        auto pos = loc + glm::vec3{23.f / 64.f, 2.f / 64.f, 1};
        auto offs = glm::vec3{21.f / 64.f, 21.f / 64.f, 0};

        auto uv1 = glm::vec2{0.5f, 0.25f};
        auto uvd = glm::vec2{0.125f, 0.125f};

        auto line_from = std::optional<glm::vec3>(std::nullopt);

        for (auto& m : c.movements) {
            pos += offs * glm::vec3{m.x, m.y, 1};

            auto uvoffs = glm::vec2{0, 0};

            if (m.attack) {
                uvoffs = {0.375, 0};
            }

            draw_sprite(pos, {0.5f, 0.5f}, "character_card2", uv1 + uvoffs, uv1 + uvoffs + uvd);

            uv1.y = 0.375f;

            // Line
            if (line_from) {
                auto from = *line_from + glm::vec3{9.f/64.f, 9.f/64.f, 1.f};
                auto to = pos + glm::vec3{9.f/64.f, 9.f/64.f, 1.f};

                auto d = to - from;
                auto nd = glm::normalize(d);

                from += nd * 9.f/64.f;
                to -= nd * 8.f/64.f;

                from.z += 1;
                to.z += 1;

                render_line(from, to, 3.f/64.f);
            }
            line_from = pos;
        }
    };

    // Render movement cards
    {
        for (auto& c : available_movement_cards) {
            if(c.visible) {
                auto loc = glm::vec3{c.pos, 1};

                if (&c == picked_card) {
                    loc = glm::vec3{mouse - glm::vec2{0.5f, 2.f/3.f}, 1};
                }

                if (!c.pickable) {
                    engine->basic_shader.set_tint({0.5, 0.5, 0.5, 0.5});
                }

                render_movement_card(loc, c.size, *c.data);

                if (!c.pickable) {
                    engine->basic_shader.set_tint({1, 1, 1, 1});
                }
            }
        }
    }

    // Render entities
    entities.visit([&](ember::database::ent_id eid, component::sprite& sprite, const component::transform& transform) {
        auto modelmat = to_mat4(transform);
        auto tex = engine->texture_cache.get(sprite.texture);

        // Calculate UV matrix for rendering the correct sprite.
        auto cols = int(1 / sprite.size.x);
        auto frame = sprite.frames[int(sprite.time * 12) % sprite.frames.size()];
        auto uvoffset = glm::vec2{frame % cols, frame / cols} * sprite.size + sprite.inset * sprite.size;
        auto uvmat = glm::mat3{1.f};
        uvmat = glm::translate(uvmat, uvoffset);
        uvmat = glm::scale(uvmat, sprite.size - 2.f * sprite.inset * sprite.size);

        // Set matrix uniforms.
        engine->basic_shader.set_uvmat(uvmat);
        engine->basic_shader.set_normal_mat(glm::inverseTranspose(view * modelmat));
        engine->basic_shader.set_MVP(projview * modelmat);

        if (tex) {
            sushi::set_texture(0, *tex);
        } else {
            std::cout << "Warning: Texture not found: " << sprite.texture << std::endl;
        }
        
        sushi::draw_mesh(sprite_mesh);

        if (auto cref = entities.get_component<component::character_ref*>(eid)) {
            // Health
            for (int i = 0; i < cref->c->max_health; ++i) {
                auto uv1 = glm::vec2{0.5f, 1.f / 8.f};
                if (i < cref->c->health) {
                    uv1.y = 0.f;
                }
                auto x = 0.8f;
                auto y = 0.8f - (i * 21.f/64.f);
                draw_sprite(
                    transform.pos + glm::vec3{x, y, 1},
                    {0.5, 0.5},
                    "character_card2",
                    uv1,
                    uv1 + glm::vec2{0.125, 0.125});
            }

            // Render play/pause icon
            if (current_turn == turn::SET_ACTIONS) {
                if (cref->player_controlled) {
                    auto paused = cref->a == component::character_ref::PAUSE;

                    auto uv1 = glm::vec2{0, 0};

                    if (paused) {
                        uv1 = glm::vec2{0.25, 0};
                    }

                    auto pos = transform.pos;
                    pos.z += 1;

                    engine->basic_shader.set_tint({1, 1, 1, 0.5});
                    draw_sprite(pos, {1, 1}, "overlays", uv1, uv1 + glm::vec2{0.25, 0.25});
                    engine->basic_shader.set_tint({1, 1, 1, 1});
                }
            }

            // Render movement card
            if (is_in(mouse, transform.pos, {1, 1})) {
                auto pos = transform.pos + glm::vec3{1, 1, 1};
                if (pos.y > 9.f - 86.f/64.f) {
                    pos.y = 9.f - 86.f/64.f;
                }
                render_movement_card(pos, glm::vec2{65.f / 64.f, 86.f / 64.f}, *cref->m);
            }
        }
    });
}

// Handle input events, called asynchronously
auto scene_gameplay::handle_game_input(const SDL_Event& event) -> bool {
    // using component::controller;

    // Updates a single key for all controllers
    // auto update = [&](bool press, bool controller::*key, bool controller::*pressed) {
    //     entities.visit([&](controller& con) {
    //         if (key) con.*key = press;
    //         if (pressed) con.*pressed = press;
    //     });
    // };

    // Processes a key event
    auto process_key = [&](const SDL_KeyboardEvent& key){
        auto pressed = key.state == SDL_PRESSED;
        switch (key.keysym.sym) {
            // case SDLK_LEFT:
            //     update(pressed, &controller::left, nullptr);
            //     return true;
            // case SDLK_RIGHT:
            //     update(pressed, &controller::right, nullptr);
            //     return true;
            case SDLK_SPACE:
                if (pressed && key.repeat == 0) {
                    next_turn();
                    return true;
                }
                return false;
            default:
                return false;
        }
    };

    // Mouse down
    auto process_mouse_down = [&](const SDL_MouseButtonEvent& button) {
        auto screenpos =
            glm::vec2{button.x / float(engine->display.width), 1.f - button.y / float(engine->display.height)};
        
        auto p = screenpos * glm::vec2{16.f, 9.f};
        
        // Check card picking
        if (current_turn == turn::SUMMON) {
            for (auto& c : available_movement_cards) {
                if (c.visible && c.pickable && is_in(p, c.pos, c.size)) {
                    picked_card = &c;
                    return true;
                }
            }
        }

        // Check unit state toggling
        if (current_turn == turn::SET_ACTIONS) {
            entities.visit([&](component::character_ref& cref, const component::transform& tform) {
                if (cref.player_controlled && p.x >= tform.pos.x && p.x <= tform.pos.x + 1 && p.y >= tform.pos.y && p.y <= tform.pos.y + 1) {
                    switch (cref.a) {
                        case component::character_ref::PAUSE:
                            cref.a = component::character_ref::PLAY;
                            break;
                        case component::character_ref::PLAY:
                            cref.a = component::character_ref::PAUSE;
                            break;
                    }
                }
            });
        }

        return false;
    };

    // Mouse up
    auto process_mouse_up = [&](const SDL_MouseButtonEvent& button) {
        auto screenpos =
            glm::vec2{button.x / float(engine->display.width), 1.f - button.y / float(engine->display.height)};
        
        auto p = screenpos * glm::vec2{16.f, 9.f};

        if (current_turn == turn::SUMMON && picked_card != nullptr) {
            auto& card = *picked_card;
            for (auto& player : player_characters) {
                if(!player.dead && !player.deployed &&
                    player.pos.x < p.x &&
                    player.pos.x + (player.size.x / 1) > p.x &&
                    player.pos.y < p.y &&
                    player.pos.y + (player.size.y / 1) > p.y ){
                    auto r = card.data->movements[0].y + player_start_point.y;
                    auto c = card.data->movements[0].x + player_start_point.x;
                    if (!tile_at(r, c).occupant) {
                        card.visible = false;
                        auto [eid, cref, sref] = spawn_entity(r, c);
                        cref->c = &player.base;
                        cref->m = card.data;
                        cref->player_controlled = true;
                        sref->texture = cref->c->portrait + "_sprite";
                        sref->size = {0.5, 0.5};
                        sref->inset = {0.15, 0.15};
                        sref->frames = {0};
                        player.deployed = true;
                        engine->soloud.play(*engine->sound_cache.get("cardstack"));
                        next_turn();
                        break;
                    }
                }
            }
        }
        picked_card = nullptr;

        return true;
    };

    switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return process_key(event.key);
        case SDL_MOUSEBUTTONDOWN:
            return process_mouse_down(event.button);
        case SDL_MOUSEBUTTONUP:
            return process_mouse_up(event.button);
    }
    return false;
}

// Render GUI, which means returning the result of `create_element`.
auto scene_gameplay::render_gui() -> sol::table {
    return ember::vdom::create_element(engine->lua, "gui.scene_gameplay.root", gui_state, engine->lua.create_table());
}

board_tile& scene_gameplay::tile_at(int r, int c) { 
    if (r < 0 || r >= num_rows || c < 0 || c >= num_cols) {
        std::cerr << "ERROR: Invalid tile coordinate: " << r << "," << c << std::endl;
        return tiles[0];
    }

    return tiles[r * num_cols + c];
}

void scene_gameplay::next_turn(bool force) {
    switch (current_turn) {
        case turn::SET_ACTIONS:
            enter_turn(turn::AUTOPLAYER);
            break;
        case turn::AUTOPLAYER:
            if (force) {
                enter_turn(turn::SUMMON);
            }
            break;
        case turn::SUMMON:
            for (auto& c : available_movement_cards) {
                c.pickable = false;
            }
            picked_card = nullptr;
            enter_turn(turn::ATTACK);
            break;
        case turn::ATTACK:
            enter_turn(turn::RETURN);
            break;
        case turn::RETURN:
            enter_turn(turn::ENEMY_MOVE);
            break;
        case turn::ENEMY_MOVE:
            enter_turn(turn::ENEMY_SPAWN);
            break;
        case turn::ENEMY_SPAWN:
            enter_turn(turn::ENEMY_ATTACK);
            break;
        case turn::ENEMY_ATTACK:
            enter_turn(turn::SET_ACTIONS);
            break;
    }
}

void scene_gameplay::enter_turn(turn t) {
    current_turn = t;
    switch (current_turn) {
        case turn::SET_ACTIONS:
            break;
        case turn::AUTOPLAYER:
            move_units(true);
            break;
        case turn::SUMMON:
            for (auto& c : available_movement_cards) {
                c.pickable = true;
            }
            break;
        case turn::ATTACK:
            do_attacks(true);
            break;
        case turn::RETURN:
            return_units();
            break;
        case turn::ENEMY_MOVE:
            enemy_ai();
            break;
        case turn::ENEMY_SPAWN:
            spawn_enemy();
            next_turn(true);
            break;
        case turn::ENEMY_ATTACK:
            do_attacks(false);
            break;
    }
}

auto scene_gameplay::spawn_entity(int r, int c) -> spawn_result {
    auto transform = component::transform{};
    transform.pos = {tile_at(r, c).center - glm::vec2{0.5, 0.5}, 1};

    auto sprite = component::sprite{};

    auto character_ref = component::character_ref{};
    character_ref.board_pos = {c, r};

    auto eid = entities.create_entity();
    entities.add_component(eid, transform);
    auto sid = entities.add_component(eid, sprite);
    auto cid = entities.add_component(eid, character_ref);

    tile_at(r, c).occupant = eid;

    return {
        eid,
        &entities.get_component_by_id<component::character_ref>(cid),
        &entities.get_component_by_id<component::sprite>(sid),
    };
}

void scene_gameplay::move_units(bool player_controlled) {
    struct unit {
        ember::database::ent_id eid;
        component::character_ref* cref;
        component::sprite* sref;
    };

    // Deferred units will repeatedly try to move until they succeed or a deadlock occurs
    std::vector<unit> deferred;
    deferred.reserve(entities.count_components<component::character_ref>());

    auto try_move = [&](const unit& u) {
        auto next_index = (u.cref->move_index + 1) % u.cref->m->movements.size();

        auto offs = u.cref->m->movements[next_index];

        auto next_pos = *u.cref->board_pos;
        
        if (next_index == 0) {
            next_pos = player_start_point;
        }
        
        next_pos += glm::ivec2{offs.x, offs.y};

        auto& cur_tile = tile_at(u.cref->board_pos->y, u.cref->board_pos->x);
        auto& next_tile = tile_at(next_pos.y, next_pos.x);

        // Check collision
        if (next_tile.occupant) {
            if (auto ocref = entities.get_component<component::character_ref*>(*next_tile.occupant)) {
                if (ocref->player_controlled != u.cref->player_controlled) {
                    engine->soloud.play(*engine->sound_cache.get("attack1"));
                    if (!damage(*next_tile.occupant, *ocref, u.cref->c->power)) {
                        entities.add_component(
                            u.eid,
                            component::locomotion{
                                {next_tile.center - glm::vec2{0.5, 0.5}, 1},
                                0.15,
                                glm::vec3{cur_tile.center - glm::vec2{0.5, 0.5}, 1},
                                0.15,
                            });

                        return true;
                    }
                    else if(u.cref->player_controlled){
                        u.cref->c->returning = true;
                    }
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        // Actually move the unit
        auto d = next_tile.center - cur_tile.center;

        cur_tile.occupant = std::nullopt;
        next_tile.occupant = u.eid;
        u.cref->board_pos = next_pos;
        u.cref->move_index = next_index;

        if (std::abs(d.x) > std::abs(d.y)) {
            if (d.x > 0) {
                u.sref->frames = {1};
            } else {
                u.sref->frames = {2};
            }
        } else {
            if (d.y > 0) {
                u.sref->frames = {0};
            } else {
                u.sref->frames = {3};
            }
        }

        entities.add_component(u.eid, component::locomotion{
            {next_tile.center - glm::vec2{0.5, 0.5}, 1},
            0.25,
        });

        u.cref->did_move = true;

        return true;
    };

    // Initial moves
    entities.visit([&](ember::database::ent_id eid, component::character_ref& cref, component::sprite& sref) {
        if (cref.player_controlled == player_controlled) {
            cref.did_move = false;
            if (cref.a == component::character_ref::PLAY) {
                if (!try_move({eid, &cref, &sref})) {
                    deferred.push_back({eid, &cref, &sref});
                }
            }
        }
    });

    // Move deferred until no more possible moves
    auto last_sz = deferred.size();
    while (last_sz > 0) {
        auto nd = std::move(deferred);
        for (auto& u : nd) {
            if (!try_move(u)) {
                deferred.push_back(u);
            }
        }
        // Check for deadlock
        if (last_sz == deferred.size()) {
            break;
        }
        last_sz = deferred.size();
    }
}

void scene_gameplay::do_attacks(bool player_controlled) {
    entities.visit([&](ember::database::ent_id eid, component::character_ref& cref) {
        if (cref.player_controlled == player_controlled && cref.board_pos) {
            auto offs = cref.m->movements[cref.move_index];

            if (offs.attack && cref.did_move) {
                // attack
                engine->soloud.play(*engine->sound_cache.get("attack2"));
                for (auto& pattern : cref.c->attack_patterns) {
                    auto r = cref.board_pos->y + pattern.y;
                    auto c = cref.board_pos->x + pattern.x;
                    if (r < num_rows && r >= 0 && c < num_cols && c >= 0) {
                        auto& tile = tile_at(r, c);
                        {
                            auto atkent = entities.create_entity();
                            auto a = tile_at(*cref.board_pos).center - glm::vec2{0.5, 0.5};
                            auto b = tile.center - glm::vec2{0.5, 0.5};

                            entities.add_component(atkent, component::transform{{{a, 4}}});
                            entities.add_component(atkent, component::locomotion{{b, 4}, 0.2, std::nullopt, 0, true});

                            auto s = component::sprite{};
                            s.texture = "attack";
                            s.frames = {0};

                            entities.add_component(atkent, s);
                        }
                        if (tile.occupant) {
                            if (auto ocref = entities.get_component<component::character_ref*>(*tile.occupant)) {
                                if (ocref->player_controlled != cref.player_controlled) {
                                    if(damage(*tile.occupant, *ocref, cref.c->power) && cref.player_controlled){
                                        cref.c->returning = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    });
}

void scene_gameplay::return_units() {
    entities.visit([&](ember::database::ent_id eid, component::character_ref& cref) {
        if (cref.player_controlled) {

            if (cref.move_index == cref.m->movements.size() - 1 && cref.c->returning) {
                auto& player_char = reinterpret_cast<player_character_card&>(*cref.c);
                player_char.deployed = false;
                if(cref.c->health < 3){
                    cref.c->health++;
                }
                if(cref.c->max_health < 3){
                    cref.c->max_health++;
                }
                cref.c->returning = false;

                for (auto& c : available_movement_cards) {
                    if (c.data == cref.m) {
                        c.visible = true;
                    }
                }

                tile_at(*cref.board_pos).occupant = std::nullopt;
                entities.destroy_entity(eid);
            }
        }
    });
}

void scene_gameplay::enemy_ai() {
    auto get_next_pos = [&](const component::character_ref& cref) {
        auto next_index = (cref.move_index + 1) % cref.m->movements.size();

        auto offs = cref.m->movements[next_index];

        auto next_pos = *cref.board_pos;
        
        if (next_index == 0) {
            next_pos = player_start_point;
        }
        
        next_pos += glm::ivec2{offs.x, offs.y};

        return next_pos;
    };

    // Threat detection
    for (auto& t : tiles) {
        t.player_threatens = false;
    }
    entities.visit([&](ember::database::ent_id eid, component::character_ref& cref) {
        if (cref.player_controlled) {
            auto next_pos = get_next_pos(cref);

            auto& cur_tile = tile_at(cref.board_pos->y, cref.board_pos->x);
            auto& next_tile = tile_at(next_pos.y, next_pos.x);

            if (next_tile.occupant) {
                if (auto ocref = entities.get_component<component::character_ref*>(eid)) {
                    if (!ocref->player_controlled) {
                        next_tile.player_threatens = true;
                    }
                }
            } else {
                next_tile.player_threatens = true;
            }
        }
    });

    // Decision making
    entities.visit([&](ember::database::ent_id eid, component::character_ref& cref) {
        if (!cref.player_controlled) {
            auto next_pos = get_next_pos(cref);

            auto& cur_tile = tile_at(cref.board_pos->y, cref.board_pos->x);
            auto& next_tile = tile_at(next_pos.y, next_pos.x);

            if (cur_tile.player_threatens) {
                cref.a = component::character_ref::PLAY;
            } else if (next_tile.player_threatens) {
                cref.a = component::character_ref::PAUSE;
            } else {
                cref.a = component::character_ref::PLAY;
            }
        }
    });

    move_units(false);
}

void scene_gameplay::spawn_enemy() {
    // Spawn previous incoming
    for (auto r = 0; r < num_rows; ++r) {
        for (auto c = 0; c < num_cols; ++c) {
            auto& tile = tile_at(r, c);
            if (tile.enemy_spawning && !tile.occupant) {
                auto i = tile.spawn_enemy_id;
                auto& e = enemy_characters[i];

                auto [eid, cref, sref] = spawn_entity(r, c);
                auto cid = entities.add_component(eid, enemy_characters[i].base);
                cref->c = &entities.get_component_by_id<character>(cid);
                cref->m = &enemy_movement_cards[tile.spawn_move_id];
                cref->player_controlled = false;
                sref->texture = e.base.portrait + "_sprite";
                sref->frames = {3};
                sref->size = {0.5, 0.5};
                sref->inset = {0.15, 0.15};

                tile.enemy_spawning = false;
            }
        }
    }

    // Count
    int enemy_count = 0;
    entities.visit([&](component::character_ref& c) {
        if (!c.player_controlled) {
            ++enemy_count;
        }
    });

    // Mark incoming
    if (enemy_count < 4) {
        auto i = 0;
        if (enemies_spawned % 7 < 6) {
            auto tweight = 0;
            for (auto& c : enemy_characters) {
                tweight += c.weight;
            }
            auto roll = std::uniform_int_distribution<>{0, int(tweight - 1)}(rng);
            while (roll >= enemy_characters[i].weight) {
                roll -= enemy_characters[i].weight;
                ++i;
            }
        } else {
            i = 2;
        }
        auto& e = enemy_characters[i];

        int r;
        int c;

        auto mi = std::uniform_int_distribution<>{0, int(e.moves.size() - 1)}(rng);
        for (int i = 0; i < enemy_movement_cards.size(); ++i) {
            auto& m = enemy_movement_cards[i];
            if (m.name == e.moves[mi]) {
                r = player_start_point.y + m.movements[0].y;
                c = player_start_point.x + m.movements[0].x;
                mi = i;
                break;
            }
        }

        auto& t = tile_at(r, c);
        if (!t.occupant) {
            t.enemy_spawning = true;
            t.spawn_enemy_id = i;
            t.spawn_move_id = mi;

            ++enemies_spawned;
        }
    }
}

bool scene_gameplay::damage(ember::database::ent_id eid, component::character_ref& cref, int dmg) {
    cref.c->health -= dmg;
    if (cref.c->health <= 0) {
        cref.c->health = 0;
        if (cref.player_controlled) {
            // Dangerous cast
            auto& player_char = reinterpret_cast<player_character_card&>(*cref.c);
            player_char.deployed = false;
            player_char.dead = true;

            for (auto& c : available_movement_cards) {
                if (c.data == cref.m) {
                    c.visible = true;
                }
            }
        }
        tile_at(*cref.board_pos).occupant = std::nullopt;
        entities.destroy_entity(eid);
        return true;
    }
    return false;
}