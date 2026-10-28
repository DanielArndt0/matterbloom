#include "World.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

World::World(int width, int height) : width(width), height(height)
{
  grid.resize(width * height);
  pixelData.resize(width * height * 4, 255);

  int size = width * height;
  tempA.resize(size, 0.0f);
  tempB.resize(size, 0.0f);
  humidA.resize(size, 0.0f);
  humidB.resize(size, 0.0f);
  nutA.resize(size, 0.0f);
  nutB.resize(size, 0.0f);

  texture.create(width, height);
  sprite.setTexture(texture);

  chunksX = (width + CHUNK_SIZE - 1) / CHUNK_SIZE;
  chunksY = (height + CHUNK_SIZE - 1) / CHUNK_SIZE;
  chunks.resize(chunksX * chunksY);

  for (auto &c : chunks)
  {
    c.setDirty(true);
    c.updateBounds(0, 0);
    c.updateBounds(CHUNK_SIZE - 1, CHUNK_SIZE - 1);
  }
}

int World::getIndex(int x, int y) const { return y * width + x; }

void World::setScale(float scaleX, float scaleY)
{
  sprite.setScale(scaleX, scaleY);
}

sf::Vector2f World::getSpriteScale() const { return sprite.getScale(); }

bool World::inBounds(int x, int y) const
{
  return x >= 0 && x < width && y >= 0 && y < height;
}

void World::setParticle(int x, int y, int matId)
{
  if (!inBounds(x, y))
    return;

  const MaterialDef &def = MaterialDB::getInstance().getMaterial(matId);

  Particle p;
  p.matId = def.id;
  p.color = def.baseColor;

  if (def.colorVariance > 0)
  {
    int rVar = (std::rand() % (def.colorVariance * 2)) - def.colorVariance;
    int gVar = (std::rand() % (def.colorVariance * 2)) - def.colorVariance;
    int bVar = (std::rand() % (def.colorVariance * 2)) - def.colorVariance;
    p.color.r = std::clamp((int)p.color.r + rVar, 0, 255);
    p.color.g = std::clamp((int)p.color.g + gVar, 0, 255);
    p.color.b = std::clamp((int)p.color.b + bVar, 0, 255);
  }

  if (def.lifetime > 0)
  {

    p.lifetime_counter = def.lifetime + (std::rand() % (def.lifetime / 2));
  }
  else
  {
    p.lifetime_counter = -1;
  }

  int i = getIndex(x, y);
  grid[i] = p;

  int cx = x / CHUNK_SIZE;
  int cy = y / CHUNK_SIZE;
  chunks[cy * chunksX + cx].setDirty(true);
  chunks[cy * chunksX + cx].updateBounds(x % CHUNK_SIZE, y % CHUNK_SIZE);
}

Particle World::getParticle(int x, int y) const
{
  if (!inBounds(x, y))
    return {0, sf::Color::Black, false, -1};
  return grid[getIndex(x, y)];
}

void World::swapParticles(int x1, int y1, int x2, int y2)
{
  if (!inBounds(x1, y1) || !inBounds(x2, y2))
    return;
  int i1 = getIndex(x1, y1);
  int i2 = getIndex(x2, y2);
  std::swap(grid[i1], grid[i2]);

  int cx1 = x1 / CHUNK_SIZE;
  int cy1 = y1 / CHUNK_SIZE;
  int cx2 = x2 / CHUNK_SIZE;
  int cy2 = y2 / CHUNK_SIZE;

  chunks[cy1 * chunksX + cx1].setDirty(true);
  chunks[cy1 * chunksX + cx1].updateBounds(x1 % CHUNK_SIZE, y1 % CHUNK_SIZE);

  chunks[cy2 * chunksX + cx2].setDirty(true);
  chunks[cy2 * chunksX + cx2].updateBounds(x2 % CHUNK_SIZE, y2 % CHUNK_SIZE);
}

