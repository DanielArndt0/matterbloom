#include "../src/World.h"
#include "TempPaths.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>

int main()
{
  std::srand(42);
  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");

  World world(128, 128);
  const int rock = db.getId("Stone");
  const int wall = db.getId("Wall");
  const int metal = db.getId("Metal");
  const int water = db.getId("Water");
  world.setParticle(60, 44, rock);
  assert(world.applyForce(60, 44, 3.0f, 4.0f,
                          World::ForceMode::Launch, 1.0f, -0.25f) > 0);
  assert(world.getParticle(60, 44).velocityX > 0.5f);
  world.update(1.0f / 60.0f);
  bool moved = false;
  for (int y = 35; y <= 48; ++y)
    for (int x = 61; x < 75; ++x)
      moved |= world.getParticle(x, y).matId == rock;
  assert(moved);

  world.clear();
  world.setParticle(30, 30, wall);
  world.applyForce(30, 30, 2.0f, 4.0f, World::ForceMode::Anchor);
  assert(world.getParticle(30, 30).anchored);
  world.applyForce(30, 30, 3.0f, 0.5f, World::ForceMode::Launch);
  assert(world.getParticle(30, 30).velocityX == 0.0f);
  world.applyForce(30, 30, 2.0f, 4.0f, World::ForceMode::Release);
  assert(!world.getParticle(30, 30).anchored);

  world.clear();
  world.setParticle(60, 62, metal);
  world.applyForce(54, 62, 16.0f, 3.0f, World::ForceMode::Attraction);
  assert(world.getParticle(60, 62).velocityX < 0.0f);

  world.clear();
  world.setParticle(48, 38, db.getId("Smoke"));
  const float oldWind = world.wind;
  world.applyForce(48, 38, 5.0f, 4.0f,
                   World::ForceMode::Fan, 1.0f, 0.0f);
  assert(world.getParticle(48, 38).velocityX > 0.0f);
  assert(world.wind > oldWind);

  world.clear();
  for (int x = 25; x < 33; ++x)
    world.setParticle(x, 25, wall);
  world.applyForce(29, 25, 3.0f, 1.2f,
                   World::ForceMode::Impulse);
  bool structuralMovement = false;
  for (int step = 0; step < 45; ++step)
  {
    world.update(1.0f / 60.0f);
    for (int y = 26; y < 36; ++y)
      for (int x = 23; x < 35; ++x)
        structuralMovement |= world.getParticle(x, y).matId == wall;
  }
  assert(structuralMovement);

  world.clear();
  world.applyForce(70, 30, 14.0f, 5.0f, World::ForceMode::Jet, 1.0f, 0.0f);
  int drops = 0;
  for (int y = 20; y < 40; ++y)
    for (int x = 60; x < 85; ++x)
      drops += world.getParticle(x, y).matId == water;
  assert(drops > 0);

  world.clear();
  for (int y = 45; y < 58; ++y)
    for (int x = 55; x < 68; ++x)
      world.setParticle(x, y, wall);
  const auto before = world.getParticle(60, 50).matId;
  assert(world.applyForce(60, 50, 12.0f, 10.0f,
                          World::ForceMode::Explosion) > 20);
  assert(before != world.getParticle(60, 50).matId ||
         world.getParticle(60, 50).damage > 0.15f);
  assert(world.getTemperature(60, 50) > 0.0f);
  assert(!world.getEvents().empty());

  world.clear();
  world.setParticle(70, 70, wall);
  world.applyForce(70, 70, 1.0f, 3.0f, World::ForceMode::Anchor);
  world.setParticle(32, 35, rock);
  world.applyForce(32, 35, 2.0f, 1.8f,
                   World::ForceMode::Launch, -1.0f, -1.0f);
  const auto original = world.getParticle(32, 35);
  assert(world.save(testTempFile("matter-bloom-090-physics.mbw")));
  World saved(64, 64);
  assert(saved.load(testTempFile("matter-bloom-090-physics.mbw")));
  assert(saved.getParticle(70, 70).anchored);
  assert(std::abs(saved.getParticle(32, 35).velocityX - original.velocityX) < 0.0001f);

  const auto snapshot = world.captureSnapshot();
  world.clear();
  world.restoreSnapshot(snapshot);
  assert(world.getParticle(70, 70).anchored);

  std::cout << "Matterbloom physics and saves passed\n";
}
