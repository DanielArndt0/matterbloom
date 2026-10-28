#include "World.h"
#include "Biology.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>
#include <utility>

namespace
{
  float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
  std::uint32_t hash32(std::uint32_t value)
  {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
  }
  float noise1D(float x, std::uint32_t seed)
  {
    const int left = static_cast<int>(std::floor(x));
    const float fraction = x - left;
    const float blend = fraction * fraction * (3.0f - 2.0f * fraction);
    const float first = static_cast<float>(hash32(static_cast<std::uint32_t>(left) ^ seed) & 65535u) / 65535.0f;
    const float second = static_cast<float>(hash32(static_cast<std::uint32_t>(left + 1) ^ seed) & 65535u) / 65535.0f;
    return first + (second - first) * blend;
  }
  float noise2D(float x, float y, std::uint32_t seed)
  {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float fx = x - ix, fy = y - iy;
    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const auto sample = [seed](int cx, int cy)
    {
      const std::uint32_t key = static_cast<std::uint32_t>(cx) * 73856093u ^
                                static_cast<std::uint32_t>(cy) * 19349663u ^ seed;
      return static_cast<float>(hash32(key) & 65535u) / 65535.0f;
    };
    const float upper = sample(ix, iy) * (1.0f - sx) + sample(ix + 1, iy) * sx;
    const float lower = sample(ix, iy + 1) * (1.0f - sx) + sample(ix + 1, iy + 1) * sx;
    return upper * (1.0f - sy) + lower * sy;
  }
  sf::Color mixColor(sf::Color a, sf::Color b, float weight)
  {
    const float t = clamp01(weight);
    return sf::Color(
        static_cast<sf::Uint8>(a.r + (b.r - a.r) * t),
        static_cast<sf::Uint8>(a.g + (b.g - a.g) * t),
        static_cast<sf::Uint8>(a.b + (b.b - a.b) * t));
  }
  bool isPlant(int id)
  {
    static const std::array<bool, 256> ids = []
    {
      std::array<bool, 256> result{};
      const auto &db = MaterialDB::getInstance();
      for (const char *name : {"Seed", "Wood", "Plant", "Moss", "Algae", "Grass",
                               "Flower", "PineSeed", "MangroveSeed", "PineNeedles",
                               "Mushroom", "Mycelium", "ShrubSeed", "Shrub", "Roots"})
      {
        const int material = db.getId(name);
        if (material > 0 && material < static_cast<int>(result.size()))
          result[static_cast<std::size_t>(material)] = true;
      }
      return result;
    }();
    return id >= 0 && id < static_cast<int>(ids.size()) &&
           ids[static_cast<std::size_t>(id)];
  }
}

World::World(int w, int h) : width(0), height(0) { resize(w, h); }

float World::getClockHour() const
{
  constexpr float radiansPerHour = 6.28318530718f / 24.0f;
  float hours = std::fmod(timeOfDay / radiansPerHour + 6.0f, 24.0f);
  return hours < 0.0f ? hours + 24.0f : hours;
}

void World::setClockHour(float hour)
{
  const float safe = std::fmod(std::max(0.0f, hour), 24.0f);
  timeOfDay = (safe - 6.0f) * (6.28318530718f / 24.0f);
  daylight = clamp01(0.5f + 0.5f * std::sin(timeOfDay));
}

void World::resize(int w, int h)
{
  width = std::clamp(w, 64, 1024);
  height = std::clamp(h, 64, 1024);
  const int count = width * height;
  grid.assign(count, Particle{});
  pixelData.assign(count * 4, 255);
  skyRows.resize(height);
  farHeight.resize(width);
  nearHeight.resize(width);
  cloudBand.resize(width);
  cloudWeight.resize(width);
  tempA.assign(count, 0.0f);
  tempB.assign(count, 0.0f);
  humidA.assign(count, 0.0f);
  humidB.assign(count, 0.0f);
  nutA.assign(count, 0.0f);
  nutB.assign(count, 0.0f);
  soilWater.assign(count, 0.0f);
  soilWaterNext.assign(count, 0.0f);
  pressureField.assign(count, 0.0f);
  structuralSupport.assign(count, 0);
  atmosphericVapor.assign((width + 15) / 16, 0.0f);
  atmosphericCloud.assign((width + 15) / 16, 0.0f);
  sprite = sf::Sprite();
  texture.create(width, height);
  sprite.setTexture(texture);
  chunksX = (width + CHUNK_SIZE - 1) / CHUNK_SIZE;
  chunksY = (height + CHUNK_SIZE - 1) / CHUNK_SIZE;
  chunks.assign(chunksX * chunksY, Chunk{});
  groundLevel = height > 512 ? 365 : static_cast<int>(height * 0.72f);
  hydrologyTickCounter = pressureTickCounter = 0;
  graphicsFrameCounter = 0;
  structuralTickCounter = 0;
  structuralActivityTicks = 0;
  emitterTickCounter = 0;
  for (auto &chunk : chunks)
    chunk.setDirty(true);
}

int World::getIndex(int x, int y) const { return y * width + x; }
bool World::inBounds(int x, int y) const
{
  return x >= 0 && y >= 0 && x < width && y < height;
}
void World::setScale(float x, float y) { sprite.setScale(x, y); }
void World::setPosition(float x, float y) { sprite.setPosition(x, y); }
void World::setBackgroundPan(float value) { backgroundPanWorld = value; }
void World::setGraphicsQuality(int quality) { graphicsQuality = std::clamp(quality, 0, 2); }
void World::setRenderTuning(bool glow, float lighting)
{
  heatGlowEnabled = glow;
  lightingStrength = std::clamp(lighting, 0.35f, 1.65f);
}
void World::setGravityScale(float scale)
{
  gravityScale = std::clamp(scale, 0.10f, 2.0f);
}
sf::Vector2f World::getSpriteScale() const { return sprite.getScale(); }
sf::Vector2f World::getSpritePosition() const { return sprite.getPosition(); }
Particle &World::getParticleRef(int x, int y) { return grid[getIndex(x, y)]; }
Particle World::getParticle(int x, int y) const
{
  return inBounds(x, y) ? grid[getIndex(x, y)] : Particle{};
}

void World::wakeAt(int x, int y)
{
  if (!inBounds(x, y))
    return;
  const int minCX = std::max(0, x - 1) / CHUNK_SIZE;
  const int maxCX = std::min(width - 1, x + 1) / CHUNK_SIZE;
  const int minCY = std::max(0, y - 1) / CHUNK_SIZE;
  const int maxCY = std::min(height - 1, y + 1) / CHUNK_SIZE;
  for (int cy = minCY; cy <= maxCY; ++cy)
    for (int cx = minCX; cx <= maxCX; ++cx)
      chunks[cy * chunksX + cx].setDirty(true);
}

void World::setParticle(int x, int y, int materialId)
{
  if (!inBounds(x, y))
    return;
  const auto &def = MaterialDB::getInstance().getMaterial(materialId);
  const int previousMaterial = grid[getIndex(x, y)].matId;
  if (previousMaterial == def.id && def.lifetime <= 0)
    return;
  if (isStructural(previousMaterial) || isStructural(def.id))
    structuralActivityTicks = 240;
  Particle particle;
  particle.matId = def.id;
  particle.color = def.baseColor;
  if (def.colorVariance > 0)
  {
    const int variance = def.colorVariance;
    const std::uint32_t variation = hash32(seed ^
                                           (static_cast<std::uint32_t>(x) * 73856093u) ^
                                           (static_cast<std::uint32_t>(y) * 19349663u) ^
                                           (static_cast<std::uint32_t>(def.id) * 83492791u));
    auto vary = [&](sf::Uint8 value, int shift)
    {
      return static_cast<sf::Uint8>(std::clamp(
          static_cast<int>(value) +
              static_cast<int>((variation >> shift) % (2 * variance + 1)) - variance,
          0, 255));
    };
    particle.color.r = vary(particle.color.r, 0);
    particle.color.g = vary(particle.color.g, 10);
    particle.color.b = vary(particle.color.b, 20);
  }
  particle.lifetime_counter = def.lifetime > 0
                                  ? def.lifetime + std::rand() % std::max(1, def.lifetime / 2)
                                  : -1.0f;
  if (def.name == "Fire")
  {
    const int previous = grid[getIndex(x, y)].matId;
    const auto &burning = MaterialDB::getInstance().getMaterial(previous);
    if (burning.flammability)
    {
      particle.burnResidue = previous == MaterialDB::getInstance().getId("Wood")
                                 ? MaterialDB::getInstance().getId("Charcoal")
                             : (previous == MaterialDB::getInstance().getId("Oil") ||
                                previous == MaterialDB::getInstance().getId("Fuel") ||
                                previous == MaterialDB::getInstance().getId("Hydrogen"))
                                 ? 0
                                 : MaterialDB::getInstance().getId("Ash");
    }
  }
  grid[getIndex(x, y)] = particle;
  soilWater[getIndex(x, y)] = std::min(soilWater[getIndex(x, y)], def.porosity);
  if (def.initialTemperature <= 1.0f)
  {
    const int index = getIndex(x, y);
    tempA[index] = tempB[index] = def.initialTemperature;
  }
  if (def.name == "Life")
    hasLife = true;
  wakeAt(x, y);
}

void World::swapParticles(int x1, int y1, int x2, int y2)
{
  const int first = getIndex(x1, y1), second = getIndex(x2, y2);
  std::swap(grid[first], grid[second]);
  std::swap(soilWater[first], soilWater[second]);
  auto &temperature = usingBufferA ? tempA : tempB;
  std::swap(temperature[first], temperature[second]);
  grid[getIndex(x2, y2)].updated_this_frame = true;
  wakeAt(x1, y1);
  wakeAt(x2, y2);
}

bool World::tryMove(int x, int y, int nx, int ny, const MaterialDef &def)
{
  if (!inBounds(nx, ny))
    return false;
  const int targetId = grid[getIndex(nx, ny)].matId;
  const auto &target = MaterialDB::getInstance().getMaterial(targetId);
  if (targetId != 0)
  {
    if (target.state != MatterState::Liquid && target.state != MatterState::Gas &&
        target.state != MatterState::Plasma)
      return false;
    if (def.state == MatterState::Gas || def.state == MatterState::Plasma)
    {
      if (def.density >= target.density)
        return false;
    }
    else if (def.density <= target.density)
    {
      return false;
    }
  }
  swapParticles(x, y, nx, ny);
  return true;
}