void World::update(float dt)
{

  std::vector<int> activeChunks;
  for (int i = 0; i < chunks.size(); ++i)
  {
    if (chunks[i].isDirty())
    {
      activeChunks.push_back(i);
      chunks[i].setDirty(
          false);
    }
  }

  for (int ci : activeChunks)
  {
    int cx = ci % chunksX;
    int cy = ci / chunksX;
    int startX = cx * CHUNK_SIZE;
    int startY = cy * CHUNK_SIZE;
    int endX = std::min(startX + CHUNK_SIZE, width);
    int endY = std::min(startY + CHUNK_SIZE, height);

    for (int y = startY; y < endY; ++y)
    {
      for (int x = startX; x < endX; ++x)
      {
        grid[getIndex(x, y)].updated_this_frame = false;
      }
    }

    chunks[ci].resetBounds();
  }

  for (int cy = chunksY - 1; cy >= 0; --cy)
  {

    bool leftToRightChunk = (std::rand() % 2 == 0);
    int startCX = leftToRightChunk ? 0 : chunksX - 1;
    int endCX = leftToRightChunk ? chunksX : -1;
    int stepCX = leftToRightChunk ? 1 : -1;

    for (int cx = startCX; cx != endCX; cx += stepCX)
    {
      int ci = cy * chunksX + cx;

      if (std::find(activeChunks.begin(), activeChunks.end(), ci) ==
          activeChunks.end())
        continue;

      int startY = cy * CHUNK_SIZE;
      int endY = std::min(startY + CHUNK_SIZE, height);
      int startX = cx * CHUNK_SIZE;
      int endX = std::min(startX + CHUNK_SIZE, width);

      for (int y = endY - 1; y >= startY; --y)
      {
        bool leftToRight = (std::rand() % 2 == 0);
        int sx = leftToRight ? startX : endX - 1;
        int ex = leftToRight ? endX : startX - 1;
        int step = leftToRight ? 1 : -1;

        for (int x = sx; x != ex; x += step)
        {
          int i = getIndex(x, y);
          Particle &p = grid[i];

          if (p.matId == 0 || p.updated_this_frame)
            continue;

          const MaterialDef &def =
              MaterialDB::getInstance().getMaterial(p.matId);

          if (def.lifetime > 0)
          {
            p.lifetime_counter--;
            if (p.lifetime_counter <= 0)
            {
              if (def.state == MatterState::Plasma)
              {

                if (std::rand() % 100 < 30)
                {
                  setParticle(x, y, MaterialDB::getInstance().getId("Smoke"));
                }
                else
                {
                  setParticle(x, y, 0);
                }
              }
              else
              {
                setParticle(x, y, 0);
              }
              continue;
            }
          }

          if (def.state == MatterState::Granular ||
              def.state == MatterState::Liquid)
          {
            int dir = (std::rand() % 2 == 0) ? 1 : -1;

            Particle pBelow = getParticle(x, y + 1);
            const MaterialDef &defBelow =
                MaterialDB::getInstance().getMaterial(pBelow.matId);

            if (inBounds(x, y + 1) && defBelow.density < def.density)
            {
              swapParticles(x, y, x, y + 1);
              grid[getIndex(x, y + 1)].updated_this_frame = true;
            }
            else
            {

              Particle pDi1 = getParticle(x + dir, y + 1);
              Particle pDi2 = getParticle(x - dir, y + 1);
              const MaterialDef &defDi1 =
                  MaterialDB::getInstance().getMaterial(pDi1.matId);
              const MaterialDef &defDi2 =
                  MaterialDB::getInstance().getMaterial(pDi2.matId);

              if (inBounds(x + dir, y + 1) && defDi1.density < def.density)
              {
                swapParticles(x, y, x + dir, y + 1);
                grid[getIndex(x + dir, y + 1)].updated_this_frame = true;
              }
              else if (inBounds(x - dir, y + 1) &&
                       defDi2.density < def.density)
              {
                swapParticles(x, y, x - dir, y + 1);
                grid[getIndex(x - dir, y + 1)].updated_this_frame = true;
              }
              else if (def.state == MatterState::Liquid)
              {

                int spread = def.dispersionRate;
                bool moved = false;
                for (int s = 1; s <= spread; ++s)
                {
                  if (inBounds(x + dir * s, y) &&
                      MaterialDB::getInstance()
                              .getMaterial(getParticle(x + dir * s, y).matId)
                              .density < def.density)
                  {
                    swapParticles(x, y, x + dir * s, y);
                    grid[getIndex(x + dir * s, y)].updated_this_frame = true;
                    moved = true;
                    break;
                  }
                }
                if (!moved)
                {
                  for (int s = 1; s <= spread; ++s)
                  {
                    if (inBounds(x - dir * s, y) &&
                        MaterialDB::getInstance()
                                .getMaterial(getParticle(x - dir * s, y).matId)
                                .density < def.density)
                    {
                      swapParticles(x, y, x - dir * s, y);
                      grid[getIndex(x - dir * s, y)].updated_this_frame = true;
                      break;
                    }
                  }
                }
              }
            }
          }

          else if (def.state == MatterState::Gas ||
                   def.state == MatterState::Plasma)
          {
            int dir = (std::rand() % 2 == 0) ? 1 : -1;

            Particle pAbove = getParticle(x, y - 1);
            const MaterialDef &defAbove =
                MaterialDB::getInstance().getMaterial(pAbove.matId);

            if (inBounds(x, y - 1) && defAbove.density > def.density)
            {
              swapParticles(x, y, x, y - 1);
              grid[getIndex(x, y - 1)].updated_this_frame = true;
            }
            else
            {
              Particle pDi1 = getParticle(x + dir, y - 1);
              Particle pDi2 = getParticle(x - dir, y - 1);
              const MaterialDef &defDi1 =
                  MaterialDB::getInstance().getMaterial(pDi1.matId);
              const MaterialDef &defDi2 =
                  MaterialDB::getInstance().getMaterial(pDi2.matId);

              if (inBounds(x + dir, y - 1) && defDi1.density > def.density)
              {
                swapParticles(x, y, x + dir, y - 1);
                grid[getIndex(x + dir, y - 1)].updated_this_frame = true;
              }
              else if (inBounds(x - dir, y - 1) &&
                       defDi2.density > def.density)
              {
                swapParticles(x, y, x - dir, y - 1);
                grid[getIndex(x - dir, y - 1)].updated_this_frame = true;
              }
              else
              {

                if (inBounds(x + dir, y) &&
                    MaterialDB::getInstance()
                            .getMaterial(getParticle(x + dir, y).matId)
                            .density > def.density)
                {
                  swapParticles(x, y, x + dir, y);
                  grid[getIndex(x + dir, y)].updated_this_frame = true;
                }
              }
            }
          }

          int dx[] = {-1, 1, 0, 0};
          int dy[] = {0, 0, -1, 1};
          for (int j = 0; j < 4; ++j)
          {
            int nx = x + dx[j];
            int ny = y + dy[j];
            if (!inBounds(nx, ny))
              continue;

            int nId = getParticle(nx, ny).matId;
            if (nId == 0 && def.id == 0)
              continue;

            const ReactionDef *rxn =
                MaterialDB::getInstance().getReaction(def.id, nId);
            if (rxn)
            {
              float randChance = (std::rand() % 1000) / 1000.0f;
              if (randChance <= rxn->probability)
              {

                setParticle(x, y, rxn->product1);
                setParticle(nx, ny, rxn->product2);
                break;
              }
            }
          }
        }
      }
    }
  }

  fieldTickCounter++;
  if (fieldTickCounter >= FIELD_TICK_RATE)
  {
    fieldTickCounter = 0;
    updateFields();
  }

  for (auto &entity : entities)
  {
    entity.update(*this, dt);
  }
}

