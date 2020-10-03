#pragma once

#include <vector>
#include <string>

struct movement {
    int x;
    int y;
};

struct movement_card {
    std::string name;
    std::vector<movement> movements;
};

auto load_movement_cards(std::string filename = "/data/movement.json") -> std::vector<movement_card>;