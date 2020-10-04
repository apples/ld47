#include "scene_gameplay.hpp"

#include "scene_mainmenu.hpp"
#include "components.hpp"
#include "meshes.hpp"

#include "board_mesh.hpp"

#include "ember/camera.hpp"
#include "ember/engine.hpp"
#include "ember/vdom.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>

#include <sushi/sushi.hpp>

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
      movement_cards(load_movement_cards()),
      available_movement_cards(),
      picked_card(nullptr) {
    camera.height = 9; // Height of the camera viewport in world units
    camera.aspect_ratio = 16.f/9.f;
    camera.pos = {-camera.height/2.f * camera.aspect_ratio, -camera.height/2.f, -50};

    board_mesh = make_board_mesh(4, 3, board_tile_size, {0, .25}, {.25, 0});

    player_characters.push_back({
        {
            1,
            1,
            1,
            "kagami",
        },
        {4, 0},
        {1.75, 2.5},
    });

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

    // Spawn test entity
    auto [eid, cref, sref] = spawn_entity(0, 1);
    cref->c = &player_characters[0].base;
    cref->m = available_movement_cards[0].data;
    sref->texture = "kagami";
    sref->frames = {0};
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

    // Dead entity cleanup
    std::sort(begin(destroy_queue), end(destroy_queue), [](auto& a, auto& b) {
        return a.get_index() < b.get_index();
    });
    destroy_queue.erase(std::unique(begin(destroy_queue), end(destroy_queue)), end(destroy_queue));
    for (const auto& eid : destroy_queue) {
        entities.destroy_entity(eid);
    }
    destroy_queue.clear();
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

    // Render character sheets
    {
        for (auto& c : player_characters) {
            draw_sprite({c.pos, 1}, c.size, "character_card", {0, 0}, {7.f/16.f, 10.f/16.f});
            draw_sprite({c.pos + glm::vec2{0, 1.5}, 2}, {1, 1}, c.base.portrait, {0, 0}, {1, 1});
        }
    }

    // Render movement cards
    {
        for (auto& c : available_movement_cards) {
            if(c.visible) {
                auto loc = glm::vec3{c.pos, 1};

                if (&c == picked_card) {
                    loc = glm::vec3{mouse - glm::vec2{0.5f, 2.f/3.f}, 1};
                }

                draw_sprite(loc, c.size, "character_card", {0.f, 170.f/256.f}, {65.f/256.f, 1.f});

                auto pos = loc + glm::vec3{23.f/64.f, 2.f/64.f, 1};
                auto offs = glm::vec3{19.f/64.f, 19.f/64.f, 0};

                auto uv1 = glm::vec2{0.5f, 0.25f};
                auto uvd = glm::vec2{0.125f, 0.125f};

                for (auto& m : c.data->movements) {
                    pos += offs * glm::vec3{m.x, m.y, 1};

                    draw_sprite(pos, {0.5f, 0.5f}, "character_card", uv1, uv1 + uvd);

                    uv1.y = 0.375f;
                }
            }
        }
    }

    // Render entities
    entities.visit([&](const component::sprite& sprite, const component::transform& transform) {
        auto modelmat = to_mat4(transform);
        auto tex = engine->texture_cache.get(sprite.texture);

        // Calculate UV matrix for rendering the correct sprite.
        auto cols = int(1 / sprite.size.x);
        auto frame = sprite.frames[int(sprite.time * 12) % sprite.frames.size()];
        auto uvoffset = glm::vec2{frame % cols, frame / cols} * sprite.size;
        auto uvmat = glm::mat3{1.f};
        uvmat = glm::translate(uvmat, uvoffset);
        uvmat = glm::scale(uvmat, sprite.size);

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
                next_turn();
                return true;
            default:
                return false;
        }
    };

    auto process_mouse_down = [&](const SDL_MouseButtonEvent& button) {
        auto screenpos =
            glm::vec2{button.x / float(engine->display.width), 1.f - button.y / float(engine->display.height)};
        
        auto p = screenpos * glm::vec2{16.f, 9.f};
        
        for (auto& c : available_movement_cards) {
            if (c.pickable && p.x >= c.pos.x && p.x <= c.pos.x + c.size.x && p.y >= c.pos.y && p.y <= c.pos.y + c.size.y) {
                picked_card = &c;
                return true;
            }
        }

        return false;
    };

    auto process_mouse_up = [&](const SDL_MouseButtonEvent& button) {
        auto screenpos =
            glm::vec2{button.x / float(engine->display.width), 1.f - button.y / float(engine->display.height)};
        
        auto p = screenpos * glm::vec2{16.f, 9.f};

        if(picked_card != nullptr) {
            for(player_character_card player : player_characters) {
                if(player.pos.x < p.x &&
                    player.pos.x + (player.size.x / 1) > p.x &&
                    player.pos.y < p.y &&
                    player.pos.y + (player.size.y / 1) > p.y ){

                    (*picked_card).visible = false;
                    //spawn_entity();
                    break;
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
    return tiles[r * num_cols + c];
}

void scene_gameplay::next_turn() {
    // TODO
}

auto scene_gameplay::spawn_entity(int r, int c) -> spawn_result {
    auto transform = component::transform{};
    transform.pos = {tile_at(r, c).center - glm::vec2{0.5, 0.5}, 1};

    auto sprite = component::sprite{};

    auto character_ref = component::character_ref{};
    character_ref.board_pos = {r, c};

    auto eid = entities.create_entity();
    entities.add_component(eid, transform);
    auto sid = entities.add_component(eid, sprite);
    auto cid = entities.add_component(eid, character_ref);

    return {
        eid,
        &entities.get_component_by_id<component::character_ref>(cid),
        &entities.get_component_by_id<component::sprite>(sid),
    };
}
