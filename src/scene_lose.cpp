#include "scene_lose.hpp"

#include "scene_mainmenu.hpp"

#include "ember/engine.hpp"
#include "ember/vdom.hpp"

scene_lose::scene_lose(ember::engine& engine, ember::scene* prev)
    : scene(engine), gui_state{engine.lua.create_table()} {
    gui_state["goto_menu"] = [this]{ goto_menu(); };
}

void scene_lose::init() {
}

void scene_lose::tick(float delta) {}

void scene_lose::render() {}

auto scene_lose::handle_game_input(const SDL_Event& event) -> bool {
    return false;
}

auto scene_lose::render_gui() -> sol::table {
    return ember::vdom::create_element(engine->lua, "gui.scene_lose.root", gui_state, engine->lua.create_table());
}

void scene_lose::goto_menu() {
    engine->queue_transition<scene_mainmenu>();
}
