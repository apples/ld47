#pragma once

#include "ember/scene.hpp"

#include <sol.hpp>

class scene_lose final : public ember::scene {
public:
    scene_lose(ember::engine& eng, ember::scene* prev);

    virtual void init() override;
    virtual void tick(float delta) override;
    virtual void render() override;
    virtual auto handle_game_input(const SDL_Event& event) -> bool override;
    virtual auto render_gui() -> sol::table override;

private:
    void goto_menu();

    sol::table gui_state;
};
