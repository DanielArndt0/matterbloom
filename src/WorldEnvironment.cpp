#include "World.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
  float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
}

float World::porosity(int materialId) const
{
  return MaterialDB::getInstance().getMaterial(materialId).porosity;
}

float World::getSoilSaturation(int x, int y) const
{
  if (!inBounds(x, y))
    return 0.0f;
  const int index = getIndex(x, y);
  const float capacity = porosity(grid[index].matId);
  return capacity > 0.001f ? clamp01(soilWater[index] / capacity) : 0.0f;
}

float World::getPressure(int x, int y) const
{
  return inBounds(x, y) ? pressureField[getIndex(x, y)] : 0.0f;
}

void World::releaseVapor(int x, float amount)
{
  if (x < 0 || x >= width || amount <= 0.0f || atmosphericVapor.empty())
    return;
  auto &vapor = atmosphericVapor[static_cast<std::size_t>(x / 16)];
  vapor = std::min(3.0f, vapor + amount);
}

void World::prepareAtmosphere()
{
  const float humid = biome == Biome::Wetland ? 2.6f : biome == Biome::Arid ? 0.32f
                                                                            : 1.25f;
  const float clouds = biome == Biome::Wetland ? 1.35f : biome == Biome::Arid ? 0.08f
                                                                              : 0.58f;
  const unsigned bandCount = static_cast<unsigned>(atmosphericVapor.size());
  for (unsigned band = 0; band < bandCount; ++band)
  {
    const unsigned variation = (seed * 1664525u + band * 1013904223u) ^
                               ((seed + band * 7919u) >> 9);
    const float variety = 0.68f + (variation % 81u) / 100.0f;
    atmosphericVapor[band] = humid * variety;
    atmosphericCloud[band] = clouds * variety;
  }
  rainIntensity = 0.0f;
}