bool World::reactAt(int x, int y, int materialId)
{
  static constexpr int dx[] = {-1, 1, 0, 0};
  static constexpr int dy[] = {0, 0, -1, 1};
  auto &db = MaterialDB::getInstance();
  for (int i = 0; i < 4; ++i)
  {
    const int nx = x + dx[i], ny = y + dy[i];
    if (!inBounds(nx, ny))
      continue;
    const auto *reaction = db.getReaction(materialId, grid[getIndex(nx, ny)].matId);
    if (!reaction || reaction->probability <= 0.0f)
      continue;
    const float contactTemperature =
        (getTemperature(x, y) + getTemperature(nx, ny)) * 0.5f;
    if (contactTemperature < reaction->minTemperature ||
        contactTemperature > reaction->maxTemperature)
      continue;
    if (reaction->minHumidity > 0.0f || reaction->maxHumidity < 1.0f)
    {
      const float contactHumidity =
          (getHumidity(x, y) + getHumidity(nx, ny)) * 0.5f;
      if (contactHumidity < reaction->minHumidity ||
          contactHumidity > reaction->maxHumidity)
        continue;
    }
    wakeAt(x, y);
    if (static_cast<float>(std::rand()) / RAND_MAX < reaction->probability)
    {
      const int resultA = reaction->product1, resultB = reaction->product2;
      setParticle(x, y, resultA);
      setParticle(nx, ny, resultB);
      if (reaction->heatDelta != 0.0f)
      {
        changeTemperature(x, y, reaction->heatDelta);
        changeTemperature(nx, ny, reaction->heatDelta);
      }
      grid[getIndex(x, y)].updated_this_frame = true;
      grid[getIndex(nx, ny)].updated_this_frame = true;
      ++reactionEvents;
      return true;
    }
  }
  return false;
}

bool World::transformAt(int x, int y, int materialId)
{
  const auto &db = MaterialDB::getInstance();
  const auto &def = db.getMaterial(materialId);
  const float temperature = getTemperature(x, y);
  if (def.ignitionTemperature <= 1.0f &&
      temperature >= def.ignitionTemperature)
  {
    wakeAt(x, y);
    if (static_cast<float>(std::rand()) / RAND_MAX < def.ignitionProbability)
    {
      setParticle(x, y, db.getId("Fire"));
      grid[getIndex(x, y)].updated_this_frame = true;
      ++reactionEvents;
      return true;
    }
  }
  for (const auto &transition : db.getTransitions(materialId))
  {
    if (temperature < transition.minTemperature ||
        temperature > transition.maxTemperature)
      continue;
    wakeAt(x, y);
    if (static_cast<float>(std::rand()) / RAND_MAX >= transition.probability)
      continue;
    setParticle(x, y, transition.product);
    changeTemperature(x, y, transition.heatDelta);
    grid[getIndex(x, y)].updated_this_frame = true;
    ++reactionEvents;
    return true;
  }
  return false;
}

void World::clear()
{
  graphicsFrameCounter = 0;
  std::fill(grid.begin(), grid.end(), Particle{});
  for (auto *field : {&tempA, &tempB, &humidA, &humidB, &nutA, &nutB})
    std::fill(field->begin(), field->end(), 0.0f);
  for (auto *field : {&soilWater, &soilWaterNext, &pressureField})
    std::fill(field->begin(), field->end(), 0.0f);
  std::fill(atmosphericVapor.begin(), atmosphericVapor.end(), 0.0f);
  std::fill(atmosphericCloud.begin(), atmosphericCloud.end(), 0.0f);
  hydrologyTickCounter = pressureTickCounter = 0;
  structuralTickCounter = 0;
  structuralActivityTicks = 0;
  stormIntensity = 0.0f;
  stormTicks = 0;
  tornadoTicks = 0;
  tornadoX = 0.0f;
  recentEvents.clear();
  emitters.clear();
  emitterTickCounter = 0;
  rainIntensity = 0.0f;
  hasLife = false;
  reactionEvents = 0;
  usingBufferA = true;
  fieldTickCounter = lifeTickCounter = weatherTickCounter = 0;
  for (auto &chunk : chunks)
    chunk.setDirty(true);
}

void World::createEcosystem(Biome selected, std::uint32_t worldSeed)
{
  clear();
  environment = Environment::Natural;
  dayNightEnabled = true;
  biome = selected;
  seed = worldSeed;
  ambientTemperature = biome == Biome::Arid ? 0.36f : biome == Biome::Wetland ? 0.15f
                                                                              : 0.06f;
  ambientHumidity = biome == Biome::Arid ? 0.008f : biome == Biome::Wetland ? 0.24f
                                                                            : 0.09f;
  elapsedTime = static_cast<float>(hash32(seed ^ 0x9ca4d2u) % 12000u) / 100.0f;
  timeOfDay = elapsedTime * 0.05236f;
  daylight = 0.5f + 0.5f * std::sin(timeOfDay);
  cloudiness = std::clamp((biome == Biome::Arid ? 0.08f : biome == Biome::Wetland ? 0.67f
                                                                                  : 0.32f) +
                              (static_cast<float>(hash32(seed ^ 0x7311u) % 100u) / 100.0f - 0.5f) * 0.22f,
                          0.0f, 1.0f);
  wind = (static_cast<float>(hash32(seed ^ 0x947bu) % 201u) - 100.0f) / 75.0f;
  rainEnabled = false;
  automaticWeather = true;
  climateOffset = 0.0f;
  groundLevel = height > 512 ? 365 : static_cast<int>(height * (biome == Biome::Wetland ? 0.69f : 0.72f));
  std::mt19937 generator(seed);
  std::uniform_int_distribution<int> chance(0, 99);
  const auto &db = MaterialDB::getInstance();
  std::vector<int> surfaces(width);
  for (int x = 0; x < width; ++x)
  {
    const float relief = (noise1D(x * 0.013f, seed) - 0.5f) *
                             (biome == Biome::Wetland ? 15.0f : 30.0f) +
                         (noise1D(x * 0.055f, seed ^ 0x9e3779b9u) - 0.5f) * 10.0f;
    const int surface = std::clamp(static_cast<int>(groundLevel + relief),
                                   40, height - 24);
    surfaces[x] = surface;
    const int upperMaterial = biome == Biome::Arid ? db.getId("Sand") : biome == Biome::Wetland ? db.getId("Peat")
                                                                                                : db.getId("Soil");
    for (int y = surface; y < height; ++y)
    {
      const int depth = y - surface;
      const float stratification = noise2D(x * 0.027f, y * 0.034f,
                                           seed ^ 0x1569u);
      const float seams = noise2D(x * 0.075f, y * 0.068f,
                                  seed ^ 0x8d11u);
      const float caves = noise2D(x * 0.041f, y * 0.051f,
                                  seed ^ 0x9b22u);
      if (depth > 31 && y < height - 7 && caves > 0.795f)
        continue;
      int material = upperMaterial;
      if (depth >= (biome == Biome::Arid ? 13 : 17) && depth < 49)
        material = biome == Biome::Arid ? db.getId("Sandstone") : biome == Biome::Wetland ? db.getId("Silt")
                                                                                          : db.getId("Clay");
      else if (depth >= 49 && depth < 126)
        material = stratification < 0.38f ? db.getId("Shale") : stratification < 0.71f ? db.getId("Limestone")
                                                                                       : db.getId("Sandstone");
      else if (depth >= 126)
        material = stratification > 0.61f ? db.getId("Basalt") : db.getId("Granite");
      if (depth > 45 && depth < height - surface - 6)
      {
        if (seams > 0.858f)
          material = db.getId("IronOre");
        else if (seams < 0.102f && depth > 82)
          material = db.getId("CopperOre");
        else if (seams > 0.755f && depth < 118 && stratification < 0.43f)
          material = db.getId("Coal");
        else if (seams > 0.79f && depth > 134 && stratification > 0.63f)
          material = db.getId("Quartz");
      }
      if (y >= height - 4)
        material = db.getId("Granite");
      setParticle(x, y, material);
    }
    if (biome != Biome::Arid && x > 12 && x < width - 12 &&
        chance(generator) < 7)
    {
      setParticle(x, surface - 1, db.getId(biome == Biome::Wetland ? "MangroveSeed" : chance(generator) < 34 ? "PineSeed"
                                                                                                             : "Seed"));
    }
    else if (biome == Biome::Arid && x > 12 && x < width - 12 &&
             chance(generator) < 4)
    {
      setParticle(x, surface - 1, db.getId("ShrubSeed"));
    }
  }
  const int center = width / 3 + static_cast<int>(hash32(seed ^ 0x98u) %
                                                  static_cast<std::uint32_t>(std::max(1, width / 3)));
  const int halfWidth = width / (biome == Biome::Arid ? 16 : biome == Biome::Wetland ? 6
                                                                                     : 9);
  const int waterline = surfaces[center] + (biome == Biome::Arid ? 4 : 3);
  for (int x = center - halfWidth; x <= center + halfWidth; ++x)
  {
    if (x < 0 || x >= width)
      continue;
    const float distance = static_cast<float>(x - center) / halfWidth;
    const int depth = static_cast<int>((biome == Biome::Arid ? 13 : biome == Biome::Wetland ? 34
                                                                                            : 27) *
                                       (1.0f - distance * distance));
    const int floor = std::min(height - 1, surfaces[x] + depth);
    for (int y = surfaces[x]; y < floor; ++y)
      setParticle(x, y, 0);
    for (int y = std::max(0, waterline); y < floor; ++y)
      if (getParticle(x, y).matId == 0)
        setParticle(x, y, db.getId("Water"));
  }
  if (biome != Biome::Arid)
  {
    for (int x = 20; x < width - 20; x += 28 + chance(generator) % 19)
    {
      if (std::abs(x - center) < halfWidth + 10)
        continue;
      const int surface = surfaces[x];
      const int trunk = 7 + chance(generator) % 6;
      const unsigned char species = biome == Biome::Wetland ? 2 : chance(generator) < 36 ? 1
                                                                                         : 0;
      for (int y = surface - trunk; y < surface; ++y)
      {
        setParticle(x, y, db.getId("Wood"));
        grid[getIndex(x, y)].species = species;
      }
      for (int dy = -4; dy <= 2; ++dy)
        for (int dx = -5; dx <= 5; ++dx)
          if (dx * dx + dy * dy * 2 < 24 &&
              inBounds(x + dx, surface - trunk + dy) &&
              getParticle(x + dx, surface - trunk + dy).matId == 0)
            setParticle(x + dx, surface - trunk + dy,
                        db.getId(species == 1 ? "PineNeedles" : "Plant"));
    }
  }
  if (biome == Biome::Wetland)
  {
    for (int x = 8; x < width - 8; x += 3 + chance(generator) % 8)
    {
      const int surface = surfaces[x];
      const int below = getParticle(x, surface).matId;
      if ((below == db.getId("Soil") || below == db.getId("Mud") ||
           below == db.getId("Peat")) &&
          getParticle(x, surface - 1).matId == 0)
        setParticle(x, surface - 1, db.getId("Grass"));
      if (std::abs(x - center) < halfWidth && chance(generator) < 35)
      {
        const int y = std::max(0, waterline);
        if (getParticle(x, y).matId == db.getId("Water"))
          setParticle(x, y, db.getId("Algae"));
      }
    }
  }
  else if (biome == Biome::Temperate)
  {
    for (int x = 10; x < width - 10; x += 4 + chance(generator) % 7)
    {
      const int surface = surfaces[x];
      if (getParticle(x, surface - 1).matId == 0 &&
          getParticle(x, surface).matId == db.getId("Soil"))
        setParticle(x, surface - 1,
                    chance(generator) < 12 ? db.getId("Flower") : db.getId("Grass"));
    }
  }
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
    {
      const int index = getIndex(x, y);
      const int id = grid[index].matId;
      const bool soil = db.getMaterial(id).porosity > 0.01f;
      tempA[index] = tempB[index] = ambientTemperature;
      humidA[index] = humidB[index] = soil && biome != Biome::Arid
                                          ? (biome == Biome::Wetland ? 0.34f : 0.16f)
                                          : ambientHumidity;
      nutA[index] = nutB[index] = soil
                                      ? (biome == Biome::Arid ? 0.08f : 0.60f)
                                      : 0.0f;
      if (soil)
      {
        const float wetness = biome == Biome::Wetland ? 0.64f : biome == Biome::Arid ? 0.035f
                                                                                     : 0.28f;
        const float aquifer = (y - surfaces[x] > 110 &&
                               noise2D(x * 0.028f, y * 0.044f, seed ^ 0x9e13u) > 0.62f)
                                  ? 0.88f
                                  : wetness;
        soilWater[index] = db.getMaterial(id).porosity * aquifer;
      }
    }
  soilWaterNext = soilWater;
  prepareAtmosphere();
  updatePressure();
  structuralActivityTicks = 0;
  logEvent("Novo mundo gerado: seed " + std::to_string(seed));
}

