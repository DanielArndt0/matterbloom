#include "../src/World.h"
#include "TempPaths.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main()
{
  std::srand(2026);
  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");

  World pond(128, 128);
  pond.setParticle(60, 62, db.getId("Soil"));
  pond.setParticle(60, 63, db.getId("Soil"));
  for (int y = 61; y <= 63; ++y)
  {
    pond.setParticle(59, y, db.getId("Wall"));
    pond.setParticle(61, y, db.getId("Wall"));
  }
  for (int x = 58; x <= 62; ++x)
    pond.setParticle(x, 64, db.getId("Wall"));
  pond.getParticleRef(58, 64).anchored = true;
  pond.getParticleRef(62, 64).anchored = true;
  pond.setParticle(60, 61, db.getId("Water"));
  const float before = pond.getSoilSaturation(60, 62);
  for (int tick = 0; tick < 80; ++tick)
    pond.update(1.0f / 60.0f);
  assert(pond.getSoilSaturation(60, 62) > before + 0.05f);
  assert(pond.getParticle(60, 61).volume < 1.0f);

  const auto saturated = pond.captureSnapshot();
  const float wetness = pond.getSoilSaturation(60, 62);
  assert(pond.save(testTempFile("matterbloom090-nature.mbw")));
  pond.clear();
  assert(pond.getSoilSaturation(60, 62) == 0.0f);
  pond.restoreSnapshot(saturated);
  assert(std::abs(pond.getSoilSaturation(60, 62) - wetness) < 0.0001f);

  World loaded(64, 64);
  assert(loaded.load(testTempFile("matterbloom090-nature.mbw")));
  assert(loaded.getWidth() == 128 && loaded.getHeight() == 128);
  assert(std::abs(loaded.getSoilSaturation(60, 62) - wetness) < 0.0001f);

  World deep(512, 768);
  deep.createEcosystem(World::Biome::Temperate, 31833);
  assert(deep.getWidth() == 512 && deep.getHeight() == 768);
  assert(deep.getGroundLevel() < 390);
  std::size_t deepMinerals = 0, deepStone = 0;
  for (int y = 450; y < deep.getHeight(); ++y)
    for (int x = 0; x < deep.getWidth(); ++x)
    {
      const int id = deep.getParticle(x, y).matId;
      deepMinerals += id == db.getId("IronOre") || id == db.getId("CopperOre") ||
                      id == db.getId("Coal") || id == db.getId("Quartz");
      deepStone += id == db.getId("Granite") || id == db.getId("Shale") ||
                   id == db.getId("Limestone");
    }
  assert(deepMinerals > 100);
  assert(deepStone > 1000);

  deep.dayNightEnabled = false;
  const float phase = deep.timeOfDay;
  const float daylight = deep.daylight;
  for (int tick = 0; tick < 120; ++tick)
    deep.update(1.0f / 60.0f);
  assert(deep.timeOfDay == phase);
  assert(deep.daylight == daylight);
  deep.dayNightEnabled = true;
  deep.update(1.0f / 60.0f);
  assert(deep.timeOfDay != phase);

  deep.createEcosystem(World::Biome::Wetland, 7761);
  const auto wetStats = deep.getStats();
  assert(wetStats.soilMoisture > 0.1f);
  assert(wetStats.cloudCover > 0.1f);
  deep.automaticWeather = false;
  deep.rainEnabled = false;
  for (int tick = 0; tick < 24; ++tick)
    deep.update(1.0f / 60.0f);
  assert(deep.getRainIntensity() == 0.0f);
  deep.rainEnabled = true;
  for (int tick = 0; tick < 36; ++tick)
    deep.update(1.0f / 60.0f);
  assert(deep.getRainIntensity() > 0.0f);

  std::cout << "Matterbloom nature tests passed\n";
}