void World::updateHydrology()
{
  const auto &db = MaterialDB::getInstance();
  const int waterId = db.getId("Water");
  const int saltwaterId = db.getId("Saltwater");
  const int mudId = db.getId("Mud");
  const int soilId = db.getId("Soil");
  const int siltId = db.getId("Silt");
  const int shaleId = db.getId("Shale");
  const int fracturedId = db.getId("FracturedRock");
  const int rootsId = db.getId("Roots");
  soilWaterNext = soilWater;

  for (int y = 1; y < height - 1; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      const int index = getIndex(x, y);
      Particle &particle = grid[index];
      const int materialId = particle.matId;
      const float capacity = porosity(materialId);

      if (materialId == waterId || materialId == saltwaterId)
      {
        for (int offset : {0, -1, 1})
        {
          const int nx = x + offset;
          const int ny = y + (offset == 0 ? 1 : 0);
          if (!inBounds(nx, ny))
            continue;
          const int target = getIndex(nx, ny);
          const auto &def = db.getMaterial(grid[target].matId);
          if (def.porosity < 0.025f || def.permeability <= 0.0f)
            continue;
          const float availability = std::max(0.0f, def.porosity - soilWaterNext[target]);
          const float absorbed = std::min({particle.volume, availability,
                                           0.036f + def.permeability * 0.13f});
          if (absorbed <= 0.0001f)
            continue;
          soilWaterNext[target] += absorbed;
          particle.volume -= absorbed;
          changeHumidity(nx, ny, absorbed * 0.26f);
          wakeAt(nx, ny);
          if (particle.volume < 0.03f)
          {
            atmosphericVapor[static_cast<std::size_t>(x / 16)] += particle.volume;
            setParticle(x, y, 0);
            break;
          }
        }
      }
      else if (capacity > 0.01f && soilWater[index] > 0.0005f)
      {
        const auto &def = db.getMaterial(materialId);
        if (y + 1 < height)
        {
          const int below = getIndex(x, y + 1);
          const auto &under = db.getMaterial(grid[below].matId);
          if (under.porosity > 0.025f)
          {
            const float room = std::max(0.0f, under.porosity - soilWaterNext[below]);
            const float flux = std::min({room, soilWaterNext[index],
                                         0.005f * std::min(def.permeability, under.permeability)});
            soilWaterNext[index] -= flux;
            soilWaterNext[below] += flux;
          }
        }
        if (x + 1 < width)
        {
          const int right = getIndex(x + 1, y);
          const auto &neighbor = db.getMaterial(grid[right].matId);
          if (neighbor.porosity > 0.02f)
          {
            const float leftSaturation = soilWater[index] / capacity;
            const float rightSaturation = soilWater[right] / neighbor.porosity;
            const float transfer = (leftSaturation - rightSaturation) *
                                   std::min(def.permeability, neighbor.permeability) * 0.006f;
            const float signedTransfer = transfer > 0 ? std::min({transfer, soilWaterNext[index], neighbor.porosity - soilWaterNext[right]}) : -std::min({-transfer, soilWaterNext[right], capacity - soilWaterNext[index]});
            soilWaterNext[index] -= signedTransfer;
            soilWaterNext[right] += signedTransfer;
          }
        }
        if (y > 0 && grid[getIndex(x, y - 1)].matId == 0)
        {
          const float evaporated = std::min(soilWaterNext[index],
                                            (0.0004f + 0.001f * daylight) * (0.2f + std::max(0.0f, getTemperature(x, y))));
          soilWaterNext[index] -= evaporated;
          atmosphericVapor[static_cast<std::size_t>(x / 16)] += evaporated;
        }
      }
    }
  }

  soilWater.swap(soilWaterNext);
  for (int y = 1; y < height - 1; ++y)
    for (int x = 1; x < width - 1; ++x)
    {
      const int index = getIndex(x, y);
      const int materialId = grid[index].matId;
      const float capacity = porosity(materialId);
      if (capacity < 0.08f || soilWater[index] < capacity * 0.92f)
        continue;
      const int above = getIndex(x, y - 1);
      const int side = (std::rand() & 1) ? 1 : -1;
      const bool exposed = grid[above].matId == waterId ||
                           grid[above].matId == saltwaterId ||
                           grid[getIndex(x + side, y)].matId == 0;
      if (!exposed)
        continue;
      const bool rooted = grid[getIndex(x - 1, y)].matId == rootsId ||
                          grid[getIndex(x + 1, y)].matId == rootsId ||
                          grid[getIndex(x, y + 1)].matId == rootsId;
      if ((materialId == soilId || materialId == siltId) &&
          std::rand() % (rooted ? 4600 : 950) == 0)
      {
        const float amount = soilWater[index];
        soilWater[index] = 0;
        setParticle(x, y, mudId);
        grid[index].volume = std::max(0.35f, amount);
      }
      else if (materialId == shaleId &&
               pressureField[index] > db.getMaterial(materialId).pressureResistance &&
               std::rand() % 3000 == 0)
        setParticle(x, y, fracturedId);
    }

  for (int y = 0; y < height - 1; ++y)
    for (int x = 0; x < width; ++x)
    {
      const int index = getIndex(x, y);
      Particle &p = grid[index];
      if ((p.matId != waterId && p.matId != saltwaterId) || y == 0 ||
          grid[index - width].matId != 0)
        continue;
      const float evaporation = std::min(p.volume,
                                         (0.001f + 0.002f * daylight) *
                                             (0.2f + std::max(0.0f, getTemperature(x, y))) *
                                             (1.0f + std::abs(wind) * 0.1f));
      p.volume -= evaporation;
      atmosphericVapor[static_cast<std::size_t>(x / 16)] += evaporation;
      if (p.volume < 0.025f)
      {
        atmosphericVapor[static_cast<std::size_t>(x / 16)] += p.volume;
        setParticle(x, y, 0);
      }
    }
  if (++pressureTickCounter >= 1)
  {
    pressureTickCounter = 0;
    updatePressure();
  }
}

