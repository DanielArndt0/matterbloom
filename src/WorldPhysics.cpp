#include "World.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
  bool structural(int id)
  {
    static const std::array<bool, 256> entries = []
    {
      std::array<bool, 256> result{};
      const auto &db = MaterialDB::getInstance();
      for (int id = 1; id < 256; ++id)
      {
        const auto &material = db.getMaterial(id);
        if (material.id != id || material.state != MatterState::Solid)
          continue;
        bool organic = false;
        for (const char *name : {"Plant", "Moss", "Algae", "Grass", "Flower",
                                 "PineNeedles", "Mushroom", "Mycelium", "Shrub", "Roots"})
          if (material.name == name)
            organic = true;
        result[static_cast<std::size_t>(id)] = !organic;
      }
      return result;
    }();
    return id > 0 && id < 256 && entries[static_cast<std::size_t>(id)];
  }

  float strengthFor(const MaterialDef &def)
  {
    return std::max(0.24f, 0.32f + def.pressureResistance * 0.72f +
                               def.cohesion * 0.58f);
  }

  bool bonded(const Particle &left, const Particle &right, int first, int second,
              const MaterialDef &material)
  {
    const float stress = (left.damage + right.damage) /
                         (2.0f * strengthFor(material));
    if (stress < 0.40f)
      return true;
    const std::uint32_t hash =
        (static_cast<std::uint32_t>(std::min(first, second)) * 0x9e3779b9u) ^
        (static_cast<std::uint32_t>(std::max(first, second)) * 0x85ebca6bu);
    return stress < 0.44f + static_cast<float>((hash >> 12) & 255u) * (0.42f / 255.0f);
  }

  float distanceFalloff(float distance, float radius)
  {
    const float value = std::max(0.0f, 1.0f - distance / std::max(1.0f, radius));
    return value * value * (3.0f - 2.0f * value);
  }
}

bool World::isStructural(int materialId) const { return structural(materialId); }

void World::logEvent(const std::string &message)
{
  recentEvents.push_front(message);
  while (recentEvents.size() > 14)
    recentEvents.pop_back();
}

void World::damageStructure(int x, int y, float amount)
{
  if (!inBounds(x, y) || amount <= 0.0f)
    return;
  const int index = getIndex(x, y);
  Particle &particle = grid[index];
  if (!particle.matId)
    return;
  const auto &db = MaterialDB::getInstance();
  const auto &material = db.getMaterial(particle.matId);
  if (material.state != MatterState::Solid)
    return;
  structuralActivityTicks = 240;
  particle.damage = std::min(8.0f, particle.damage + amount / (particle.anchored ? 1.9f : 1.0f));
  const float threshold = strengthFor(material);
  if (particle.damage >= threshold * 0.42f && amount >= 0.12f)
  {
    const float transfer = std::min(threshold * 0.42f, amount * 0.35f);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
      {
        if ((!dx && !dy) || !inBounds(x + dx, y + dy))
          continue;
        auto &neighbor = grid[getIndex(x + dx, y + dy)];
        if (neighbor.matId != particle.matId)
          continue;
        const float orientation = (dx && dy) ? 0.35f : 0.70f;
        neighbor.damage = std::min(threshold * 1.60f,
                                   neighbor.damage + transfer * orientation /
                                                         (neighbor.anchored ? 1.9f : 1.0f));
        wakeAt(x + dx, y + dy);
      }
  }
  if (particle.damage < threshold * 1.65f)
  {
    wakeAt(x, y);
    return;
  }

  const float oldX = particle.velocityX;
  const float oldY = particle.velocityY;
  const bool organic = material.name == "Wood" || material.name == "Roots";
  int debris = 0;
  if (organic)
    debris = db.getId("Wood");
  else if (material.name == "Glass" || material.name == "Brick" ||
           material.name == "Wall" || material.name == "Concrete")
    debris = db.getId("Gravel");
  else if (material.name == "Metal" || material.name == "ReinforcedConcrete")
    debris = db.getId("Metal");
  else
    debris = db.getId("FracturedRock");
  setParticle(x, y, debris);
  auto &fragment = grid[index];
  fragment.velocityX = std::clamp(oldX * 0.7f + (std::rand() % 101 - 50) * 0.012f, -5.0f, 5.0f);
  fragment.velocityY = std::clamp(oldY * 0.7f - 0.7f, -5.0f, 5.0f);
  fragment.damage = 0.0f;
  fragment.anchored = false;
  ++reactionEvents;
}

