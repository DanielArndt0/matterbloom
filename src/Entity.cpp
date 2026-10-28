#include "Entity.h"
#include "MaterialDB.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

Entity::Entity(float startX, float startY, int type)
    : x(startX), y(startY), vx(0), vy(0), health(100), type(type)
{
  if (type == 0)
  {
    width = 4;
    height = 3;
  }
}

void Entity::update(World &world, float dt)
{
  if (health <= 0)
    return;

  age += dt;
  hungerTimer += dt;
  reproductionCooldown -= dt;
  if (hungerTimer >= 3.0f)
  {
    health -= 2;
    hungerTimer = 0.0f;
  }

  vy += 980.0f * dt;

  float nextX = x + vx * dt;
  float nextY = y + vy * dt;

  bool onGround = false;

  int checkY = static_cast<int>(nextY) + height;
  if (checkY >= world.getHeight())
  {
    nextY = world.getHeight() - height - 1;
    vy = 0;
    onGround = true;
  }
  else
  {

    for (int i = 0; i < width; ++i)
    {
      int checkX = static_cast<int>(nextX) + i;
      if (world.inBounds(checkX, checkY))
      {
        Particle p = world.getParticle(checkX, checkY);
        if (p.matId != 0)
        {
          const MaterialDef &def =
              MaterialDB::getInstance().getMaterial(p.matId);
          if (def.density > 1)
          {
            nextY = static_cast<float>(checkY - height);
            vy = 0;
            onGround = true;
            break;
          }
        }
      }
    }
  }

  if (nextX < 0)
  {
    nextX = 0;
    vx = -vx;
  }
  if (nextX + width >= world.getWidth())
  {
    nextX = world.getWidth() - width - 1;
    vx = -vx;
  }

  int sideX =
      vx > 0 ? static_cast<int>(nextX) + width : static_cast<int>(nextX) - 1;
  for (int j = 0; j < height; ++j)
  {
    int checkY2 = static_cast<int>(nextY) + j;
    if (world.inBounds(sideX, checkY2))
    {
      Particle p = world.getParticle(sideX, checkY2);
      if (p.matId != 0 &&
          MaterialDB::getInstance().getMaterial(p.matId).density > 1)
      {
        vx = -vx;
        if (vx > 0)
          nextX = sideX + 1;
        else
          nextX = sideX - width;
        break;
      }
    }
  }

  x = nextX;
  y = nextY;

  if (type == 0)
  {
    const auto &db = MaterialDB::getInstance();
    const int plantId = db.getId("Plant");
    const int mossId = db.getId("Moss");
    const int cx = std::clamp(static_cast<int>(x) + width / 2, 0, world.getWidth() - 1);
    const int cy = std::clamp(static_cast<int>(y) + height / 2, 0, world.getHeight() - 1);
    bool seesFood = false;
    for (int dx = -14; dx <= 14 && !seesFood; ++dx)
    {
      for (int dy = -5; dy <= 5; ++dy)
      {
        if (!world.inBounds(cx + dx, cy + dy))
          continue;
        const int id = world.getParticle(cx + dx, cy + dy).matId;
        if (id != plantId && id != mossId)
          continue;
        seesFood = true;
        vx = dx > 0 ? 28.0f : dx < 0 ? -28.0f
                                     : 0.0f;
        if (std::abs(dx) <= 3 && std::abs(dy) <= 3)
        {
          world.setParticle(cx + dx, cy + dy, db.getId("Compost"));
          health = std::min(150, health + 12);
        }
        break;
      }
    }
    stateTimer -= dt;
    if (!seesFood && stateTimer <= 0)
    {

      if (rand() % 100 < 80)
      {
        behaviorState = 0;

        vx = ((rand() % 100) / 100.0f - 0.5f) * 100.0f;
        if (onGround && rand() % 100 < 10)
          vy = -200.0f;
        stateTimer = ((rand() % 200) / 100.0f) + 0.5f;
      }
      else
      {
        behaviorState = 1;
        vx = 0.0f;
        stateTimer = 1.0f;
      }
    }

    float localTemp =
        world.getTemperature(static_cast<int>(x), static_cast<int>(y));
    if (localTemp > 0.6f)
      health -= 1;
  }
}

void Entity::render(sf::RenderWindow &window, float scaleX,
                    float scaleY, float offsetX, float offsetY) const
{
  if (health <= 0)
    return;

  sf::RectangleShape rect(sf::Vector2f(width * scaleX, height * scaleY));
  rect.setPosition(offsetX + x * scaleX, offsetY + y * scaleY);

  if (type == 0)
  {

    if (health < 50 && (rand() % 10) < 2)
      rect.setFillColor(sf::Color::Red);
    else
      rect.setFillColor(sf::Color(50, 200, 50));
  }

  window.draw(rect);
}
