#pragma once

#include <SFML/Graphics.hpp>

class World;

struct Entity
{
  float x, y;
  float vx, vy;
  int health;
  int type;

  int width = 3;
  int height = 3;

  float stateTimer = 0.0f;
  int behaviorState = 0;
  float age = 0.0f;
  float hungerTimer = 0.0f;
  float reproductionCooldown = 10.0f;

  Entity(float startX, float startY, int type);

  void update(World &world, float dt);
  void render(sf::RenderWindow &window, float scaleX, float scaleY,
              float offsetX, float offsetY) const;
};