void World::createLaboratory(std::uint32_t worldSeed)
{
  clear();
  environment = Environment::Laboratory;
  biome = Biome::Temperate;
  seed = worldSeed;
  elapsedTime = 0.0f;
  setClockHour(12.0f);
  daylight = 0.86f;
  dayNightEnabled = false;
  automaticWeather = false;
  rainEnabled = false;
  cloudiness = 0.0f;
  wind = 0.0f;
  climateOffset = 0.0f;
  ambientTemperature = 0.06f;
  ambientHumidity = 0.06f;
  groundLevel = height - 20;
  const int wallId = MaterialDB::getInstance().getId("Wall");
  for (int x = 0; x < width; ++x)
    for (int y = height - 8; y < height; ++y)
    {
      setParticle(x, y, wallId);
      grid[getIndex(x, y)].anchored = true;
    }
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < 5; ++x)
    {
      setParticle(x, y, wallId);
      setParticle(width - 1 - x, y, wallId);
      grid[getIndex(x, y)].anchored = true;
      grid[getIndex(width - 1 - x, y)].anchored = true;
    }
  structuralActivityTicks = 0;
  prepareAtmosphere();
  updatePressure();
  logEvent("Laboratorio iniciado");
}

void World::update(float dt)
{
  elapsedTime += dt;
  updateEmitters();
  if (dayNightEnabled)
  {
    timeOfDay += dt * 6.28318530718f / std::max(30.0f, dayLengthSeconds);
    daylight = 0.5f + 0.5f * std::sin(timeOfDay);
  }
  else
    daylight = clamp01(daylight);
  updateWeather();
  if (++hydrologyTickCounter >= 18)
  {
    hydrologyTickCounter = 0;
    updateHydrology();
  }
  const int structuralInterval = grid.size() >= 600000 ? 3 : 2;
  if (++structuralTickCounter >= structuralInterval)
  {
    structuralTickCounter = 0;
    if (structuralActivityTicks > 0)
      updateStructures();
  }
  if (structuralActivityTicks > 0)
    --structuralActivityTicks;

  std::vector<unsigned char> active(chunks.size(), 0);
  for (std::size_t i = 0; i < chunks.size(); ++i)
  {
    if (!chunks[i].isDirty())
      continue;
    active[i] = 1;
    chunks[i].setDirty(false);
    const int cx = static_cast<int>(i) % chunksX;
    const int cy = static_cast<int>(i) / chunksX;
    for (int y = cy * CHUNK_SIZE; y < std::min((cy + 1) * CHUNK_SIZE, height); ++y)
      for (int x = cx * CHUNK_SIZE; x < std::min((cx + 1) * CHUNK_SIZE, width); ++x)
        grid[getIndex(x, y)].updated_this_frame = false;
  }

  auto &db = MaterialDB::getInstance();
  const int fireId = db.getId("Fire");
  const int seedId = db.getId("Seed");
  const int pineSeedId = db.getId("PineSeed");
  const int mangroveSeedId = db.getId("MangroveSeed");
  const int shrubSeedId = db.getId("ShrubSeed");
  const int ashId = db.getId("Ash");
  for (int cy = chunksY - 1; cy >= 0; --cy)
  {
    for (int cx = 0; cx < chunksX; ++cx)
    {
      if (!active[cy * chunksX + cx])
        continue;
      const int xMin = cx * CHUNK_SIZE;
      const int xMax = std::min(xMin + CHUNK_SIZE, width);
      const int yMin = cy * CHUNK_SIZE;
      const int yMax = std::min(yMin + CHUNK_SIZE, height);
      for (int y = yMax - 1; y >= yMin; --y)
      {
        const int direction = std::rand() % 2 == 0 ? 1 : -1;
        for (int xi = 0; xi < xMax - xMin; ++xi)
        {
          const int x = direction > 0 ? xMin + xi : xMax - 1 - xi;
          Particle &p = grid[getIndex(x, y)];
          if (p.matId == 0 || p.updated_this_frame)
            continue;
          const int materialId = p.matId;
          const auto &def = db.getMaterial(materialId);
          if (def.lifetime > 0)
          {
            wakeAt(x, y);
            if (--p.lifetime_counter <= 0)
            {
              const int residue = p.burnResidue;
              setParticle(x, y, residue > 0 ? residue : (def.state == MatterState::Plasma && std::rand() % 100 < 35 ? db.getId("Smoke") : 0));
              continue;
            }
          }
          if (isPlant(materialId))
          {
            wakeAt(x, y);
            if (std::rand() % 8 == 0)
              Biology::processPlant(this, x, y, p);
            if (grid[getIndex(x, y)].matId != materialId)
              continue;
          }
          if (reactAt(x, y, materialId))
            continue;
          if ((def.hasTransitions || def.ignitionProbability > 0.0f) &&
              transformAt(x, y, materialId))
            continue;
          if (materialId == fireId && p.burnResidue != 0)
          {
            wakeAt(x, y);
            continue;
          }
          if (def.state == MatterState::Liquid)
            updateFluidMotion(x, y, def);
          if (def.state == MatterState::Granular && !p.anchored)
          {
            const int below = y + 1 < height ? grid[getIndex(x, y + 1)].matId : -1;
            const auto &belowDef = db.getMaterial(below);
            const bool falling = below == 0 ||
                                 (below > 0 && (belowDef.state == MatterState::Liquid || belowDef.state == MatterState::Gas) &&
                                  def.density > belowDef.density);
            if (falling)
            {
              p.velocityY = std::min(6.0f, p.velocityY + 0.52f * gravityScale);
              p.velocityX *= 0.97f;
            }
            else
            {
              p.velocityY = std::min(p.velocityY, 0.0f);
              p.velocityX *= std::clamp(1.0f - def.friction * 0.09f, 0.84f, 0.98f);
            }
          }
          if (!isStructural(materialId) &&
              (std::abs(p.velocityX) >= 0.08f ||
               std::abs(p.velocityY) >= 0.08f) &&
              integrateMotion(x, y, def))
            continue;
          if (def.state == MatterState::Granular ||
              def.state == MatterState::Liquid)
          {
            if (def.state == MatterState::Liquid &&
                (std::abs(p.velocityX) + std::abs(p.velocityY) >= 0.08f))
              continue;
            const bool carriedByWind = materialId == seedId ||
                                       materialId == pineSeedId || materialId == mangroveSeedId ||
                                       materialId == shrubSeedId || materialId == ashId;
            const int side = carriedByWind && std::abs(wind) > 0.15f &&
                                     std::rand() % 100 < std::min(85, static_cast<int>(std::abs(wind) * 38.0f))
                                 ? (wind > 0 ? 1 : -1)
                             : std::rand() % 2 == 0 ? 1
                                                    : -1;
            if (tryMove(x, y, x, y + 1, def) ||
                tryMove(x, y, x + side, y + 1, def) ||
                tryMove(x, y, x - side, y + 1, def))
              continue;
            if (def.state == MatterState::Liquid)
            {
              const int range = std::clamp(static_cast<int>(def.dispersionRate), 0, 8);
              for (int d : {side, -side})
              {
                bool moved = false;
                int reachable = 0;
                for (int step = 1; step <= range; ++step)
                {
                  if (!inBounds(x + d * step, y) ||
                      grid[getIndex(x + d * step, y)].matId != 0)
                    break;
                  reachable = step;
                }
                if (reachable > 0)
                  moved = tryMove(x, y, x + d * reachable, y, def);
                if (moved)
                  break;
              }
            }
          }
          else if (def.state == MatterState::Gas ||
                   def.state == MatterState::Plasma)
          {
            const int side = std::abs(wind) > 0.10f &&
                                     std::rand() % 100 < std::min(94, static_cast<int>(std::abs(wind) * 55.0f))
                                 ? (wind > 0 ? 1 : -1)
                             : std::rand() % 2 == 0 ? 1
                                                    : -1;
            if (std::abs(wind) > 0.32f && std::rand() % 3 == 0 &&
                tryMove(x, y, x + side, y, def))
              continue;
            if (tryMove(x, y, x, y - 1, def) ||
                tryMove(x, y, x + side, y - 1, def) ||
                tryMove(x, y, x - side, y - 1, def))
              continue;
            tryMove(x, y, x + side, y, def);
          }
        }
      }
    }
  }

  if (++fieldTickCounter >= 12)
  {
    fieldTickCounter = 0;
    updateFields();
  }
  if (hasLife && ++lifeTickCounter >= 6)
  {
    lifeTickCounter = 0;
    updateLife();
  }
}