int World::applyForce(int x, int y, float radius, float intensity, ForceMode mode,
                      float directionX, float directionY,
                      int selectedMaterial, bool directed)
{
  if (!inBounds(x, y))
    return 0;
  radius = std::clamp(radius, 1.0f, 96.0f);
  intensity = std::clamp(intensity, 0.05f, 12.0f);
  const float length = std::sqrt(directionX * directionX + directionY * directionY);
  directionX = length > 0.0001f ? directionX / length : 1.0f;
  directionY = length > 0.0001f ? directionY / length : 0.0f;
  const int r = static_cast<int>(std::ceil(radius));
  const auto &db = MaterialDB::getInstance();
  const int left = std::max(0, x - r), right = std::min(width - 1, x + r);
  const int top = std::max(0, y - r), bottom = std::min(height - 1, y + r);
  int touched = 0;

  for (int yy = top; yy <= bottom; ++yy)
  {
    for (int xx = left; xx <= right; ++xx)
    {
      const float dx = static_cast<float>(xx - x), dy = static_cast<float>(yy - y);
      const float distance = std::sqrt(dx * dx + dy * dy);
      if (distance > radius)
        continue;
      const int index = getIndex(xx, yy);
      Particle &particle = grid[index];
      if (!particle.matId)
        continue;
      if (mode == ForceMode::Anchor || mode == ForceMode::Release)
      {
        if (!structural(particle.matId))
          continue;
        particle.anchored = mode == ForceMode::Anchor;
        structuralActivityTicks = 240;
        if (particle.anchored)
          particle.velocityX = particle.velocityY = 0.0f;
        else
        {
          particle.damage = std::max(particle.damage, 0.04f);
          particle.velocityY += 0.65f;
        }
        ++touched;
        wakeAt(xx, yy);
        continue;
      }

      const auto &def = db.getMaterial(particle.matId);
      const float falloff = distanceFalloff(distance, radius);
      const float mass = std::clamp(std::abs(def.density), 0.30f, 5.0f);
      float fx = directionX, fy = directionY;
      if ((mode == ForceMode::Impulse && !directed) || mode == ForceMode::Explosion ||
          mode == ForceMode::Attraction)
      {
        const float divisor = std::max(1.0f, distance);
        fx = dx / divisor;
        fy = dy / divisor;
        if (mode == ForceMode::Attraction)
        {
          fx = -fx;
          fy = -fy;
        }
      }
      if (mode == ForceMode::Fan && def.state == MatterState::Solid &&
          !structural(particle.matId))
        continue;
      const float impulse = std::min(6.0f, intensity * falloff *
                                               (mode == ForceMode::Fan ? 0.24f : mode == ForceMode::Jet ? 0.52f
                                                                                                        : 0.88f) /
                                               std::sqrt(mass));
      if (mode == ForceMode::Attraction && def.name == "Metal")
        particle.velocityX += fx * impulse * 1.6f;
      else
        particle.velocityX += fx * impulse;
      particle.velocityY += fy * impulse;
      particle.velocityX = std::clamp(particle.velocityX, -6.0f, 6.0f);
      particle.velocityY = std::clamp(particle.velocityY, -6.0f, 6.0f);
      if (def.state == MatterState::Solid)
      {
        float damage = impulse * (mode == ForceMode::Explosion ? 0.55f : 0.16f);
        if (mode == ForceMode::Explosion)
          damage += intensity * falloff * 0.18f;
        if (particle.anchored)
          particle.velocityX = particle.velocityY = 0.0f;
        damageStructure(xx, yy, damage);
      }
      if (mode == ForceMode::Explosion)
      {
        changeTemperature(xx, yy, 0.28f * falloff * intensity);
        pressureField[index] = std::max(pressureField[index],
                                        std::min(1.0f, falloff * intensity * 0.28f));
        if (def.flammability && falloff * intensity > 2.2f && std::rand() % 6 == 0)
          setParticle(xx, yy, db.getId("Fire"));
      }
      wakeAt(xx, yy);
      ++touched;
    }
  }

  if (mode == ForceMode::Jet)
  {
    const auto &selected = db.getMaterial(selectedMaterial);
    const int liquidId = selected.state == MatterState::Liquid ? selectedMaterial : db.getId("Water");
    const int droplets = std::clamp(static_cast<int>(intensity * 3.0f), 1, 18);
    for (int i = 0; i < droplets; ++i)
    {
      const int px = x + static_cast<int>(std::round(directionX * (1 + i % 6))) +
                     (std::rand() % 3 - 1);
      const int py = y + static_cast<int>(std::round(directionY * (1 + i % 6))) +
                     (std::rand() % 3 - 1);
      if (!inBounds(px, py) || getParticle(px, py).matId)
        continue;
      setParticle(px, py, liquidId);
      auto &droplet = getParticleRef(px, py);
      droplet.velocityX = directionX * std::min(6.0f, intensity * 0.9f);
      droplet.velocityY = directionY * std::min(6.0f, intensity * 0.9f);
      ++touched;
    }
  }

  if (mode == ForceMode::Explosion)
  {
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx)
        if (inBounds(x + dx, y + dy) && grid[getIndex(x + dx, y + dy)].matId == 0 &&
            std::rand() % 4 == 0)
          setParticle(x + dx, y + dy, db.getId(std::rand() % 3 ? "Smoke" : "Fire"));
    logEvent("Explosao em " + std::to_string(x) + ", " + std::to_string(y));
  }
  if (mode == ForceMode::Fan)
    wind = std::clamp(wind + directionX * intensity * 0.002f, -2.8f, 2.8f);
  if (touched > 0 && mode != ForceMode::Anchor && mode != ForceMode::Release &&
      structuralActivityTicks > 0)
    structuralTickCounter = 3;
  return touched;
}