float World::getNeighborAverage(const std::vector<float> &buffer, int cx,
                                int cy)
{
  float sum = 0.0f;
  int count = 0;

  int dx[] = {-1, 1, 0, 0};
  int dy[] = {0, 0, -1, 1};
  for (int i = 0; i < 4; ++i)
  {
    int nx = cx + dx[i];
    int ny = cy + dy[i];
    if (inBounds(nx, ny))
    {
      sum += buffer[getIndex(nx, ny)];
      count++;
    }
  }
  return sum / count;
}

void World::updateFields()
{
  const std::vector<float> &tSource = usingBufferA ? tempA : tempB;
  std::vector<float> &tDest = usingBufferA ? tempB : tempA;

  const std::vector<float> &hSource = usingBufferA ? humidA : humidB;
  std::vector<float> &hDest = usingBufferA ? humidB : humidA;

  float DIFFUSION_RATE = 0.1f;
  float DISSIPATION = 0.98f;

  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      int i = getIndex(x, y);
      const Particle &p = grid[i];

      float tAvg = getNeighborAverage(tSource, x, y);
      float hAvg = getNeighborAverage(hSource, x, y);

      float newTemp = tSource[i] + (tAvg - tSource[i]) * DIFFUSION_RATE;
      float newHumid = hSource[i] + (hAvg - hSource[i]) * DIFFUSION_RATE;

      newTemp *= DISSIPATION;
      newHumid *= DISSIPATION;

      if (p.matId != 0)
      {
        const MaterialDef &def = MaterialDB::getInstance().getMaterial(p.matId);

        if (def.name == "Lava" || def.name == "Fire")
        {
          newTemp = 1.0f;
        }
        else if (def.name == "Ice")
        {
          newTemp = -1.0f;
        }

        if (def.name == "Water" || def.name == "Saltwater" ||
            def.name == "Steam")
        {
          newHumid = 1.0f;
        }
      }

      tDest[i] = newTemp;
      hDest[i] = newHumid;
    }
  }

  usingBufferA = !usingBufferA;
}

