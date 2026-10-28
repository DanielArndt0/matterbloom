#include "../src/World.h"
#include "../src/Units.h"
#include "TempPaths.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
  std::srand(20261027);
  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");

  assert(std::abs(Units::celsius(0.0f) - 20.0f) < 0.01f);
  assert(std::abs(Units::celsius(-0.46f)) < 0.01f);
  assert(std::abs(Units::temperature(0.0f, Units::Temperature::Fahrenheit) - 68.0f) < 0.01f);
  assert(std::abs(Units::temperature(0.0f, Units::Temperature::Kelvin) - 293.15f) < 0.01f);
  assert(std::abs(Units::pressure(0.0f, Units::Pressure::Kilopascal) - 101.325f) < 0.01f);
  assert(std::abs(Units::pressure(0.0f, Units::Pressure::Bar) - 1.01325f) < 0.001f);
  assert(std::abs(Units::humidityPercent(0.74f) - 74.0f) < 0.001f);
  assert(Units::clock24(19.5f) == "19:30");

  World world(128, 128);
  world.dayNightEnabled = false;
  world.setClockHour(19.5f);
  const float frozenHour = world.getClockHour();
  for (int tick = 0; tick < 45; ++tick)
    world.update(1.0f / 60.0f);
  assert(std::abs(world.getClockHour() - frozenHour) < 0.001f);
  world.dayNightEnabled = true;
  world.dayLengthSeconds = 180.0f;
  world.update(1.0f / 60.0f);
  assert(world.getClockHour() > frozenHour);

  const int water = db.getId("Water");
  const int oil = db.getId("Oil");
  const int resin = db.getId("Resin");
  const int rock = db.getId("Stone");

  world.clear();
  for (int y = 35; y < 48; ++y)
    for (int x = 28; x < 42; ++x)
      world.setParticle(x, y, resin);
  for (int tick = 0; tick < 24; ++tick)
    world.update(1.0f / 60.0f);
  bool observedLiquidPressure = false;
  for (int y = 25; y < 80; ++y)
    for (int x = 18; x < 60; ++x)
      observedLiquidPressure |= world.getParticle(x, y).matId == resin &&
                                world.getPressure(x, y) > 0.001f;
  assert(observedLiquidPressure);

  world.clear();
  for (int x = 8; x < 120; ++x)
    world.setParticle(x, 100, rock);
  world.getParticleRef(8, 100).anchored = true;
  world.getParticleRef(119, 100).anchored = true;
  for (int y = 70; y < 78; ++y)
    for (int x = 36; x < 48; ++x)
      world.setParticle(x, y, water);
  for (int tick = 0; tick < 90; ++tick)
    world.update(1.0f / 60.0f);
  bool flowedSideways = false;
  for (int y = 65; y < 100; ++y)
    for (int x = 49; x < 110; ++x)
      flowedSideways |= world.getParticle(x, y).matId == water;
  assert(flowedSideways);

  world.clear();
  const int emission = world.emitMaterial(50, 40, rock, 12, 8.0f, 1.0f, -0.3f);
  assert(emission > 0);
  bool hasImpulse = false;
  for (int y = 36; y < 46; ++y)
    for (int x = 45; x < 57; ++x)
      hasImpulse |= world.getParticle(x, y).matId == rock &&
                    world.getParticle(x, y).velocityX > 1.0f;
  assert(hasImpulse);
  world.clear();
  assert(world.applyForce(60, 40, 9.0f, 3.0f, World::ForceMode::Jet,
                          1.0f, 0.0f, oil, true) > 0);
  bool firedOil = false;
  for (int y = 30; y < 55; ++y)
    for (int x = 50; x < 85; ++x)
      firedOil |= world.getParticle(x, y).matId == oil;
  assert(firedOil);

  world.dayLengthSeconds = 330.0f;
  world.setClockHour(8.75f);
  world.setParticle(100, 15, water);
  world.applyForce(100, 15, 2.0f, 0.9f, World::ForceMode::Launch, -1.0f, 0.0f);
  const auto before = world.getParticle(100, 15);
  const auto filename = testTempFile("matterbloom-010-flow.mbw");
  assert(world.save(filename));
  World restored(64, 64);
  assert(restored.load(filename));
  assert(std::abs(restored.dayLengthSeconds - 330.0f) < 0.001f);
  assert(std::abs(restored.getClockHour() - 8.75f) < 0.001f);
  assert(std::abs(restored.getParticle(100, 15).velocityX - before.velocityX) < 0.0001f);
  std::ifstream file(filename, std::ios::binary);
  char signature[8];
  file.read(signature, sizeof(signature));
  assert(std::string(signature, signature + 8) == "MBWORLD9");
  std::cout << "Matterbloom fluid, tools, clock, units, and saves passed\n";
}