int World::emitMaterial(int x, int y, int materialId, int amount,
                        float intensity, float directionX, float directionY)
{
  if (!inBounds(x, y) || materialId <= 0 || amount <= 0)
    return 0;
  const auto &db = MaterialDB::getInstance();
  if (db.getMaterial(materialId).id != materialId)
    return 0;
  const float length = std::max(0.0001f, std::sqrt(directionX * directionX + directionY * directionY));
  directionX /= length;
  directionY /= length;
  int spawned = 0;
  const int capped = std::min(amount, 36);
  for (int offset = 0; offset < 90 && spawned < capped; ++offset)
  {
    const int layer = static_cast<int>(std::sqrt(static_cast<float>(offset)));
    const int px = x + (offset % 2 ? -layer : layer);
    const int py = y + (offset % 3 == 0 ? -layer : offset % 3 == 1 ? layer
                                                                   : 0);
    if (!inBounds(px, py) || grid[getIndex(px, py)].matId)
      continue;
    setParticle(px, py, materialId);
    auto &p = grid[getIndex(px, py)];
    const float spread = static_cast<float>((offset * 37) % 13 - 6) * 0.075f;
    p.velocityX = std::clamp(directionX * intensity * 0.55f - directionY * spread, -6.0f, 6.0f);
    p.velocityY = std::clamp(directionY * intensity * 0.55f + directionX * spread, -6.0f, 6.0f);
    ++spawned;
  }
  return spawned;
}

