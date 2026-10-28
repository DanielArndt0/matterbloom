#pragma once

#include <SFML/Graphics/Color.hpp>

struct Particle
{
    int matId = 0;
    sf::Color color = sf::Color::Transparent;
    bool updated_this_frame = false;
    float lifetime_counter = 0;
    int age = 0;
    int burnResidue = 0;
    float volume = 1.0f;
    unsigned char species = 0;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float subcellX = 0.0f;
    float subcellY = 0.0f;
    float damage = 0.0f;
    bool anchored = false;
};