float World::neighborAverage(const std::vector<float> &values, int x, int y) const
{
  float sum = 0.0f;
  int n = 0;
  for (int d = 0; d < 4; ++d)
  {
    const int nx = x + (d == 0) - (d == 1);
    const int ny = y + (d == 2) - (d == 3);
    if (inBounds(nx, ny))
    {
      sum += values[getIndex(nx, ny)];
      ++n;
    }
  }
  return n ? sum / n : 0.0f;
}

float World::getTemperature(int x, int y) const
{
  return inBounds(x, y) ? (usingBufferA ? tempA : tempB)[getIndex(x, y)] : 0;
}
float World::getHumidity(int x, int y) const
{
  return inBounds(x, y) ? (usingBufferA ? humidA : humidB)[getIndex(x, y)] : 0;
}
float World::getNutrients(int x, int y) const
{
  return inBounds(x, y) ? (usingBufferA ? nutA : nutB)[getIndex(x, y)] : 0;
}
void World::changeNutrients(int x, int y, float amount)
{
  if (inBounds(x, y))
    (usingBufferA ? nutA : nutB)[getIndex(x, y)] =
        clamp01(getNutrients(x, y) + amount);
}
void World::changeHumidity(int x, int y, float amount)
{
  if (inBounds(x, y))
    (usingBufferA ? humidA : humidB)[getIndex(x, y)] =
        clamp01(getHumidity(x, y) + amount);
}

void World::consumeSoilWater(int x, int y, float amount)
{
  if (!inBounds(x, y) || amount <= 0.0f)
    return;
  const int index = getIndex(x, y);
  soilWater[index] = std::max(0.0f, soilWater[index] - amount);
  wakeAt(x, y);
}

void World::changeTemperature(int x, int y, float amount)
{
  if (!inBounds(x, y) || amount == 0.0f)
    return;
  const int index = getIndex(x, y);
  const float result = std::clamp(getTemperature(x, y) + amount, -1.0f, 1.0f);
  tempA[index] = tempB[index] = result;
  wakeAt(x, y);
}

void World::updateFields()
{
  const auto &tSrc = usingBufferA ? tempA : tempB;
  auto &tDst = usingBufferA ? tempB : tempA;
  const auto &hSrc = usingBufferA ? humidA : humidB;
  auto &hDst = usingBufferA ? humidB : humidA;
  const auto &nSrc = usingBufferA ? nutA : nutB;
  auto &nDst = usingBufferA ? nutB : nutA;
  const auto &db = MaterialDB::getInstance();
  const float seasonalTemperature = 0.05f * std::sin(elapsedTime * 0.0009f);
  const float effectiveAmbient = std::clamp(ambientTemperature + climateOffset +
                                                seasonalTemperature,
                                            -1.0f, 1.0f);
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      const int i = getIndex(x, y);
      const auto &name = db.getMaterial(grid[i].matId).name;
      const auto &material = db.getMaterial(grid[i].matId);
      const float capacity = material.heatCapacity;
      float heatExchange = 0.0f;
      for (int d = 0; d < 4; ++d)
      {
        const int nx = x + (d == 0) - (d == 1);
        const int ny = y + (d == 2) - (d == 3);
        if (!inBounds(nx, ny))
          continue;
        const int ni = getIndex(nx, ny);
        const auto &neighbor = db.getMaterial(grid[ni].matId);
        heatExchange += (tSrc[ni] - tSrc[i]) *
                        std::min(material.thermalConductivity,
                                 neighbor.thermalConductivity);
      }
      float temp = tSrc[i] + 0.21f * heatExchange / capacity;
      const float cooling = grid[i].matId == 0 ? 0.025f : 0.003f;
      temp += (effectiveAmbient - temp) * cooling + daylight * 0.0003f;
      float humidity = hSrc[i] * 0.90f +
                       neighborAverage(hSrc, x, y) * 0.09f +
                       ambientHumidity * 0.01f;
      float nutrients = nSrc[i] * 0.93f +
                        neighborAverage(nSrc, x, y) * 0.065f;
      if (name == "Fire")
        temp = std::max(temp, 0.97f);
      if (name == "Water" || name == "Saltwater" || name == "Steam")
        humidity = 1.0f;
      if (name == "Soil" || name == "Mud")
      {
        nutrients = std::max(nutrients, biome == Biome::Arid ? 0.12f : biome == Biome::Wetland ? 0.72f
                                                                                               : 0.60f);
        humidity = std::max(humidity, ambientHumidity * 0.38f);
      }
      if (material.porosity > 0.01f)
        humidity = std::max(humidity, clamp01(soilWater[i] / material.porosity) * 0.88f);
      if (name == "Peat")
        nutrients = std::max(nutrients, 0.79f);
      if (name == "Compost")
        nutrients = 1.0f;
      tDst[i] = std::clamp(temp, -1.0f, 1.0f);
      hDst[i] = clamp01(humidity);
      nDst[i] = clamp01(nutrients);
      if (grid[i].matId != 0 &&
          (std::abs(tDst[i] - tSrc[i]) > 0.015f ||
           (tSrc[i] < 0.30f && tDst[i] >= 0.30f) ||
           (tSrc[i] > -0.40f && tDst[i] <= -0.40f)))
        wakeAt(x, y);
    }
  }
  usingBufferA = !usingBufferA;
}

void World::updateLife()
{
  const int count = width * height;
  const int lifeId = MaterialDB::getInstance().getId("Life");
  std::vector<unsigned char> old(count, 0);
  for (int i = 0; i < count; ++i)
    old[i] = grid[i].matId == lifeId ? 1 : 0;
  std::vector<std::pair<int, int>> changes;
  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 1; x < width - 1; ++x)
    {
      const int i = getIndex(x, y);
      if (!old[i] && grid[i].matId != 0)
        continue;
      int neighbors = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
          if (dx || dy)
            neighbors += old[getIndex(x + dx, y + dy)];
      if (old[i] && (neighbors < 2 || neighbors > 3))
        changes.emplace_back(x, y);
      else if (!old[i] && neighbors == 3)
        changes.emplace_back(x, y);
    }
  }
  bool anyLife = false;
  for (const auto &change : changes)
  {
    const int x = change.first, y = change.second;
    setParticle(x, y, old[getIndex(x, y)] ? 0 : lifeId);
  }
  for (const auto &cell : grid)
    if (cell.matId == lifeId)
    {
      anyLife = true;
      break;
    }
  hasLife = anyLife;
}

bool World::save(const std::filesystem::path &path) const
{
  std::ofstream file(path, std::ios::binary);
  if (!file)
    return false;
  const char magic[8] = {'M', 'B', 'W', 'O', 'R', 'L', 'D', '9'};
  const std::uint32_t w = static_cast<std::uint32_t>(width);
  const std::uint32_t h = static_cast<std::uint32_t>(height);
  const std::uint32_t biomeValue = static_cast<std::uint32_t>(biome);
  const std::uint8_t rainValue = rainEnabled ? 1 : 0;
  const std::uint8_t dayNightValue = dayNightEnabled ? 1 : 0;
  const std::uint8_t weatherValue = automaticWeather ? 1 : 0;
  file.write(magic, sizeof(magic));
  file.write(reinterpret_cast<const char *>(&w), sizeof(w));
  file.write(reinterpret_cast<const char *>(&h), sizeof(h));
  file.write(reinterpret_cast<const char *>(&elapsedTime), sizeof(elapsedTime));
  file.write(reinterpret_cast<const char *>(&biomeValue), sizeof(biomeValue));
  file.write(reinterpret_cast<const char *>(&seed), sizeof(seed));
  file.write(reinterpret_cast<const char *>(&rainValue), sizeof(rainValue));
  file.write(reinterpret_cast<const char *>(&dayNightValue), sizeof(dayNightValue));
  file.write(reinterpret_cast<const char *>(&reactionEvents), sizeof(reactionEvents));
  file.write(reinterpret_cast<const char *>(&cloudiness), sizeof(cloudiness));
  file.write(reinterpret_cast<const char *>(&wind), sizeof(wind));
  file.write(reinterpret_cast<const char *>(&daylight), sizeof(daylight));
  file.write(reinterpret_cast<const char *>(&timeOfDay), sizeof(timeOfDay));
  file.write(reinterpret_cast<const char *>(&weatherValue), sizeof(weatherValue));
  file.write(reinterpret_cast<const char *>(&groundLevel), sizeof(groundLevel));
  file.write(reinterpret_cast<const char *>(&stormIntensity), sizeof(stormIntensity));
  file.write(reinterpret_cast<const char *>(&stormTicks), sizeof(stormTicks));
  file.write(reinterpret_cast<const char *>(&tornadoX), sizeof(tornadoX));
  file.write(reinterpret_cast<const char *>(&tornadoTicks), sizeof(tornadoTicks));
  file.write(reinterpret_cast<const char *>(&climateOffset), sizeof(climateOffset));
  file.write(reinterpret_cast<const char *>(&dayLengthSeconds), sizeof(dayLengthSeconds));
  const std::uint8_t environmentValue = environment == Environment::Laboratory ? 1 : 0;
  file.write(reinterpret_cast<const char *>(&environmentValue), sizeof(environmentValue));
  for (const auto &p : grid)
  {
    file.write(reinterpret_cast<const char *>(&p.matId), sizeof(p.matId));
    file.write(reinterpret_cast<const char *>(&p.age), sizeof(p.age));
    file.write(reinterpret_cast<const char *>(&p.lifetime_counter),
               sizeof(p.lifetime_counter));
    file.write(reinterpret_cast<const char *>(&p.burnResidue), sizeof(p.burnResidue));
    file.write(reinterpret_cast<const char *>(&p.volume), sizeof(p.volume));
    file.write(reinterpret_cast<const char *>(&p.species), sizeof(p.species));
    file.write(reinterpret_cast<const char *>(&p.velocityX), sizeof(p.velocityX));
    file.write(reinterpret_cast<const char *>(&p.velocityY), sizeof(p.velocityY));
    file.write(reinterpret_cast<const char *>(&p.subcellX), sizeof(p.subcellX));
    file.write(reinterpret_cast<const char *>(&p.subcellY), sizeof(p.subcellY));
    file.write(reinterpret_cast<const char *>(&p.damage), sizeof(p.damage));
    const std::uint8_t anchor = p.anchored ? 1 : 0;
    file.write(reinterpret_cast<const char *>(&anchor), sizeof(anchor));
  }
  for (const auto *buffer : {&(usingBufferA ? tempA : tempB),
                             &(usingBufferA ? humidA : humidB),
                             &(usingBufferA ? nutA : nutB)})
    file.write(reinterpret_cast<const char *>(buffer->data()),
               static_cast<std::streamsize>(buffer->size() * sizeof(float)));
  for (const auto *buffer : {&soilWater, &atmosphericVapor, &atmosphericCloud})
    file.write(reinterpret_cast<const char *>(buffer->data()),
               static_cast<std::streamsize>(buffer->size() * sizeof(float)));
  const std::uint32_t count = 0;
  file.write(reinterpret_cast<const char *>(&count), sizeof(count));
  const std::uint32_t emitterCount = static_cast<std::uint32_t>(emitters.size());
  file.write(reinterpret_cast<const char *>(&emitterCount), sizeof(emitterCount));
  for (const auto &device : emitters)
  {
    const std::uint8_t mode = static_cast<std::uint8_t>(device.mode);
    file.write(reinterpret_cast<const char *>(&device.x), sizeof(device.x));
    file.write(reinterpret_cast<const char *>(&device.y), sizeof(device.y));
    file.write(reinterpret_cast<const char *>(&device.materialId), sizeof(device.materialId));
    file.write(reinterpret_cast<const char *>(&mode), sizeof(mode));
    file.write(reinterpret_cast<const char *>(&device.intensity), sizeof(device.intensity));
    file.write(reinterpret_cast<const char *>(&device.radius), sizeof(device.radius));
    file.write(reinterpret_cast<const char *>(&device.directionX), sizeof(device.directionX));
    file.write(reinterpret_cast<const char *>(&device.directionY), sizeof(device.directionY));
  }
  return static_cast<bool>(file);
}