void World::updatePressure()
{
  const auto &db = MaterialDB::getInstance();
  for (int x = 0; x < width; ++x)
  {
    float fluidHead = 0.0f;
    float overburden = 0.0f;
    for (int y = 0; y < height; ++y)
    {
      const int index = getIndex(x, y);
      const auto &particle = grid[index];
      const auto &def = db.getMaterial(particle.matId);
      const bool liquid = def.state == MatterState::Liquid;
      const bool gas = def.state == MatterState::Gas || def.state == MatterState::Plasma;
      if (liquid)
      {
        fluidHead = std::min(1.0f, fluidHead + 0.0041f *
                                                   std::max(0.25f, def.density) * std::max(0.1f, particle.volume) *
                                                   def.pressureResponse);
        overburden *= 0.85f;
      }
      else if (gas)
      {
        fluidHead = std::max(0.0f, fluidHead - 0.025f);
        overburden *= 0.94f;
      }
      else if (particle.matId)
      {
        fluidHead *= 0.14f;
        overburden = std::min(0.94f, overburden +
                                         std::max(0.0f, def.density) * 0.00042f * def.pressureResponse);
      }
      else
      {
        fluidHead = 0.0f;
        overburden *= 0.84f;
      }
      const float saturation = getSoilSaturation(x, y);
      const float motion = (particle.velocityX * particle.velocityX +
                            particle.velocityY * particle.velocityY) *
                           std::min(4.0f, std::abs(def.density)) * 0.004f;
      const float atmosphere = gas ? 0.025f +
                                         (0.065f + def.compressibility * 0.050f) *
                                             std::max(0.0f, getTemperature(x, y))
                                   : 0.0f;
      pressureField[index] = clamp01(fluidHead + overburden +
                                     saturation * 0.27f + atmosphere + motion);
      if (liquid && fluidHead > 0.45f && y > 1 && y < height - 1 &&
          std::rand() % 600 == 0)
      {
        for (int side : {-1, 1})
        {
          if (!inBounds(x + side, y))
            continue;
          const int target = grid[getIndex(x + side, y)].matId;
          if (target == db.getId("Brick") || target == db.getId("Concrete") ||
              target == db.getId("Wall") || target == db.getId("Glass"))
            damageStructure(x + side, y, fluidHead * def.pressureResponse * 0.10f);
        }
      }
    }
  }
}

