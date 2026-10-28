#include "../src/World.h"
#include "TempPaths.h"
#include <cassert>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>

int main()
{
  std::srand(12345);
  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");

  World world(64, 64);
  world.setParticle(20, 30, db.getId("Wood"));
  world.setParticle(19, 30, db.getId("Wall"));
  world.setParticle(21, 30, db.getId("Wall"));
  world.setParticle(20, 29, db.getId("Sand"));
  world.update(1.0f / 60);
  assert(world.getParticle(20, 30).matId == db.getId("Wood"));

  world.clear();
  world.setParticle(10, 10, db.getId("Life"));
  world.setParticle(11, 10, db.getId("Life"));
  world.setParticle(12, 10, db.getId("Life"));
  for (int i = 0; i < 6; ++i)
    world.update(1.0f / 60);
  assert(world.getParticle(11, 9).matId == db.getId("Life"));
  assert(world.getParticle(11, 10).matId == db.getId("Life"));
  assert(world.getParticle(11, 11).matId == db.getId("Life"));
  assert(world.getParticle(10, 10).matId == 0);

  world.clear();
  world.setParticle(10, 20, db.getId("Water"));
  world.setParticle(11, 20, db.getId("Lava"));
  world.update(1.0f / 60);
  bool obsidianFound = false;
  for (int y = 19; y <= 21; ++y)
    for (int x = 9; x <= 12; ++x)
      obsidianFound |= world.getParticle(x, y).matId == db.getId("Obsidian");
  assert(obsidianFound);

  assert(!world.rainEnabled);
  world.clear();
  world.setParticle(0, 63, db.getId("Salt"));
  world.setParticle(1, 63, db.getId("Water"));
  world.update(1.0f / 60);
  assert(world.getParticle(1, 63).matId == db.getId("Saltwater"));

  world.clear();
  world.setParticle(0, 63, db.getId("Lava"));
  world.setParticle(1, 63, db.getId("Sand"));
  world.update(1.0f / 60);
  assert(world.getParticle(1, 63).matId == db.getId("MoltenGlass"));

  world.clear();
  world.setParticle(4, 63, db.getId("Ice"));
  assert(world.getTemperature(4, 63) < -0.6f);
  world.changeTemperature(4, 63, 1.0f);
  assert(world.getTemperature(4, 63) > 0.2f);
  bool melted = false;
  for (int i = 0; i < 60; ++i)
  {
    world.update(1.0f / 60.0f);
    for (int x = 0; x < 64; ++x)
      melted |= world.getParticle(x, 63).matId == db.getId("Water");
  }
  assert(melted);

  world.clear();
  world.setParticle(4, 63, db.getId("Wall"));
  world.setParticle(6, 63, db.getId("Wall"));
  world.setParticle(5, 63, db.getId("Lava"));
  assert(world.getTemperature(5, 63) > 0.9f);
  world.changeTemperature(5, 63, -0.88f);
  for (int i = 0; i < 90; ++i)
    world.update(1.0f / 60.0f);
  assert(world.getParticle(5, 63).matId == db.getId("Basalt"));

  world.clear();
  world.setParticle(3, 63, db.getId("Acid"));
  world.setParticle(4, 63, db.getId("Metal"));
  world.update(1.0f / 60.0f);
  assert(world.getParticle(4, 63).matId == db.getId("Rust"));

  world.clear();
  world.setParticle(22, 20, db.getId("Wood"));
  world.getParticleRef(22, 20).anchored = true;
  world.changeTemperature(22, 20, 0.8f);
  bool ignitionFound = false;
  for (int i = 0; i < 70; ++i)
  {
    world.update(1.0f / 60.0f);
    ignitionFound |= world.getParticle(22, 20).matId == db.getId("Fire");
  }
  assert(ignitionFound);

  world.clear();
  for (int x = 0; x < 64; ++x)
    world.setParticle(x, 31, db.getId("Wall"));
  for (int y = 25; y <= 30; ++y)
    world.setParticle(12, y, db.getId("Wall"));
  world.setParticle(10, 30, db.getId("Water"));
  for (int i = 0; i < 12; ++i)
    world.update(1.0f / 60);
  for (int x = 13; x < 63; ++x)
    for (int y = 25; y <= 30; ++y)
      assert(world.getParticle(x, y).matId != db.getId("Water"));

  world.clear();
  world.setParticle(0, 62, db.getId("Wall"));
  world.setParticle(1, 62, db.getId("Wall"));
  world.setParticle(0, 63, db.getId("Fire"));
  world.setParticle(1, 63, db.getId("Clay"));
  world.update(1.0f / 60);
  assert(world.getParticle(1, 63).matId == db.getId("Brick"));
  for (int i = 0; i < 20; ++i)
    world.update(1.0f / 60);
  assert(world.getParticle(1, 63).matId == db.getId("Brick"));

  world.clear();
  for (int x = 0; x < world.getWidth(); ++x)
    for (int y = 31; y < world.getHeight(); ++y)
      world.setParticle(x, y, db.getId("Soil"));
  world.setParticle(30, 30, db.getId("Water"));
  world.setParticle(29, 30, db.getId("Wall"));
  world.setParticle(31, 30, db.getId("Wall"));
  for (int i = 0; i < 65; ++i)
    world.update(1.0f / 60);
  assert(world.getSoilSaturation(30, 31) > 0.05f);
  world.changeNutrients(30, 30, 0.9f);
  world.changeNutrients(30, 31, 0.9f);
  world.changeHumidity(30, 30, 0.5f);
  world.setParticle(30, 30, db.getId("Seed"));
  for (int i = 0; i < 1200; ++i)
    world.update(1.0f / 60);
  bool germinated = false;
  for (int y = 18; y <= 30; ++y)
    for (int x = 23; x <= 36; ++x)
      germinated |= world.getParticle(x, y).matId == db.getId("Wood");
  assert(germinated);

  world.clear();
  world.createEcosystem();
  auto stats = world.getStats();
  assert(stats.plants > 0);
  assert(stats.particles > 0);
  world.rainEnabled = true;
  world.dayNightEnabled = false;
  world.cloudiness = 0.81f;
  world.wind = 1.42f;
  world.daylight = 0.23f;
  assert(world.save(testTempFile("matterbloom-smoke.agw")));
  world.clear();
  assert(world.load(testTempFile("matterbloom-smoke.agw")));
  auto restored = world.getStats();
  assert(restored.particles == stats.particles);
  assert(world.rainEnabled);
  assert(!world.dayNightEnabled);
  assert(world.getBiome() == World::Biome::Temperate);
  assert(std::abs(world.cloudiness - 0.81f) < 0.0001f);
  assert(std::abs(world.wind - 1.42f) < 0.0001f);
  assert(std::abs(world.daylight - 0.23f) < 0.0001f);

  {
    std::fstream file(testTempFile("matterbloom-smoke.agw"),
                      std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(-static_cast<std::streamoff>(sizeof(std::uint32_t) * 2), std::ios::end);
    const std::uint32_t legacyCount = 1;
    const std::uint32_t emitterCount = 0;
    const std::array<char, 7 * sizeof(float) + 2 * sizeof(int)> legacyData{};
    file.write(reinterpret_cast<const char *>(&legacyCount), sizeof(legacyCount));
    file.write(legacyData.data(), static_cast<std::streamsize>(legacyData.size()));
    file.write(reinterpret_cast<const char *>(&emitterCount), sizeof(emitterCount));
    assert(static_cast<bool>(file));
  }
  world.clear();
  assert(world.load(testTempFile("matterbloom-smoke.agw")));
  assert(world.getStats().particles == stats.particles);

  const auto original = world.captureSnapshot();
  world.setParticle(5, 5, db.getId("Lava"));
  assert(world.getParticle(5, 5).matId == db.getId("Lava"));
  world.restoreSnapshot(original);
  assert(world.getParticle(5, 5).matId == original.grid[world.getIndex(5, 5)].matId);

  {
    std::ofstream legacy(testTempFile("matterbloom-legacy.agw"), std::ios::binary);
    const char magic[8] = {'A', 'G', 'W', 'O', 'R', 'L', 'D', '2'};
    const std::uint32_t width = 64, height = 64, creatures = 0;
    legacy.write(magic, sizeof(magic));
    legacy.write(reinterpret_cast<const char *>(&width), sizeof(width));
    legacy.write(reinterpret_cast<const char *>(&height), sizeof(height));
    legacy.write(reinterpret_cast<const char *>(&original.elapsedTime), sizeof(float));
    for (const auto &particle : original.grid)
    {
      legacy.write(reinterpret_cast<const char *>(&particle.matId), sizeof(int));
      legacy.write(reinterpret_cast<const char *>(&particle.age), sizeof(int));
      legacy.write(reinterpret_cast<const char *>(&particle.lifetime_counter), sizeof(float));
    }
    for (const auto *buffer : {&original.temperature, &original.humidity, &original.nutrients})
      legacy.write(reinterpret_cast<const char *>(buffer->data()),
                   static_cast<std::streamsize>(buffer->size() * sizeof(float)));
    legacy.write(reinterpret_cast<const char *>(&creatures), sizeof(creatures));
    assert(static_cast<bool>(legacy));
  }
  world.clear();
  assert(world.load(testTempFile("matterbloom-legacy.agw")));
  assert(world.getStats().particles == stats.particles);
  assert(!world.rainEnabled);
  assert(world.dayNightEnabled);

  World second(64, 64);
  world.createEcosystem(World::Biome::Arid, 98765);
  second.createEcosystem(World::Biome::Arid, 98765);
  assert(world.getBiome() == World::Biome::Arid);
  assert(world.getSeed() == 98765);
  int sandCount = 0;
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x)
    {
      const auto a = world.getParticle(x, y);
      const auto b = second.getParticle(x, y);
      assert(a.matId == b.matId);
      assert(a.color.r == b.color.r && a.color.g == b.color.g && a.color.b == b.color.b);
      sandCount += a.matId == db.getId("Sand");
    }
  assert(sandCount > 100);
  assert(world.save(testTempFile("matterbloom-arid.agw")));
  world.clear();
  assert(world.load(testTempFile("matterbloom-arid.agw")));
  assert(world.getBiome() == World::Biome::Arid && world.getSeed() == 98765);

  world.createEcosystem(World::Biome::Wetland, 41891);
  assert(world.getBiome() == World::Biome::Wetland);
  assert(world.getSeed() == 41891);
  assert(!world.rainEnabled);
  assert(world.cloudiness > 0.5f);
  bool wetlandVegetation = false;
  for (int y = 0; y < world.getHeight(); ++y)
    for (int x = 0; x < world.getWidth(); ++x)
    {
      const int material = world.getParticle(x, y).matId;
      wetlandVegetation |= material == db.getId("Grass") ||
                           material == db.getId("Algae");
    }
  assert(wetlandVegetation);
  world.dayNightEnabled = false;
  world.daylight = 0.34f;
  world.update(1.0f / 60.0f);
  assert(world.daylight == 0.34f);

  sf::RenderWindow fakeWindow;
  world.clear();
  world.renderMode = World::RenderMode::Normal;
  world.setGraphicsQuality(0);
  world.daylight = 0.05f;
  world.render(fakeWindow);
  int nightSky = 0;
  for (int x = 0; x < 64; ++x)
    nightSky += sf::Texture::lastPixels[4 * world.getIndex(x, 2)];
  world.daylight = 0.95f;
  world.render(fakeWindow);
  int daySky = 0;
  for (int x = 0; x < 64; ++x)
    daySky += sf::Texture::lastPixels[4 * world.getIndex(x, 2)];
  assert(daySky > nightSky);
  world.setParticle(30, 8, db.getId("Fire"));
  world.daylight = 0.08f;
  world.render(fakeWindow);
  const auto noGlow = sf::Texture::lastPixels[4 * world.getIndex(28, 8)];
  world.setGraphicsQuality(2);
  world.render(fakeWindow);
  assert(sf::Texture::lastPixels[4 * world.getIndex(28, 8)] > noGlow);

  world.clear();
  for (int x = 8; x < 56; ++x)
    for (int y = 8; y < 56; ++y)
      world.setParticle(x, y, (x + y) % 2 ? db.getId("Lava") : db.getId("Water"));
  for (int i = 0; i < 200; ++i)
    world.update(1.0f / 60);
  assert(world.getStats().particles <= 64 * 64);
  std::cout << "World simulation smoke tests passed\n";
}