bool World::installEmitter(const Emitter &device)
{
  if (!inBounds(device.x, device.y) || emitters.size() >= 64 ||
      device.mode == ForceMode::Anchor || device.mode == ForceMode::Release ||
      device.materialId < 0 || device.materialId >= 256)
    return false;
  Emitter configured = device;
  const float magnitude = std::hypot(configured.directionX, configured.directionY);
  if (!std::isfinite(magnitude) || magnitude < 0.0001f ||
      !std::isfinite(configured.intensity) || !std::isfinite(configured.radius))
    return false;
  configured.directionX /= magnitude;
  configured.directionY /= magnitude;
  configured.intensity = std::clamp(configured.intensity, 0.1f, 12.0f);
  configured.radius = std::clamp(configured.radius, 1.0f, 40.0f);
  for (const auto &current : emitters)
    if (current.x == device.x && current.y == device.y)
      return false;
  emitters.push_back(configured);
  return true;
}

void World::clearEmitters() { emitters.clear(); }

void World::updateEmitters()
{
  ++emitterTickCounter;
  if (emitterTickCounter % 3 != 0)
    return;
  const auto &db = MaterialDB::getInstance();
  for (const auto &emitter : emitters)
  {
    if (emitter.mode == ForceMode::Explosion && emitterTickCounter % 30 != 0)
      continue;
    if (emitter.mode == ForceMode::Launch)
    {
      const int id = emitter.materialId > 0 ? emitter.materialId : db.getId("Stone");
      emitMaterial(emitter.x, emitter.y, id, 5, emitter.intensity,
                   emitter.directionX, emitter.directionY);
    }
    else
    {
      applyForce(emitter.x, emitter.y, emitter.radius, emitter.intensity,
                 emitter.mode, emitter.directionX, emitter.directionY,
                 emitter.materialId, true);
    }
  }
}

void World::updateFluidMotion(int x, int y, const MaterialDef &material)
{
  auto &p = grid[getIndex(x, y)];
  if (p.anchored)
    return;
  const int id = p.matId;
  const bool leftOpen = x > 0 && grid[getIndex(x - 1, y)].matId == 0;
  const bool rightOpen = x + 1 < width && grid[getIndex(x + 1, y)].matId == 0;
  const bool belowOpen = y + 1 < height && grid[getIndex(x, y + 1)].matId == 0;
  const bool exposed = leftOpen || rightOpen ||
                       (y > 0 && grid[getIndex(x, y - 1)].matId == 0);
  const float pressureLeft = x > 0 ? pressureField[getIndex(x - 1, y)] : pressureField[getIndex(x, y)];
  const float pressureRight = x + 1 < width ? pressureField[getIndex(x + 1, y)] : pressureField[getIndex(x, y)];
  const float lateralPressure = pressureLeft - pressureRight;
  const float drag = 0.988f - material.viscosity * 0.090f;
  const std::uint32_t tick = static_cast<std::uint32_t>(elapsedTime * 60.0f);
  std::uint32_t variation = static_cast<std::uint32_t>(x) * 73856093u ^
                            static_cast<std::uint32_t>(y) * 19349663u ^ tick * 83492791u ^ seed;
  variation ^= variation >> 16;
  variation *= 0x7feb352du;
  const float noise = static_cast<float>(variation & 1023u) / 511.5f - 1.0f;
  const float surface = exposed ? 1.0f : 0.19f;
  const float turbulence = noise * material.turbulence * surface *
                           (0.1f + std::min(1.0f, std::abs(p.velocityY) * 0.17f));
  const float cohesion = (leftOpen != rightOpen)
                             ? (leftOpen ? 1.0f : -1.0f) * material.surfaceTension * 0.065f
                             : 0.0f;
  p.velocityX = std::clamp(p.velocityX * drag +
                               lateralPressure * material.pressureResponse * (0.90f - material.viscosity * 0.4f) +
                               turbulence + cohesion,
                           -4.5f, 4.5f);
  const float buoyancy = (y > 0 && grid[getIndex(x, y - 1)].matId == id) ? -0.015f : 0.0f;
  p.velocityY = std::clamp(p.velocityY * (0.981f - material.viscosity * 0.035f) +
                               (belowOpen ? 0.32f : 0.085f) * (1.0f - material.viscosity * 0.65f) + buoyancy,
                           -4.3f, 5.0f);
  if (exposed || std::abs(p.velocityX) + std::abs(p.velocityY) > 0.20f)
    wakeAt(x, y);
}