void World::render(sf::RenderWindow &window)
{
  const std::vector<float> &tSource = usingBufferA ? tempA : tempB;
  const std::vector<float> &hSource = usingBufferA ? humidA : humidB;

  for (size_t i = 0; i < grid.size(); ++i)
  {
    if (renderMode == RenderMode::Normal)
    {
      pixelData[i * 4 + 0] = grid[i].color.r;
      pixelData[i * 4 + 1] = grid[i].color.g;
      pixelData[i * 4 + 2] = grid[i].color.b;
    }
    else if (renderMode == RenderMode::Temperature)
    {
      float t = tSource[i];
      if (t > 0)
      {

        pixelData[i * 4 + 0] = std::min(255, (int)(t * 255.0f));
        pixelData[i * 4 + 1] = 0;
        pixelData[i * 4 + 2] = 0;
      }
      else
      {

        pixelData[i * 4 + 0] = 0;
        pixelData[i * 4 + 1] = 0;
        pixelData[i * 4 + 2] = std::min(255, (int)(-t * 255.0f));
      }

      if (t == 0)
      {
        pixelData[i * 4 + 0] = grid[i].color.r / 3;
        pixelData[i * 4 + 1] = grid[i].color.g / 3;
        pixelData[i * 4 + 2] = grid[i].color.b / 3;
      }
    }
    else if (renderMode == RenderMode::Humidity)
    {
      float h = hSource[i];
      pixelData[i * 4 + 0] = 0;
      pixelData[i * 4 + 1] = std::min(255, (int)(h * 200.0f));
      pixelData[i * 4 + 2] = std::min(255, (int)(h * 255.0f));

      if (h == 0)
      {
        pixelData[i * 4 + 0] = grid[i].color.r / 3;
        pixelData[i * 4 + 1] = grid[i].color.g / 3;
        pixelData[i * 4 + 2] = grid[i].color.b / 3;
      }
    }
    else
    {

      pixelData[i * 4 + 0] = grid[i].color.r;
      pixelData[i * 4 + 1] = grid[i].color.g;
      pixelData[i * 4 + 2] = grid[i].color.b;
    }

    pixelData[i * 4 + 3] = 255;
  }

  texture.update(pixelData.data());
  window.draw(sprite);
}
