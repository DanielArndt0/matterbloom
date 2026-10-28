#include "../src/World.h"
#include "TempPaths.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main()
{
  std::srand(1100);
  auto &materials = MaterialDB::getInstance();
  materials.loadMaterials("");
  materials.loadReactions("");

  World falling(96, 96);
  const int granite = materials.getId("Granite");
  const int concrete = materials.getId("Concrete");
  const int metal = materials.getId("Metal");
  falling.setParticle(22, 10, granite);
  for (int step = 0; step < 24; ++step)
    falling.update(1.0f / 60.0f);
  assert(falling.getParticle(22, 10).matId == 0);
  bool landedLower = false;
  for (int y = 11; y < falling.getHeight(); ++y)
    landedLower |= falling.getParticle(22, y).matId == granite;
  assert(landedLower);

  falling.clear();
  for (int y = 10; y < 14; ++y)
    for (int x = 32; x < 38; ++x)
      falling.setParticle(x, y, concrete);
  for (int step = 0; step < 20; ++step)
    falling.update(1.0f / 60.0f);
  int solidCount = 0;
  int lowest = 0, highest = 95;
  for (int y = 0; y < 96; ++y)
    for (int x = 0; x < 96; ++x)
      if (falling.getParticle(x, y).matId == concrete)
      {
        ++solidCount;
        lowest = std::max(lowest, y);
        highest = std::min(highest, y);
      }
  assert(solidCount == 24);
  assert(lowest - highest == 3);
  assert(highest > 10);

  falling.clear();
  falling.setParticle(55, 50, metal);
  falling.applyForce(55, 50, 2.0f, 8.0f, World::ForceMode::Launch,
                     0.0f, -1.0f, 0, true);
  falling.update(1.0f / 60.0f);
  bool rose = false;
  for (int y = 42; y < 50; ++y)
    rose |= falling.getParticle(55, y).matId == metal;
  assert(rose);

  falling.clear();
  falling.setParticle(30, 30, metal);
  falling.getParticleRef(30, 30).anchored = true;
  for (int step = 0; step < 15; ++step)
    falling.update(1.0f / 60.0f);
  assert(falling.getParticle(30, 30).matId == metal);
  falling.applyForce(30, 30, 2.0f, 3.0f, World::ForceMode::Release);
  for (int step = 0; step < 15; ++step)
    falling.update(1.0f / 60.0f);
  assert(falling.getParticle(30, 30).matId == 0);

  World lab(128, 128);
  lab.createLaboratory(112233);
  assert(lab.getEnvironment() == World::Environment::Laboratory);
  assert(!lab.automaticWeather && !lab.rainEnabled && !lab.dayNightEnabled);
  assert(lab.getParticle(64, 64).matId == 0);
  assert(lab.getParticle(64, 127).anchored);
  World::Emitter machine;
  machine.x = 40;
  machine.y = 45;
  machine.mode = World::ForceMode::Launch;
  machine.materialId = granite;
  machine.directionX = 1.0f;
  machine.directionY = -0.15f;
  machine.intensity = 5.0f;
  assert(lab.installEmitter(machine));
  assert(!lab.installEmitter(machine));
  for (int step = 0; step < 9; ++step)
    lab.update(1.0f / 60.0f);
  int launched = 0;
  for (int y = 25; y < 65; ++y)
    for (int x = 30; x < 100; ++x)
      launched += lab.getParticle(x, y).matId == granite;
  assert(launched > 0);
  assert(lab.getEmitterCount() == 1);
  const auto savedPath = testTempFile("matterbloom-011-laboratory.mbw");
  assert(lab.save(savedPath));
  World loaded(64, 64);
  assert(loaded.load(savedPath));
  assert(loaded.getEnvironment() == World::Environment::Laboratory);
  assert(loaded.getEmitterCount() == 1);
  assert(loaded.getParticle(64, 127).anchored);

  World leaning(128, 128);
  leaning.createLaboratory(11);
  for (int y = 35; y <= 125; ++y)
  {
    const int x = 18 + (125 - y) * 65 / 90;
    leaning.setParticle(x, y, granite);
    leaning.setParticle(x + 1, y, granite);
  }
  int countBefore = 0;
  for (int y = 0; y < 128; ++y)
    for (int x = 0; x < 128; ++x)
      countBefore += leaning.getParticle(x, y).matId != 0;
  const int tipX = 18 + (125 - 35) * 65 / 90;
  for (int step = 0; step < 80; ++step)
    leaning.update(1.0f / 60.0f);
  assert(leaning.getParticle(tipX, 35).matId != granite);
  int countAfter = 0;
  for (int y = 0; y < 128; ++y)
    for (int x = 0; x < 128; ++x)
      countAfter += leaning.getParticle(x, y).matId != 0;
  assert(countAfter == countBefore);

  World stable(128, 128);
  stable.createLaboratory(13);
  for (int x = 35; x <= 78; ++x)
    stable.setParticle(x, 126, granite);
  for (int y = 90; y <= 126; ++y)
  {
    stable.setParticle(35, y, granite);
    stable.setParticle(78, y, granite);
  }
  for (int step = 0; step < 80; ++step)
    stable.update(1.0f / 60.0f);
  assert(stable.getParticle(35, 90).matId == granite);
  assert(stable.getParticle(78, 90).matId == granite);

  World fixed(128, 128);
  fixed.createLaboratory(12);
  for (int y = 35; y <= 125; ++y)
  {
    const int x = 18 + (125 - y) * 65 / 90;
    fixed.setParticle(x, y, granite);
    fixed.setParticle(x + 1, y, granite);
  }
  fixed.applyForce(tipX, 35, 3.0f, 1.0f, World::ForceMode::Anchor);
  for (int step = 0; step < 50; ++step)
    fixed.update(1.0f / 60.0f);
  assert(fixed.getParticle(tipX, 35).matId == granite);

  const auto snapshot = lab.captureSnapshot();
  lab.clear();
  lab.restoreSnapshot(snapshot);
  assert(lab.getEnvironment() == World::Environment::Laboratory);
  assert(lab.getEmitterCount() == 1);
  lab.clearEmitters();
  assert(lab.getEmitterCount() == 0);

  std::cout << "Matterbloom structural, laboratory, emitters and saves passed\n";
}