bool World::integrateMotion(int x, int y, const MaterialDef &material)
{
  const int index = getIndex(x, y);
  Particle &particle = grid[index];
  if (particle.anchored ||
      (std::abs(particle.velocityX) < 0.08f &&
       std::abs(particle.velocityY) < 0.08f))
  {
    if (material.state != MatterState::Liquid)
      particle.velocityX = particle.velocityY = 0.0f;
    return false;
  }
  const float vx = particle.velocityX;
  const float vy = particle.velocityY;
  const float magnitude = std::max(std::abs(vx), std::abs(vy));
  particle.subcellX = std::clamp(particle.subcellX + vx, -6.0f, 6.0f);
  particle.subcellY = std::clamp(particle.subcellY + vy, -6.0f, 6.0f);
  const int intendedX = std::clamp(static_cast<int>(std::trunc(particle.subcellX)), -6, 6);
  const int intendedY = std::clamp(static_cast<int>(std::trunc(particle.subcellY)), -6, 6);
  if (!intendedX && !intendedY)
    return false;
  const int steps = std::clamp(std::max(std::abs(intendedX), std::abs(intendedY)), 1, 6);
  int cx = x, cy = y;
  bool moved = false;
  bool collided = false;
  for (int step = 1; step <= steps; ++step)
  {
    const int nx = x + static_cast<int>(std::round(intendedX * static_cast<float>(step) / steps));
    const int ny = y + static_cast<int>(std::round(intendedY * static_cast<float>(step) / steps));
    if (!inBounds(nx, ny))
      break;
    if (nx == cx && ny == cy)
      continue;
    if (!tryMove(cx, cy, nx, ny, material))
    {
      collided = true;
      const float impact = magnitude * std::clamp(material.density, 0.3f, 4.0f) * 0.10f;
      if (grid[getIndex(nx, ny)].matId)
        damageStructure(nx, ny, impact);
      damageStructure(cx, cy, impact * 0.15f);
      auto &current = grid[getIndex(cx, cy)];
      if (material.state == MatterState::Liquid && magnitude > 1.25f)
      {
        const float splash = std::min(2.5f, magnitude * material.turbulence);
        for (int side : {-1, 1})
        {
          const int sx = cx + side;
          if (!inBounds(sx, cy))
            continue;
          auto &neighbor = grid[getIndex(sx, cy)];
          if (neighbor.matId != current.matId)
            continue;
          neighbor.velocityX = std::clamp(neighbor.velocityX + side * splash, -5.0f, 5.0f);
          neighbor.velocityY = std::min(neighbor.velocityY, -0.4f - splash * 0.7f);
          wakeAt(sx, cy);
        }
        current.velocityX += (std::rand() % 2 ? 1.0f : -1.0f) * splash * 0.65f;
      }
      current.subcellX = current.subcellY = 0.0f;
      current.velocityX *= -0.20f;
      current.velocityY *= material.state == MatterState::Liquid ? -0.22f : -0.13f;
      if (std::abs(current.velocityX) < 0.4f)
        current.velocityX = 0.0f;
      if (std::abs(current.velocityY) < 0.4f)
        current.velocityY = 0.0f;
      break;
    }
    cx = nx;
    cy = ny;
    moved = true;
  }
  if (grid[getIndex(cx, cy)].matId == material.id)
  {
    auto &current = grid[getIndex(cx, cy)];
    if (!collided)
    {
      current.subcellX = std::clamp(current.subcellX - (cx - x), -1.0f, 1.0f);
      current.subcellY = std::clamp(current.subcellY - (cy - y), -1.0f, 1.0f);
    }
    const float drag = material.state == MatterState::Gas ? 0.93f : material.state == MatterState::Liquid ? 0.993f - material.viscosity * 0.050f
                                                                                                          : 0.84f;
    current.velocityX *= drag;
    current.velocityY = std::clamp(current.velocityY * drag +
                                       (material.state == MatterState::Gas ? -0.05f : material.state == MatterState::Liquid ? 0.0f
                                                                                                                            : 0.12f),
                                   -6.0f, 6.0f);
    current.updated_this_frame = moved;
  }
  return moved;
}

