#include "movement.hpp"

#include "ember/json.hpp"

#include <fstream>

auto load_movement_cards(std::string filename) -> std::vector<movement_card> {
    auto file = std::ifstream(filename);
    auto json = nlohmann::json{};
    file >> json;

    std::vector<movement_card> movement_cards;
    for (const auto& movement_card : json) {
        std::vector<movement> movements;
        for (const auto& movement : movement_card["movements"]) {
            movements.push_back({movement["x"].get<int>(), movement["y"].get<int>(), movement["attack"].get<bool>()});
        }
        movement_cards.push_back({movement_card["name"].get<std::string>(), movements});
    }

    return movement_cards;
}