bool World::load(const std::filesystem::path &path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return false;
  char magic[8];
  std::uint32_t w = 0, h = 0;
  float savedTime = 0.0f;
  file.read(magic, sizeof(magic));
  file.read(reinterpret_cast<char *>(&w), sizeof(w));
  file.read(reinterpret_cast<char *>(&h), sizeof(h));
  file.read(reinterpret_cast<char *>(&savedTime), sizeof(savedTime));
  const char previous[8] = {'A', 'G', 'W', 'O', 'R', 'L', 'D', '2'};
  const char previous3[8] = {'A', 'G', 'W', 'O', 'R', 'L', 'D', '3'};
  const char previous4[8] = {'A', 'G', 'W', 'O', 'R', 'L', 'D', '4'};
  const char current[8] = {'A', 'G', 'W', 'O', 'R', 'L', 'D', '5'};
  const char current6[8] = {'M', 'B', 'W', 'O', 'R', 'L', 'D', '6'};
  const char current7[8] = {'M', 'B', 'W', 'O', 'R', 'L', 'D', '7'};
  const char current8[8] = {'M', 'B', 'W', 'O', 'R', 'L', 'D', '8'};
  const char current9[8] = {'M', 'B', 'W', 'O', 'R', 'L', 'D', '9'};
  const bool legacy = std::equal(std::begin(magic), std::end(magic), previous);
  const bool version3 = std::equal(std::begin(magic), std::end(magic), previous3);
  const bool version4 = std::equal(std::begin(magic), std::end(magic), previous4);
  const bool version5 = std::equal(std::begin(magic), std::end(magic), current);
  const bool version6 = std::equal(std::begin(magic), std::end(magic), current6);
  const bool version7 = std::equal(std::begin(magic), std::end(magic), current7);
  const bool version8 = std::equal(std::begin(magic), std::end(magic), current8);
  const bool version9 = std::equal(std::begin(magic), std::end(magic), current9);
  if (!file || (!legacy && !version3 && !version4 && !version5 && !version6 && !version7 && !version8 && !version9) ||
      w < 64 || h < 64 || w > 1024 || h > 1024 || !std::isfinite(savedTime))
    return false;
  std::uint32_t savedBiome = 0, savedSeed = 1;
  std::uint8_t savedRain = 0, savedDayNight = 1;
  std::uint64_t savedReactions = 0;
  float savedCloudiness = 0.32f, savedWind = 0.15f, savedDaylight = 0.8f;
  float savedTimeOfDay = savedTime * 0.05236f;
  std::uint8_t savedAutomaticWeather = 1;
  int savedGroundLevel = std::min<int>(365, static_cast<int>(h * 0.72f));
  float savedStorm = 0.0f;
  int savedStormTicks = 0;
  float savedTornadoX = 0.0f;
  int savedTornadoTicks = 0;
  float savedClimateOffset = 0.0f;
  float savedDayLength = 120.0f;
  std::uint8_t savedEnvironment = 0;
  if (!legacy)
  {
    file.read(reinterpret_cast<char *>(&savedBiome), sizeof(savedBiome));
    file.read(reinterpret_cast<char *>(&savedSeed), sizeof(savedSeed));
    file.read(reinterpret_cast<char *>(&savedRain), sizeof(savedRain));
    file.read(reinterpret_cast<char *>(&savedDayNight), sizeof(savedDayNight));
    if (!file || savedBiome > 2 || savedRain > 1 || savedDayNight > 1)
      return false;
  }
  if (version4 || version5 || version6 || version7 || version8 || version9)
  {
    file.read(reinterpret_cast<char *>(&savedReactions), sizeof(savedReactions));
    if (!file)
      return false;
  }
  if (version5 || version6 || version7 || version8 || version9)
  {
    file.read(reinterpret_cast<char *>(&savedCloudiness), sizeof(savedCloudiness));
    file.read(reinterpret_cast<char *>(&savedWind), sizeof(savedWind));
    file.read(reinterpret_cast<char *>(&savedDaylight), sizeof(savedDaylight));
    if (!file || !std::isfinite(savedCloudiness) || !std::isfinite(savedWind) ||
        !std::isfinite(savedDaylight) || savedCloudiness < 0 ||
        savedCloudiness > 1 || savedWind < -3 || savedWind > 3 ||
        savedDaylight < 0 || savedDaylight > 1)
      return false;
  }
  if (version6 || version7 || version8 || version9)
  {
    file.read(reinterpret_cast<char *>(&savedTimeOfDay), sizeof(savedTimeOfDay));
    file.read(reinterpret_cast<char *>(&savedAutomaticWeather), sizeof(savedAutomaticWeather));
    file.read(reinterpret_cast<char *>(&savedGroundLevel), sizeof(savedGroundLevel));
    if (!file || !std::isfinite(savedTimeOfDay) || savedAutomaticWeather > 1 ||
        savedGroundLevel < 0 || savedGroundLevel >= static_cast<int>(h))
      return false;
  }
  if (version7 || version8 || version9)
  {
    file.read(reinterpret_cast<char *>(&savedStorm), sizeof(savedStorm));
    file.read(reinterpret_cast<char *>(&savedStormTicks), sizeof(savedStormTicks));
    file.read(reinterpret_cast<char *>(&savedTornadoX), sizeof(savedTornadoX));
    file.read(reinterpret_cast<char *>(&savedTornadoTicks), sizeof(savedTornadoTicks));
    file.read(reinterpret_cast<char *>(&savedClimateOffset), sizeof(savedClimateOffset));
    if (version8 || version9)
      file.read(reinterpret_cast<char *>(&savedDayLength), sizeof(savedDayLength));
    if (version9)
      file.read(reinterpret_cast<char *>(&savedEnvironment), sizeof(savedEnvironment));
    if (!file || !std::isfinite(savedStorm) || savedStorm < 0.0f ||
        savedStorm > 1.0f || savedStormTicks < 0 || savedStormTicks > 100000 ||
        !std::isfinite(savedTornadoX) || savedTornadoX < 0.0f ||
        savedTornadoX > static_cast<float>(w) || savedTornadoTicks < 0 ||
        savedTornadoTicks > 100000 || !std::isfinite(savedClimateOffset) ||
        savedClimateOffset < -0.61f || savedClimateOffset > 0.61f ||
        !std::isfinite(savedDayLength) || savedDayLength < 30.0f || savedDayLength > 3600.0f ||
        savedEnvironment > 1)
      return false;
  }

  struct Cell
  {
    int material;
    int age;
    float lifetime;
    int burnResidue = 0;
    float volume = 1.0f;
    std::uint8_t species = 0;
    float velocityX = 0.0f, velocityY = 0.0f, damage = 0.0f;
    float subcellX = 0.0f, subcellY = 0.0f;
    std::uint8_t anchored = 0;
  };
  const std::size_t cellCount = static_cast<std::size_t>(w) * h;
  std::vector<Cell> cells(cellCount);
  for (auto &cell : cells)
  {
    file.read(reinterpret_cast<char *>(&cell.material), sizeof(cell.material));
    file.read(reinterpret_cast<char *>(&cell.age), sizeof(cell.age));
    file.read(reinterpret_cast<char *>(&cell.lifetime), sizeof(cell.lifetime));
    if (version4 || version5 || version6 || version7 || version8 || version9)
      file.read(reinterpret_cast<char *>(&cell.burnResidue), sizeof(cell.burnResidue));
    if (version6 || version7 || version8 || version9)
    {
      file.read(reinterpret_cast<char *>(&cell.volume), sizeof(cell.volume));
      file.read(reinterpret_cast<char *>(&cell.species), sizeof(cell.species));
    }
    if (version7 || version8 || version9)
    {
      file.read(reinterpret_cast<char *>(&cell.velocityX), sizeof(cell.velocityX));
      file.read(reinterpret_cast<char *>(&cell.velocityY), sizeof(cell.velocityY));
      if (version8 || version9)
      {
        file.read(reinterpret_cast<char *>(&cell.subcellX), sizeof(cell.subcellX));
        file.read(reinterpret_cast<char *>(&cell.subcellY), sizeof(cell.subcellY));
      }
      file.read(reinterpret_cast<char *>(&cell.damage), sizeof(cell.damage));
      file.read(reinterpret_cast<char *>(&cell.anchored), sizeof(cell.anchored));
    }
    if (!file || cell.material < 0 || cell.material >= 256 ||
        cell.age < 0 || cell.age > 100000 ||
        !std::isfinite(cell.lifetime) || cell.burnResidue < 0 ||
        cell.burnResidue >= 256 || !std::isfinite(cell.volume) ||
        cell.volume < 0 || cell.volume > 1.01f || cell.species > 7 ||
        !std::isfinite(cell.velocityX) || std::abs(cell.velocityX) > 6.1f ||
        !std::isfinite(cell.velocityY) || std::abs(cell.velocityY) > 6.1f ||
        !std::isfinite(cell.subcellX) || std::abs(cell.subcellX) > 6.1f ||
        !std::isfinite(cell.subcellY) || std::abs(cell.subcellY) > 6.1f ||
        !std::isfinite(cell.damage) || cell.damage < 0.0f ||
        cell.damage > 8.1f || cell.anchored > 1)
      return false;
  }
  std::vector<float> temp(cellCount), humid(cellCount), nutrient(cellCount);
  for (auto *buffer : {&temp, &humid, &nutrient})
  {
    file.read(reinterpret_cast<char *>(buffer->data()),
              static_cast<std::streamsize>(buffer->size() * sizeof(float)));
    if (!file || std::any_of(buffer->begin(), buffer->end(),
                             [](float v)
                             { return !std::isfinite(v); }))
      return false;
  }
  std::vector<float> savedSoilWater(cellCount, 0.0f);
  std::vector<float> savedVapor((w + 15) / 16, 0.0f);
  std::vector<float> savedCloud((w + 15) / 16, 0.0f);
  if (version6 || version7 || version8 || version9)
  {
    for (auto *buffer : {&savedSoilWater, &savedVapor, &savedCloud})
    {
      file.read(reinterpret_cast<char *>(buffer->data()),
                static_cast<std::streamsize>(buffer->size() * sizeof(float)));
      if (!file || std::any_of(buffer->begin(), buffer->end(),
                               [](float v)
                               { return !std::isfinite(v) || v < 0 || v > 3.0f; }))
        return false;
    }
  }
  std::uint32_t count = 0;
  file.read(reinterpret_cast<char *>(&count), sizeof(count));
  if (!file || count > 80)
    return false;
  for (std::uint32_t i = 0; i < count; ++i)
  {
    char legacyCreature[7 * sizeof(float) + 2 * sizeof(int)];
    file.read(legacyCreature, sizeof(legacyCreature));
    if (!file)
      return false;
  }
  std::vector<Emitter> savedEmitters;
  if (version9)
  {
    std::uint32_t emitterCount = 0;
    file.read(reinterpret_cast<char *>(&emitterCount), sizeof(emitterCount));
    if (!file || emitterCount > 64)
      return false;
    savedEmitters.reserve(emitterCount);
    for (std::uint32_t i = 0; i < emitterCount; ++i)
    {
      Emitter device;
      std::uint8_t mode = 0;
      file.read(reinterpret_cast<char *>(&device.x), sizeof(device.x));
      file.read(reinterpret_cast<char *>(&device.y), sizeof(device.y));
      file.read(reinterpret_cast<char *>(&device.materialId), sizeof(device.materialId));
      file.read(reinterpret_cast<char *>(&mode), sizeof(mode));
      file.read(reinterpret_cast<char *>(&device.intensity), sizeof(device.intensity));
      file.read(reinterpret_cast<char *>(&device.radius), sizeof(device.radius));
      file.read(reinterpret_cast<char *>(&device.directionX), sizeof(device.directionX));
      file.read(reinterpret_cast<char *>(&device.directionY), sizeof(device.directionY));
      if (!file || device.x < 0 || device.y < 0 ||
          device.x >= static_cast<int>(w) || device.y >= static_cast<int>(h) ||
          device.materialId < 0 || device.materialId >= 256 ||
          mode > static_cast<std::uint8_t>(ForceMode::Explosion) ||
          !std::isfinite(device.intensity) || device.intensity < 0.1f ||
          device.intensity > 12.0f || !std::isfinite(device.radius) ||
          device.radius < 1.0f || device.radius > 40.0f ||
          !std::isfinite(device.directionX) || !std::isfinite(device.directionY) ||
          std::hypot(device.directionX, device.directionY) < 0.9f ||
          std::hypot(device.directionX, device.directionY) > 1.1f)
        return false;
      device.mode = static_cast<ForceMode>(mode);
      savedEmitters.push_back(device);
    }
  }

  if (width != static_cast<int>(w) || height != static_cast<int>(h))
    resize(static_cast<int>(w), static_cast<int>(h));
  clear();
  environment = savedEnvironment ? Environment::Laboratory : Environment::Natural;
  emitters = std::move(savedEmitters);
  biome = static_cast<Biome>(savedBiome);
  seed = savedSeed;
  ambientTemperature = biome == Biome::Arid ? 0.36f : biome == Biome::Wetland ? 0.15f
                                                                              : 0.06f;
  ambientHumidity = biome == Biome::Arid ? 0.008f : biome == Biome::Wetland ? 0.24f
                                                                            : 0.09f;
  cloudiness = (version5 || version6 || version7 || version8 || version9) ? savedCloudiness : (biome == Biome::Arid ? 0.08f : biome == Biome::Wetland ? 0.67f
                                                                                                                                                      : 0.32f);
  wind = savedWind;
  daylight = (version5 || version6 || version7 || version8 || version9) ? savedDaylight : 0.5f + 0.5f * std::sin(savedTimeOfDay);
  timeOfDay = savedTimeOfDay;
  automaticWeather = savedAutomaticWeather != 0;
  groundLevel = savedGroundLevel;
  stormIntensity = savedStorm;
  stormTicks = savedStormTicks;
  tornadoX = savedTornadoX;
  tornadoTicks = savedTornadoTicks;
  climateOffset = savedClimateOffset;
  dayLengthSeconds = savedDayLength;
  rainEnabled = savedRain != 0;
  dayNightEnabled = savedDayNight != 0;
  reactionEvents = savedReactions;
  const auto &db = MaterialDB::getInstance();
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
    {
      const auto &cell = cells[getIndex(x, y)];
      if (!cell.material)
        continue;
      const auto &def = db.getMaterial(cell.material);
      if (def.id == 0)
        continue;
      setParticle(x, y, cell.material);
      auto &p = grid[getIndex(x, y)];
      p.age = cell.age;
      p.lifetime_counter = cell.lifetime;
      p.burnResidue = cell.burnResidue;
      p.volume = cell.volume;
      p.species = cell.species;
      p.velocityX = cell.velocityX;
      p.velocityY = cell.velocityY;
      p.subcellX = cell.subcellX;
      p.subcellY = cell.subcellY;
      p.damage = cell.damage;
      p.anchored = cell.anchored != 0;
    }
  tempA = tempB = std::move(temp);
  humidA = humidB = std::move(humid);
  nutA = nutB = std::move(nutrient);
  if (version6 || version7 || version8 || version9)
  {
    soilWater = soilWaterNext = std::move(savedSoilWater);
    atmosphericVapor = std::move(savedVapor);
    atmosphericCloud = std::move(savedCloud);
  }
  else
  {
    prepareAtmosphere();
  }
  updatePressure();
  structuralActivityTicks = 240;
  elapsedTime = savedTime;
  for (auto &chunk : chunks)
    chunk.setDirty(true);
  logEvent("Mundo carregado: " + path.filename().string());
  return true;
}