void World::updateStructures()
{
  structuralSupport.assign(grid.size(), 0);
  structuralVisited.assign(grid.size(), 0);
  structuralGroupIds.assign(grid.size(), 0);
  auto &visited = structuralVisited;
  auto &groupOf = structuralGroupIds;
  std::vector<int> component;
  component.reserve(512);
  const auto &db = MaterialDB::getInstance();
  int moving = 0;
  std::uint32_t groupId = 0;
  for (int origin = static_cast<int>(grid.size()) - 1; origin >= 0; --origin)
  {
    if (visited[origin] || !structural(grid[origin].matId))
      continue;
    component.clear();
    component.push_back(origin);
    visited[origin] = 1;
    ++groupId;
    groupOf[origin] = groupId;
    bool supported = false;
    bool anchored = false;
    float speedX = 0.0f, speedY = 0.0f;
    double massX = 0.0, massTotal = 0.0;
    for (std::size_t head = 0; head < component.size(); ++head)
    {
      const int index = component[head];
      const int x = index % width, y = index / width;
      const auto &particle = grid[index];
      const float mass = std::max(0.1f, db.getMaterial(particle.matId).density);
      massX += static_cast<double>(x) * mass;
      massTotal += mass;
      if (std::abs(particle.velocityX) > std::abs(speedX))
        speedX = particle.velocityX;
      if (std::abs(particle.velocityY) > std::abs(speedY))
        speedY = particle.velocityY;
      anchored = anchored || particle.anchored;
      supported = supported || y == height - 1;
      for (int direction = 0; direction < 4; ++direction)
      {
        const int nx = x + (direction == 0) - (direction == 1);
        const int ny = y + (direction == 2) - (direction == 3);
        if (!inBounds(nx, ny))
          continue;
        const int neighbor = getIndex(nx, ny);
        const bool worldBoundary = environment == Environment::Laboratory &&
                                   (x < 2 || nx < 2 || y >= height - 2 || ny >= height - 2 ||
                                    x >= width - 2 || nx >= width - 2);
        if (!visited[neighbor] && grid[neighbor].matId == particle.matId &&
            !(worldBoundary && particle.anchored != grid[neighbor].anchored) &&
            bonded(particle, grid[neighbor], index, neighbor,
                   db.getMaterial(particle.matId)))
        {
          visited[neighbor] = 1;
          groupOf[neighbor] = groupId;
          component.push_back(neighbor);
        }
      }
    }
    int minSupportX = width, maxSupportX = -1;
    double supportY = 0.0;
    int supportCount = 0;
    for (int index : component)
    {
      if (index / width + 1 < height && groupOf[index + width] == groupId)
        continue;
      const bool atBottom = index / width + 1 >= height;
      const auto &below = grid[atBottom ? index : index + width];
      const auto &def = db.getMaterial(below.matId);
      if (atBottom || (below.matId &&
                       (def.state == MatterState::Solid || def.state == MatterState::Granular)))
      {
        supported = true;
        const int x = index % width;
        minSupportX = std::min(minSupportX, x);
        maxSupportX = std::max(maxSupportX, x);
        supportY += index / width;
        ++supportCount;
      }
    }
    const float centerX = massTotal > 0.0 ? static_cast<float>(massX / massTotal) : 0.0f;
    const bool unbalanced = supported && !anchored && supportCount > 0 &&
                            (centerX < minSupportX - 1.5f || centerX > maxSupportX + 1.5f);
    if (unbalanced && std::abs(speedX) < 0.40f &&
        std::abs(speedY) < 0.40f)
    {
      const int direction = centerX > maxSupportX ? 1 : -1;
      const float pivotX = static_cast<float>(direction > 0 ? maxSupportX : minSupportX);
      const float pivotY = static_cast<float>(supportY / supportCount);
      const float leverage = std::abs(centerX - pivotX);
      const float angle = direction * std::clamp(
                                          leverage / std::sqrt(static_cast<float>(component.size())) * 0.012f,
                                          0.018f, 0.055f);
      bool rotated = false;
      if (component.size() <= 2400)
      {
        const float cosine = std::cos(angle), sine = std::sin(angle);
        std::unordered_set<int> destinationsSet;
        destinationsSet.reserve(component.size() * 2);
        std::vector<int> destinations;
        destinations.reserve(component.size());
        rotated = true;
        for (int index : component)
        {
          const float rx = index % width - pivotX;
          const float ry = index / width - pivotY;
          const float tx = pivotX + rx * cosine - ry * sine;
          const float ty = pivotY + rx * sine + ry * cosine;
          const int cx = static_cast<int>(std::round(tx));
          const int cy = static_cast<int>(std::round(ty));
          int chosen = -1;
          float distance = 1000.0f;
          for (int radius = 0; radius <= 2 && chosen < 0; ++radius)
          {
            for (int dy = -radius; dy <= radius; ++dy)
              for (int dx = -radius; dx <= radius; ++dx)
              {
                if (std::max(std::abs(dx), std::abs(dy)) != radius ||
                    !inBounds(cx + dx, cy + dy))
                  continue;
                const int target = getIndex(cx + dx, cy + dy);
                if (destinationsSet.count(target))
                  continue;
                const int otherId = grid[target].matId;
                const auto state = db.getMaterial(otherId).state;
                if (groupOf[target] != groupId && otherId &&
                    state != MatterState::Liquid && state != MatterState::Gas &&
                    state != MatterState::Plasma)
                  continue;
                const float candidateDistance =
                    (cx + dx - tx) * (cx + dx - tx) +
                    (cy + dy - ty) * (cy + dy - ty);
                if (candidateDistance < distance)
                {
                  distance = candidateDistance;
                  chosen = target;
                }
              }
          }
          if (chosen < 0)
          {
            rotated = false;
            break;
          }
          destinations.push_back(chosen);
          destinationsSet.insert(chosen);
        }
        if (rotated)
        {
          struct CellState
          {
            Particle particle;
            float moisture, moistureNext, temperatureA, temperatureB;
          };
          auto capture = [&](int index)
          {
            return CellState{grid[index], soilWater[index], soilWaterNext[index],
                             tempA[index], tempB[index]};
          };
          auto restore = [&](int index, const CellState &state)
          {
            grid[index] = state.particle;
            soilWater[index] = state.moisture;
            soilWaterNext[index] = state.moistureNext;
            tempA[index] = state.temperatureA;
            tempB[index] = state.temperatureB;
          };
          std::vector<CellState> movingCells, displacedCells;
          std::vector<int> vacated;
          movingCells.reserve(component.size());
          for (int index : component)
          {
            movingCells.push_back(capture(index));
            if (!destinationsSet.count(index))
              vacated.push_back(index);
          }
          for (int target : destinations)
            if (groupOf[target] != groupId)
              displacedCells.push_back(capture(target));
          if (vacated.size() == displacedCells.size())
          {
            for (int index : component)
            {
              grid[index] = Particle{};
              soilWater[index] = soilWaterNext[index] = 0.0f;
              tempA[index] = tempB[index] = ambientTemperature;
              visited[index] = 0;
              groupOf[index] = 0;
              wakeAt(index % width, index / width);
            }
            for (std::size_t i = 0; i < vacated.size(); ++i)
              restore(vacated[i], displacedCells[i]);
            for (std::size_t i = 0; i < destinations.size(); ++i)
            {
              restore(destinations[i], movingCells[i]);
              grid[destinations[i]].velocityX = 0.0f;
              grid[destinations[i]].velocityY = 0.0f;
              grid[destinations[i]].updated_this_frame = true;
              visited[destinations[i]] = 2;
              groupOf[destinations[i]] = groupId;
              wakeAt(destinations[i] % width, destinations[i] / width);
            }
            ++moving;
            structuralActivityTicks = std::max(structuralActivityTicks, 24);
            continue;
          }
          rotated = false;
        }
      }
      if (!rotated)
      {
        const float strain = std::clamp(leverage / std::sqrt(static_cast<float>(component.size())) * 0.012f, 0.02f, 0.11f);
        for (int index : component)
          if (std::abs(index % width - pivotX) < 3 &&
              std::abs(index / width - pivotY) < 5)
            damageStructure(index % width, index / width, strain);
      }
    }
    if (anchored || (supported && std::abs(speedX) < 0.40f &&
                     std::abs(speedY) < 0.40f))
    {
      for (int index : component)
      {
        structuralSupport[index] = 24;
        if (!grid[index].anchored)
        {
          grid[index].velocityX = 0.0f;
          grid[index].velocityY = 0.0f;
          grid[index].subcellY = 0.0f;
        }
      }
      continue;
    }
    const float terminalVelocity = 8.0f * std::sqrt(gravityScale);
    const float nextVelocityY = supported ? speedY : std::clamp(speedY + 1.55f * gravityScale, -terminalVelocity, terminalVelocity);
    int sortDirectionX = 100, sortDirectionY = 100;
    auto moveBody = [&](int dx, int dy)
    {
      bool clearPath = true;
      for (int index : component)
      {
        const int x = index % width, y = index / width;
        if (!inBounds(x + dx, y + dy))
        {
          clearPath = false;
          break;
        }
        const int target = getIndex(x + dx, y + dy);
        if (groupOf[target] == groupId)
          continue;
        const auto &other = db.getMaterial(grid[target].matId);
        if (grid[target].matId &&
            other.state != MatterState::Liquid &&
            other.state != MatterState::Gas &&
            other.state != MatterState::Plasma)
        {
          clearPath = false;
          break;
        }
      }
      if (!clearPath)
      {
        const float impact = std::hypot(speedX, nextVelocityY) * 0.09f;
        if (impact > 0.04f)
          for (int index : component)
          {
            const int x = index % width, y = index / width;
            if (inBounds(x + dx, y + dy) &&
                grid[getIndex(x + dx, y + dy)].matId &&
                groupOf[getIndex(x + dx, y + dy)] != groupId)
              damageStructure(x, y, impact);
          }
        return false;
      }
      if (sortDirectionX != dx || sortDirectionY != dy)
      {
        std::sort(component.begin(), component.end(), [=](int a, int b)
                  {
          if (dy != 0) return dy > 0 ? a > b : a < b;
          return dx > 0 ? a % width > b % width : a % width < b % width; });
        sortDirectionX = dx;
        sortDirectionY = dy;
      }
      for (int &index : component)
      {
        const int target = index + dy * width + dx;
        swapParticles(index % width, index / width, target % width, target / width);
        visited[index] = 0;
        visited[target] = 2;
        groupOf[index] = 0;
        groupOf[target] = groupId;
        index = target;
      }
      ++moving;
      return true;
    };
    const int moveX = std::clamp(static_cast<int>(std::round(speedX)), -6, 6);
    int moveY = std::clamp(static_cast<int>(std::round(nextVelocityY)), -8, 8);
    if (!supported && moveY == 0)
      moveY = 1;
    bool horizontalBlocked = false, verticalBlocked = false;
    for (int step = 0; step < std::abs(moveX); ++step)
      if (!moveBody(moveX > 0 ? 1 : -1, 0))
      {
        horizontalBlocked = true;
        break;
      }
    for (int step = 0; step < std::abs(moveY); ++step)
      if (!moveBody(0, moveY > 0 ? 1 : -1))
      {
        verticalBlocked = true;
        break;
      }
    for (int index : component)
    {
      visited[index] = 2;
      if (!structural(grid[index].matId))
        continue;
      auto &cell = grid[index];
      cell.velocityX = horizontalBlocked ? 0.0f : cell.velocityX * (supported ? 0.74f : 0.94f);
      cell.velocityY = verticalBlocked ? 0.0f : nextVelocityY;
      cell.subcellX = cell.subcellY = 0.0f;
    }
  }
  if (moving > 0)
  {
    structuralActivityTicks = std::max(structuralActivityTicks, 12);
    if (moving > 20 && std::rand() % 16 == 0)
      logEvent("Estruturas em queda");
  }
}
