#pragma once

#include <vector>
#include <string>

struct attack_pattern {
    int x;
    int y;
};

struct character {
    int max_health;
    int health;
    int power;
    std::string portrait;
    std::vector<attack_pattern> attack_patterns;
    bool returning;
};
