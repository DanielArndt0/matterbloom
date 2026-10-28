#include "MaterialDB.h"
#include "World.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

int main()
{
  std::srand(1130);
  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");
  const int granite = db.getId("Granite");
  const int gravel = db.getId("Gravel");
  const int concrete = db.getId("Concrete");
  assert(granite && gravel && concrete);

  World falling(120, 120);
  falling.setGravityScale(1.0f);
  falling.setParticle(25, 10, granite);
  for (int i = 0; i < 12; ++i)
    falling.update(1.0f / 60.0f);
  int graniteY = -1;
  for (int y = 0; y < falling.getHeight(); ++y)
    if (falling.getParticle(25, y).matId == granite)
      graniteY = y;
  assert(graniteY >= 28);

  falling.clear();
  falling.setParticle(25, 10, gravel);
  for (int i = 0; i < 12; ++i)
    falling.update(1.0f / 60.0f);
  int gravelY = -1;
  for (int y = 0; y < falling.getHeight(); ++y)
    if (falling.getParticle(25, y).matId == gravel)
      gravelY = y;
  assert(gravelY >= 19);

  falling.clear();
  for (int y = 10; y < 14; ++y)
    for (int x = 40; x < 45; ++x)
      falling.setParticle(x, y, concrete);
  for (int i = 0; i < 12; ++i)
    falling.update(1.0f / 60.0f);
  int count = 0, top = 120, bottom = 0, left = 120, right = 0;
  for (int y = 0; y < 120; ++y)
    for (int x = 0; x < 120; ++x)
      if (falling.getParticle(x, y).matId == concrete)
      {
        ++count;
        top = std::min(top, y);
        bottom = std::max(bottom, y);
        left = std::min(left, x);
        right = std::max(right, x);
      }
  assert(count == 20 && bottom - top == 3 && right - left == 4);
  assert(top >= 28);

  World fracture(96, 96);
  for (int y = 28; y < 45; ++y)
    for (int x = 27; x < 48; ++x)
      fracture.setParticle(x, y, concrete);
  fracture.applyForce(37, 36, 7.0f, 9.0f,
                      World::ForceMode::Explosion);
  int cracks = 0;
  int rubble = 0;
  for (int y = 28; y < 45; ++y)
    for (int x = 27; x < 48; ++x)
    {
      const auto p = fracture.getParticle(x, y);
      if (p.matId == concrete && p.damage > 0.1f)
        ++cracks;
      if (p.matId == gravel)
        ++rubble;
    }
  assert(cracks > 2 && rubble > 0);

  std::cout << "Matterbloom accelerated motion and fracture passed\n";
}
