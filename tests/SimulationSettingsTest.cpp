#include "MaterialDB.h"
#include "SimulationTuning.h"
#include "World.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
  int findMaterialY(const World &world, int x, int material)
  {
    for (int y = world.getHeight() - 1; y >= 0; --y)
      if (world.getParticle(x, y).matId == material)
        return y;
    return -1;
  }
}

int main()
{
  SimulationTuning tuning;
  assert(std::abs(tuning.speed - 0.70f) < 0.0001f);
  assert(tuning.physicsHz == 60);
  assert(tuning.maxUpdatesPerFrame == 3);
  tuning.speed = 1000.0f;
  tuning.gravity = -10.0f;
  tuning.physicsHz = 999;
  tuning.maxUpdatesPerFrame = 0;
  tuning.frameBudgetMs = -1.0f;
  tuning.lightStrength = 99.0f;
  tuning.sanitize();
  assert(tuning.speed == 3.0f && tuning.gravity == 0.10f);
  assert(tuning.physicsHz == 120 && tuning.maxUpdatesPerFrame == 1);
  assert(tuning.frameBudgetMs == 2.0f && tuning.lightStrength == 1.65f);

  SimulationPacer pacer;
  tuning = SimulationTuning{};
  pacer.push(1.0f / 60.0f, tuning);
  assert(!pacer.ready(tuning));
  pacer.push(1.0f / 60.0f, tuning);
  assert(pacer.ready(tuning));
  pacer.consume(tuning);
  assert(!pacer.ready(tuning));
  pacer.reset();
  assert(pacer.debt == 0.0);

  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");
  const int rock = db.getId("Granite");
  const int gravel = db.getId("Gravel");
  assert(rock > 0 && gravel > 0);
  World slow(120, 120), fast(120, 120);
  slow.setGravityScale(0.25f);
  fast.setGravityScale(1.6f);
  slow.setParticle(40, 12, rock);
  fast.setParticle(40, 12, rock);
  for (int tick = 0; tick < 8; ++tick)
  {
    slow.update(1.0f / 60.0f);
    fast.update(1.0f / 60.0f);
  }
  assert(findMaterialY(fast, 40, rock) > findMaterialY(slow, 40, rock));

  slow.clear();
  fast.clear();
  slow.setParticle(50, 12, gravel);
  fast.setParticle(50, 12, gravel);
  for (int tick = 0; tick < 5; ++tick)
  {
    slow.update(1.0f / 60.0f);
    fast.update(1.0f / 60.0f);
  }
  assert(findMaterialY(fast, 50, gravel) > findMaterialY(slow, 50, gravel));
  slow.setRenderTuning(false, 0.65f);
  fast.setRenderTuning(true, 1.2f);
  std::cout << "Matterbloom configurable pacing, gravity and graphics passed\n";
}