void World::updateWeather()
{
  if (++weatherTickCounter % 3 != 0)
    return;
  if (atmosphericCloud.empty())
    return;
  const int waterId = MaterialDB::getInstance().getId("Water");
  const std::size_t bands = atmosphericCloud.size();
  float meanClouds = 0.0f;
  for (const float cloud : atmosphericCloud)
    meanClouds += cloud;
  meanClouds /= static_cast<float>(bands);
  if (automaticWeather && stormTicks <= 0 && meanClouds > 0.60f &&
      std::rand() % 260 == 0)
  {
    stormTicks = 130 + std::rand() % 200;
    stormIntensity = std::clamp((meanClouds - 0.40f) * 0.75f + 0.25f, 0.2f, 1.0f);
    logEvent("Uma tempestade se formou");
  }
  if (stormTicks > 0)
  {
    --stormTicks;
    wind = std::clamp(wind + (wind < 0 ? -0.0035f : 0.0035f) *
                                 stormIntensity,
                      -2.8f, 2.8f);
    if (stormIntensity > 0.68f && stormTicks % 37 == 0 && std::rand() % 4 == 0)
    {
      const int strikeX = std::rand() % width;
      for (int strikeY = 85; strikeY < height - 1; ++strikeY)
      {
        if (grid[getIndex(strikeX, strikeY)].matId == 0)
          continue;
        changeTemperature(strikeX, strikeY, 1.0f);
        applyForce(strikeX, strikeY, 6.0f, 1.8f, ForceMode::Impulse);
        logEvent("Raio atingiu o solo em " + std::to_string(strikeX));
        break;
      }
    }
  }
  else if (stormIntensity > 0.0f)
  {
    stormIntensity = std::max(0.0f, stormIntensity - 0.018f);
    if (stormIntensity == 0.0f)
      logEvent("A tempestade passou");
  }
  if (automaticWeather && tornadoTicks == 0 && stormIntensity > 0.76f &&
      std::abs(wind) > 0.4f && std::rand() % 160 == 0)
  {
    tornadoTicks = 95 + std::rand() % 80;
    tornadoX = static_cast<float>(std::rand() % width);
    logEvent("Tornado se formou no horizonte");
  }
  if (tornadoTicks > 0)
  {
    --tornadoTicks;
    tornadoX = std::clamp(tornadoX + (wind < 0 ? -1.0f : 1.0f) *
                                         (0.8f + std::abs(wind) * 0.7f),
                          0.0f, static_cast<float>(width - 1));
    const int funnelY = std::clamp(groundLevel - 13, 12, height - 12);
    applyForce(static_cast<int>(tornadoX), funnelY, 18.0f,
               1.5f + stormIntensity * 3.0f, ForceMode::Launch,
               wind < 0 ? -0.35f : 0.35f, -1.0f);
    if (tornadoTicks == 0)
      logEvent("O tornado se dissipou");
  }
  if (automaticWeather)
  {
    for (std::size_t i = 0; i < bands; ++i)
    {
      const float colder = 1.0f - daylight * 0.35f;
      const float threshold = 0.65f +
                              std::clamp(ambientTemperature + climateOffset, -1.0f, 1.0f) * 1.8f;
      if (atmosphericVapor[i] > threshold)
      {
        const float condensation = std::min(atmosphericVapor[i] - threshold,
                                            0.0075f * colder);
        atmosphericVapor[i] -= condensation;
        atmosphericCloud[i] += condensation;
      }
      if (atmosphericCloud[i] > 0.0f && daylight > 0.64f)
      {
        const float release = std::min(atmosphericCloud[i], 0.0017f);
        atmosphericCloud[i] -= release;
        atmosphericVapor[i] += release;
      }
    }
  }
  if (std::abs(wind) > 0.035f)
  {
    const float fluxRate = std::min(0.17f, std::abs(wind) * 0.006f);
    const int direction = wind >= 0 ? 1 : -1;
    for (std::size_t i = 0; i < bands; ++i)
    {
      const std::size_t to = (i + (direction > 0 ? 1u : bands - 1u)) % bands;
      const float vaporFlow = atmosphericVapor[i] * fluxRate;
      const float cloudFlow = atmosphericCloud[i] * fluxRate;
      atmosphericVapor[i] -= vaporFlow;
      atmosphericVapor[to] += vaporFlow;
      atmosphericCloud[i] -= cloudFlow;
      atmosphericCloud[to] += cloudFlow;
    }
  }

  if (!automaticWeather && !rainEnabled)
  {
    rainIntensity = 0.0f;
    return;
  }

  float rainSum = 0.0f;
  for (std::size_t band = 0; band < bands; ++band)
  {
    if (rainEnabled && atmosphericCloud[band] < 1.25f)
    {
      const float supplied = 0.015f;
      atmosphericCloud[band] += supplied;
    }
    const float cloud = atmosphericCloud[band];
    const float threshold = rainEnabled ? 0.63f : stormIntensity > 0.1f ? 0.75f
                                                                        : 1.1f;
    if (cloud < threshold)
      continue;
    const float probability = std::clamp((cloud - threshold) * 0.12f +
                                             stormIntensity * 0.14f,
                                         0.015f, 0.46f);
    rainSum += probability;
    if (static_cast<float>(std::rand()) / RAND_MAX > probability)
      continue;
    const int x = std::min(width - 1, static_cast<int>(band * 16) + std::rand() % 16);
    const int skyY = std::clamp(65 + std::rand() % 32, 0, height - 1);
    if (grid[getIndex(x, skyY)].matId != 0)
      continue;
    const float drop = std::min(cloud - 0.15f, 0.15f + probability * 0.25f);
    if (drop <= 0)
      continue;
    setParticle(x, skyY, waterId);
    grid[getIndex(x, skyY)].volume = drop;
    atmosphericCloud[band] -= drop;
  }
  rainIntensity = clamp01(rainSum / static_cast<float>(bands));
}