World::Snapshot World::captureSnapshot() const
{
  Snapshot snapshot;
  snapshot.grid = grid;
  snapshot.temperature = usingBufferA ? tempA : tempB;
  snapshot.humidity = usingBufferA ? humidA : humidB;
  snapshot.nutrients = usingBufferA ? nutA : nutB;
  snapshot.soilWater = soilWater;
  snapshot.atmosphericVapor = atmosphericVapor;
  snapshot.atmosphericCloud = atmosphericCloud;
  snapshot.width = width;
  snapshot.height = height;
  snapshot.groundLevel = groundLevel;
  snapshot.elapsedTime = elapsedTime;
  snapshot.timeOfDay = timeOfDay;
  snapshot.daylight = daylight;
  snapshot.rainEnabled = rainEnabled;
  snapshot.dayNightEnabled = dayNightEnabled;
  snapshot.biome = biome;
  snapshot.environment = environment;
  snapshot.emitters = emitters;
  snapshot.seed = seed;
  snapshot.reactions = reactionEvents;
  snapshot.cloudiness = cloudiness;
  snapshot.wind = wind;
  snapshot.automaticWeather = automaticWeather;
  snapshot.stormIntensity = stormIntensity;
  snapshot.stormTicks = stormTicks;
  snapshot.tornadoX = tornadoX;
  snapshot.tornadoTicks = tornadoTicks;
  snapshot.climateOffset = climateOffset;
  snapshot.dayLengthSeconds = dayLengthSeconds;
  return snapshot;
}

void World::restoreSnapshot(const Snapshot &snapshot)
{
  if (snapshot.width < 64 || snapshot.height < 64 ||
      snapshot.width > 1024 || snapshot.height > 1024 ||
      snapshot.grid.size() != static_cast<std::size_t>(snapshot.width) * snapshot.height ||
      snapshot.temperature.size() != snapshot.grid.size() ||
      snapshot.humidity.size() != snapshot.grid.size() ||
      snapshot.nutrients.size() != snapshot.grid.size() ||
      snapshot.soilWater.size() != snapshot.grid.size() ||
      snapshot.atmosphericVapor.size() != static_cast<std::size_t>((snapshot.width + 15) / 16) ||
      snapshot.atmosphericCloud.size() != snapshot.atmosphericVapor.size())
    return;
  if (width != snapshot.width || height != snapshot.height)
    resize(snapshot.width, snapshot.height);
  grid = snapshot.grid;
  tempA = tempB = snapshot.temperature;
  humidA = humidB = snapshot.humidity;
  nutA = nutB = snapshot.nutrients;
  soilWater = soilWaterNext = snapshot.soilWater;
  atmosphericVapor = snapshot.atmosphericVapor;
  atmosphericCloud = snapshot.atmosphericCloud;
  elapsedTime = snapshot.elapsedTime;
  timeOfDay = snapshot.timeOfDay;
  daylight = snapshot.daylight;
  rainEnabled = snapshot.rainEnabled;
  dayNightEnabled = snapshot.dayNightEnabled;
  biome = snapshot.biome;
  environment = snapshot.environment;
  emitters = snapshot.emitters;
  seed = snapshot.seed;
  reactionEvents = snapshot.reactions;
  cloudiness = snapshot.cloudiness;
  wind = snapshot.wind;
  automaticWeather = snapshot.automaticWeather;
  stormIntensity = snapshot.stormIntensity;
  stormTicks = snapshot.stormTicks;
  tornadoX = snapshot.tornadoX;
  tornadoTicks = snapshot.tornadoTicks;
  climateOffset = snapshot.climateOffset;
  dayLengthSeconds = snapshot.dayLengthSeconds;
  groundLevel = std::clamp(snapshot.groundLevel, 0, height - 1);
  updatePressure();
  ambientTemperature = biome == Biome::Arid ? 0.36f : biome == Biome::Wetland ? 0.15f
                                                                              : 0.06f;
  ambientHumidity = biome == Biome::Arid ? 0.008f : biome == Biome::Wetland ? 0.24f
                                                                            : 0.09f;
  usingBufferA = true;
  fieldTickCounter = lifeTickCounter = weatherTickCounter = 0;
  hasLife = std::any_of(grid.begin(), grid.end(), [](const Particle &particle)
                        { return particle.matId == MaterialDB::getInstance().getId("Life"); });
  for (auto &chunk : chunks)
    chunk.setDirty(true);
  structuralActivityTicks = 240;
  logEvent("Estado anterior restaurado");
}

WorldStats World::getStats() const
{
  WorldStats stats;
  stats.daylight = daylight;
  stats.rainIntensity = rainIntensity;
  stats.stormIntensity = stormIntensity;
  stats.tornadoActive = tornadoTicks > 0;
  stats.reactions = reactionEvents;
  for (float cloud : atmosphericCloud)
    stats.cloudCover += cloud;
  stats.cloudCover /= std::max<std::size_t>(1, atmosphericCloud.size());
  const auto &db = MaterialDB::getInstance();
  for (const auto &particle : grid)
  {
    if (!particle.matId)
      continue;
    ++stats.particles;
    if (particle.matId == db.getId("Life"))
      ++stats.lifeCells;
    if (isPlant(particle.matId))
      ++stats.plants;
    if (std::abs(particle.velocityX) + std::abs(particle.velocityY) > 0.3f)
      ++stats.movingParticles;
  }
  float stored = 0, capacity = 0;
  for (std::size_t i = 0; i < grid.size(); ++i)
  {
    const float pore = porosity(grid[i].matId);
    if (pore > 0.0f)
    {
      capacity += pore;
      stored += soilWater[i];
    }
  }
  stats.soilMoisture = capacity > 0 ? clamp01(stored / capacity) : 0.0f;
  return stats;
}

void World::render(sf::RenderWindow &window)
{
  if (graphicsQuality == 0 && width * height > 512 * 512 &&
      (++graphicsFrameCounter % 2u) == 0u)
  {
    window.draw(sprite);
    return;
  }
  const auto &t = usingBufferA ? tempA : tempB;
  const auto &h = usingBufferA ? humidA : humidB;
  const auto &n = usingBufferA ? nutA : nutB;
  const float light = 0.46f + daylight * 0.54f;
  const float dayWeight = clamp01((daylight - 0.12f) / 0.58f);
  const float nightWeight = 1.0f - dayWeight;
  const float sunset = clamp01(1.0f - std::abs(daylight - 0.38f) / 0.24f);
  const bool arid = biome == Biome::Arid;
  const bool wetland = biome == Biome::Wetland;
  float cloudAverage = 0.0f;
  for (float cloud : atmosphericCloud)
    cloudAverage += cloud;
  cloudAverage /= std::max<std::size_t>(1, atmosphericCloud.size());
  const float cloudAmount = clamp01(cloudiness * 0.53f + cloudAverage * 0.36f +
                                    (rainEnabled ? 0.16f : 0.0f) +
                                    stormIntensity * 0.36f);
  const float skylineTone = static_cast<float>(hash32(seed ^ 0x53e1u) % 100u) / 100.0f;
  const sf::Color skyTop = mixColor(sf::Color(9, 15, 33),
                                    arid ? sf::Color(65, 142, 200) : wetland ? sf::Color(49, 112, 150)
                                                                             : mixColor(sf::Color(37, 118, 188), sf::Color(78, 141, 185), skylineTone),
                                    dayWeight);
  const sf::Color skyBottom = mixColor(sf::Color(30, 39, 68),
                                       arid ? sf::Color(236, 183, 125) : wetland ? sf::Color(150, 194, 178)
                                                                                 : sf::Color(151, 203, 217),
                                       dayWeight);
  for (int y = 0; y < height; ++y)
  {
    const float vertical = static_cast<float>(y) / std::max(1, height - 1);
    skyRows[y] = mixColor(skyTop, skyBottom, vertical);
    skyRows[y] = mixColor(skyRows[y], sf::Color(243, 126, 92),
                          sunset * vertical * vertical * 0.55f);
    skyRows[y] = mixColor(skyRows[y], sf::Color(67, 82, 108),
                          cloudAmount * 0.18f * (0.35f + vertical * 0.65f));
    skyRows[y] = mixColor(skyRows[y], sf::Color(30, 42, 63),
                          stormIntensity * 0.58f * (1.0f - vertical * 0.3f));
  }

  const sf::Color farMountain = mixColor(sf::Color(20, 31, 49),
                                         arid ? sf::Color(156, 129, 113) : wetland ? sf::Color(81, 123, 122)
                                                                                   : sf::Color(96, 135, 157),
                                         dayWeight);
  const sf::Color nearMountain = mixColor(sf::Color(12, 24, 36),
                                          arid ? sf::Color(126, 97, 88) : wetland ? sf::Color(37, 87, 79)
                                                                                  : sf::Color(42, 91, 104),
                                          dayWeight);
  const sf::Color distantTrees = mixColor(sf::Color(8, 22, 28),
                                          sf::Color(29, 69, 75), dayWeight);
  for (int x = 0; x < width; ++x)
  {
    const float farX = x + backgroundPanWorld * 0.17f;
    const float nearX = x + backgroundPanWorld * 0.08f;
    const float distant = (noise1D(farX * 0.013f, seed ^ 0xa541u) - 0.5f) * 110.0f +
                          (noise1D(farX * 0.048f, seed ^ 0xf3d5u) - 0.5f) * 18.0f;
    const float nearby = (noise1D(nearX * 0.018f, seed ^ 0x7d1bu) - 0.5f) * 52.0f +
                         (noise1D(nearX * 0.071f, seed ^ 0x593au) - 0.5f) * 16.0f;
    farHeight[x] = static_cast<int>(height * 0.53f + distant);
    nearHeight[x] = static_cast<int>(height * 0.66f + nearby);
    const float driftingX = x * 0.018f + elapsedTime * wind * 0.052f;
    const float cloudNoise = noise1D(driftingX, seed ^ 0xc701u);
    const float textureNoise = noise1D(driftingX * 2.7f, seed ^ 0x3b45u);
    cloudBand[x] = static_cast<int>(height * (0.22f + 0.08f * cloudNoise) +
                                    (textureNoise - 0.5f) * 20.0f);
    const float cloudColumn = atmosphericCloud[static_cast<std::size_t>(x / 16)];
    cloudWeight[x] = noise1D(x * 0.025f + elapsedTime * wind * 0.04f,
                             seed ^ 0x58e9u) *
                     (0.43f + 0.65f * clamp01(cloudColumn * 0.6f));
  }

  const float phase = timeOfDay;
  const float orbit = std::cos(phase);
  const int sunX = static_cast<int>(width * (0.5f + orbit * 0.40f));
  const int sunY = static_cast<int>(height * (0.70f - daylight * 0.60f));
  const int moonX = static_cast<int>(width * (0.5f - orbit * 0.40f));
  const int moonY = static_cast<int>(height * (0.70f - (1.0f - daylight) * 0.60f));
  const auto &db = MaterialDB::getInstance();
  const int waterId = db.getId("Water"), saltwaterId = db.getId("Saltwater");
  const int fireId = db.getId("Fire"), lavaId = db.getId("Lava");
  const int moltenMetalId = db.getId("MoltenMetal");
  const int moltenGlassId = db.getId("MoltenGlass");
  const int glassId = db.getId("Glass");

  const int tilesX = (width + 7) / 8;
  const int tilesY = (height + 7) / 8;
  if (heatGlowEnabled && graphicsQuality > 0 && renderMode == RenderMode::Normal)
  {
    glowTiles.assign(tilesX * tilesY, 0);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
      {
        const int material = grid[getIndex(x, y)].matId;
        if (material != fireId && material != lavaId &&
            material != moltenMetalId && material != moltenGlassId)
          continue;
        const float heatStrength = std::clamp((t[getIndex(x, y)] - 0.30f) / 0.70f,
                                              0.0f, 1.0f);
        auto &tile = glowTiles[(y / 8) * tilesX + x / 8];
        tile = std::max(tile, static_cast<unsigned char>(
                                  (material == fireId ? 215 : material == lavaId ? 170
                                                                                 : 130) *
                                  heatStrength));
      }
    const std::vector<unsigned char> emitters = glowTiles;
    for (int pass = 0; pass < graphicsQuality + 1; ++pass)
    {
      glowBuffer = glowTiles;
      for (int ty = 0; ty < tilesY; ++ty)
        for (int tx = 0; tx < tilesX; ++tx)
        {
          int sum = glowTiles[ty * tilesX + tx] * 4;
          int weight = 4;
          for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
              if ((dx || dy) && tx + dx >= 0 && tx + dx < tilesX &&
                  ty + dy >= 0 && ty + dy < tilesY)
              {
                sum += glowTiles[(ty + dy) * tilesX + tx + dx];
                ++weight;
              }
          glowBuffer[ty * tilesX + tx] = static_cast<unsigned char>(
              std::max(static_cast<int>(emitters[ty * tilesX + tx]), sum / weight));
        }
      glowTiles.swap(glowBuffer);
    }
  }

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
    {
      const int i = getIndex(x, y);
      const int material = grid[i].matId;
      const auto &c = grid[i].color;
      float r = c.r, g = c.g, b = c.b;
      if (renderMode == RenderMode::Temperature)
      {
        r = std::max(0.0f, t[i]) * 255;
        g = 0;
        b = std::max(0.0f, -t[i]) * 255;
      }
      else if (renderMode == RenderMode::Humidity)
      {
        r = 0;
        g = h[i] * 170;
        b = h[i] * 255;
      }
      else if (renderMode == RenderMode::Nutrients)
      {
        r = n[i] * 70;
        g = n[i] * 225;
        b = n[i] * 65;
      }
      else if (renderMode == RenderMode::Saturation)
      {
        const float saturated = getSoilSaturation(x, y);
        r = 34.0f + saturated * 32.0f;
        g = 38.0f + saturated * 151.0f;
        b = 47.0f + saturated * 198.0f;
      }
      else if (renderMode == RenderMode::Pressure)
      {
        const float pressure = pressureField[i];
        r = pressure * 238.0f;
        g = pressure * 99.0f;
        b = 45.0f + pressure * 104.0f;
      }
      else if (renderMode == RenderMode::Stress)
      {
        const float damage = clamp01(grid[i].damage);
        r = material ? 28.0f + damage * 222.0f : 11.0f;
        g = material ? 115.0f - damage * 77.0f : 18.0f;
        b = grid[i].anchored ? 231.0f : material ? 75.0f - damage * 54.0f
                                                 : 25.0f;
      }
      else if (renderMode == RenderMode::Velocity)
      {
        const float speed = clamp01(std::sqrt(
                                        grid[i].velocityX * grid[i].velocityX +
                                        grid[i].velocityY * grid[i].velocityY) /
                                    6.0f);
        r = material ? 32.0f + speed * 232.0f : 10.0f;
        g = material ? 75.0f + speed * 111.0f : 19.0f;
        b = material ? 91.0f + speed * 60.0f : 30.0f;
      }
      else if (!material && environment == Environment::Laboratory)
      {
        const bool major = x % 64 == 0 || y % 64 == 0;
        const bool minor = x % 16 == 0 || y % 16 == 0;
        const float shade = major ? 40.0f : minor ? 32.0f
                                                  : 25.0f;
        r = shade;
        g = shade + 7.0f;
        b = shade + 12.0f;
      }
      else if (!material)
      {
        sf::Color background = skyRows[y];
        if (y >= nearHeight[x])
          background = nearMountain;
        else if (y >= farHeight[x])
          background = farMountain;
        else
        {
          const int sunDistance = (x - sunX) * (x - sunX) + (y - sunY) * (y - sunY);
          const int moonDistance = (x - moonX) * (x - moonX) + (y - moonY) * (y - moonY);
          if (dayWeight > 0.1f && sunDistance < 11 * 11)
            background = mixColor(background, sf::Color(255, 245, 191), dayWeight);
          else if (nightWeight > 0.25f && moonDistance < 8 * 8)
            background = mixColor(background, sf::Color(210, 222, 233), nightWeight);
          else if (nightWeight > 0.4f && y < height * 0.50f &&
                   (hash32(seed ^ (static_cast<std::uint32_t>(x) * 73856093u) ^
                           (static_cast<std::uint32_t>(y) * 19349663u)) %
                        1800u ==
                    0))
            background = mixColor(background, sf::Color(232, 237, 251), nightWeight);
          const int cloudDistance = std::abs(y - cloudBand[x]);
          const float driftScale = cloudWeight[x];
          if (cloudAmount > 0.03f && driftScale > 0.28f &&
              cloudDistance < 7 + static_cast<int>(driftScale * 11.0f))
          {
            const float feather = clamp01((19.0f - cloudDistance) / 19.0f);
            const float coverage = clamp01((driftScale - 0.28f) * 4.0f);
            background = mixColor(background,
                                  mixColor(sf::Color(83, 96, 120), sf::Color(226, 229, 219), dayWeight),
                                  cloudAmount * feather * coverage * (0.18f + 0.33f * driftScale));
          }
        }
        if (!arid && y < nearHeight[x] && y >= farHeight[x])
        {
          const int sceneX = static_cast<int>(std::floor(x + backgroundPanWorld * 0.08f));
          const int group = static_cast<int>(std::floor(sceneX / 22.0f));
          const std::uint32_t tree = hash32(static_cast<std::uint32_t>(group) ^ seed ^ 0x47f2u);
          const int center = group * 22 + 11 + static_cast<int>(tree % 7u) - 3;
          const int elevation = nearHeight[x] - y;
          const int treeHeight = 14 + static_cast<int>((tree >> 9) % 17u);
          const int treeHalfWidth = (treeHeight - elevation) / 3;
          if (elevation > 0 && elevation < treeHeight &&
              std::abs(sceneX - center) <= std::max(1, treeHalfWidth))
            background = distantTrees;
        }
        if (renderMode == RenderMode::Normal && !backgroundLayers.empty() &&
            y < groundLevel)
        {
          const int firstLayer = arid ? 4 : wetland ? 2
                                                    : 0;
          const int count = arid ? 1 : 2;
          for (int layerIndex = firstLayer; layerIndex < firstLayer + count; ++layerIndex)
          {
            const auto &layer = backgroundLayers[static_cast<std::size_t>(layerIndex)];
            if (layer.width == 0 || layer.height == 0)
              continue;
            const int sourceY = y - (groundLevel - layer.height);
            if (sourceY < 0 || sourceY >= layer.height)
              continue;
            const int shift = static_cast<int>(backgroundPanWorld * layer.parallax) +
                              static_cast<int>(seed % static_cast<std::uint32_t>(layer.width));
            const int sourceX = ((x + shift) % layer.width + layer.width) % layer.width;
            const auto pixelOffset = (static_cast<std::size_t>(sourceY) * layer.width +
                                      sourceX) *
                                     4;
            const float opacity = layer.rgba[pixelOffset + 3] / 255.0f * 0.86f;
            if (opacity < 0.02f)
              continue;
            const float tint = 0.38f + 0.62f * dayWeight;
            const sf::Color art(
                static_cast<sf::Uint8>(layer.rgba[pixelOffset] * tint),
                static_cast<sf::Uint8>(layer.rgba[pixelOffset + 1] * tint),
                static_cast<sf::Uint8>(layer.rgba[pixelOffset + 2] * tint));
            background = mixColor(background, art, opacity);
          }
        }
        if (graphicsQuality >= 2 && y + 1 < height &&
            (grid[i + width].matId == waterId ||
             grid[i + width].matId == saltwaterId) &&
            grid[i + width].updated_this_frame &&
            hash32(static_cast<std::uint32_t>(x) ^
                   static_cast<std::uint32_t>(y * 131) ^
                   static_cast<std::uint32_t>(elapsedTime * 60)) %
                    17u ==
                0)
          background = mixColor(background, sf::Color(151, 207, 238), 0.7f);
        r = background.r;
        g = background.g;
        b = background.b;
      }
      else
      {
        float surfaceLight = material == fireId || material == lavaId ||
                                     material == moltenMetalId ||
                                     material == moltenGlassId
                                 ? 1.0f
                                 : light;
        surfaceLight = std::clamp(surfaceLight * lightingStrength, 0.08f, 1.65f);
        if (graphicsQuality > 0 && y > 0 && material &&
            grid[i - width].matId == 0 && material != fireId)
        {
          surfaceLight = std::min(1.12f, surfaceLight * 1.10f);
        }
        else if (graphicsQuality >= 2 && x > 0 && y > 0 &&
                 material && grid[i - 1].matId != 0 &&
                 grid[i - width].matId != 0)
        {
          surfaceLight *= 0.94f;
        }
        const float moistureShade = getSoilSaturation(x, y);
        const float wetDarkening = 1.0f - moistureShade * 0.35f;
        r *= surfaceLight * wetDarkening;
        g *= surfaceLight * wetDarkening;
        b *= surfaceLight * wetDarkening;
        if (material == waterId || material == saltwaterId)
        {
          const float depthOpacity = 0.56f + 0.44f * std::sqrt(
                                                         std::clamp(grid[i].volume, 0.0f, 1.0f));
          r *= depthOpacity;
          g *= depthOpacity;
          b *= depthOpacity;
        }
        if (material == lavaId || material == moltenMetalId ||
            material == moltenGlassId)
        {
          const float hot = clamp01((t[i] - 0.32f) / 0.68f);
          r *= 0.42f + 0.58f * hot;
          g *= 0.25f + 0.75f * hot;
          b *= 0.32f + 0.68f * hot;
        }
        if (grid[i].damage > 0.09f)
        {
          const float damage = std::clamp(grid[i].damage, 0.0f, 1.8f);
          const unsigned crack = hash32(static_cast<std::uint32_t>(x * 73856093u) ^
                                        static_cast<std::uint32_t>(y * 19349663u) ^ seed);
          const float fracture = crack % 100u < static_cast<unsigned>(damage * 36.0f)
                                     ? 0.34f
                                     : (1.0f - damage * 0.13f);
          r *= fracture;
          g *= fracture;
          b *= fracture;
        }
        if ((material == waterId || material == saltwaterId) && y > 0 &&
            grid[i - width].matId == 0)
        {
          const float shimmer = 0.5f + 0.5f * std::sin(x * 0.31f + elapsedTime * 2.4f);
          const float highlight = (12.0f + 27.0f * shimmer) * dayWeight;
          r += highlight * 0.60f;
          g += highlight;
          b += highlight;
        }
        else if (material == glassId)
        {
          b += 12.0f * dayWeight;
        }
      }
      if (heatGlowEnabled && graphicsQuality > 0 && renderMode == RenderMode::Normal)
      {
        const int tx = x / 8;
        const int ty = y / 8;
        const int ntx = std::min(tx + 1, tilesX - 1);
        const int nty = std::min(ty + 1, tilesY - 1);
        const int fx = x % 8, fy = y % 8;
        const int topGlow = glowTiles[ty * tilesX + tx] * (8 - fx) +
                            glowTiles[ty * tilesX + ntx] * fx;
        const int bottomGlow = glowTiles[nty * tilesX + tx] * (8 - fx) +
                               glowTiles[nty * tilesX + ntx] * fx;
        const int glow = (topGlow * (8 - fy) + bottomGlow * fy) / 64;
        const float glowWeight = (0.20f + nightWeight * 0.42f) * glow;
        r += glowWeight * 0.45f;
        g += glowWeight * 0.21f;
        b += glowWeight * 0.09f;
      }
      pixelData[i * 4] = static_cast<sf::Uint8>(std::clamp(r, 0.0f, 255.0f));
      pixelData[i * 4 + 1] = static_cast<sf::Uint8>(std::clamp(g, 0.0f, 255.0f));
      pixelData[i * 4 + 2] = static_cast<sf::Uint8>(std::clamp(b, 0.0f, 255.0f));
      pixelData[i * 4 + 3] = 255;
    }
  if (renderMode == RenderMode::Normal)
    for (const auto &emitter : emitters)
    {
      for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
        {
          const int px = emitter.x + dx, py = emitter.y + dy;
          if (!inBounds(px, py))
            continue;
          const bool rim = std::abs(dx) == 3 || std::abs(dy) == 3;
          const bool core = std::abs(dx) <= 1 && std::abs(dy) <= 1;
          if (!rim && !core)
            continue;
          const int at = getIndex(px, py) * 4;
          pixelData[at] = rim ? 57 : 129;
          pixelData[at + 1] = rim ? 237 : 246;
          pixelData[at + 2] = rim ? 171 : 221;
        }
      const int nx = emitter.x + static_cast<int>(std::round(emitter.directionX * 5.0f));
      const int ny = emitter.y + static_cast<int>(std::round(emitter.directionY * 5.0f));
      if (inBounds(nx, ny))
      {
        const int at = getIndex(nx, ny) * 4;
        pixelData[at] = 255;
        pixelData[at + 1] = 181;
        pixelData[at + 2] = 68;
      }
    }
  texture.update(pixelData.data());
  window.draw(sprite);
}
