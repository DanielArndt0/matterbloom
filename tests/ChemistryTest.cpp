#include "../src/World.h"
#include "TempPaths.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

int main()
{
  std::srand(4401);
  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");

  assert(db.getId("MoltenMetal") != 0);
  assert(db.getId("Obsidian") != 0);
  assert(db.getTransitions(db.getId("Water")).size() == 2);
  assert(db.getMaterial("Water").heatCapacity > db.getMaterial("Fire").heatCapacity);

  World world(48, 48);
  world.setParticle(25, 30, db.getId("Metal"));
  world.changeTemperature(25, 30, 0.99f);
  bool metalMelted = false;
  for (int tick = 0; tick < 150; ++tick)
  {
    world.update(1.0f / 60.0f);
    for (int y = 28; y < 48; ++y)
      for (int x = 18; x < 34; ++x)
        metalMelted |= world.getParticle(x, y).matId == db.getId("MoltenMetal");
  }
  assert(metalMelted);

  world.clear();
  world.setParticle(12, 35, db.getId("Wood"));
  world.setParticle(12, 35, db.getId("Fire"));
  const int residue = db.getId("Charcoal");
  assert(world.getParticle(12, 35).burnResidue == residue);
  assert(world.save(testTempFile("matterbloom-chemistry.agw")));
  const auto snapshot = world.captureSnapshot();
  world.clear();
  assert(world.load(testTempFile("matterbloom-chemistry.agw")));
  assert(world.getParticle(12, 35).burnResidue == residue);
  world.restoreSnapshot(snapshot);
  assert(world.getParticle(12, 35).burnResidue == residue);
  bool charcoalAppeared = false;
  for (int tick = 0; tick < 80; ++tick)
  {
    world.update(1.0f / 60.0f);
    charcoalAppeared |= world.getParticle(12, 35).matId == residue;
  }
  assert(charcoalAppeared);

  world.clear();
  world.setParticle(12, 30, db.getId("Acid"));
  world.setParticle(13, 30, db.getId("Metal"));
  const auto reactionsBefore = world.getStats().reactions;
  world.update(1.0f / 60.0f);
  assert(world.getParticle(13, 30).matId == db.getId("Rust"));
  assert(world.getStats().reactions > reactionsBefore);

  world.clear();
  world.setParticle(12, 30, db.getId("Fire"));
  world.setParticle(13, 30, db.getId("Hydrogen"));
  world.update(1.0f / 60.0f);
  assert(world.getStats().reactions > 0);

  world.clear();
  world.setParticle(24, 28, db.getId("Wall"));
  world.setParticle(25, 28, db.getId("Wall"));
  world.setParticle(24, 29, db.getId("Metal"));
  world.changeTemperature(24, 29, 0.80f);
  world.setParticle(25, 29, db.getId("Metal"));
  const float coldNeighbor = world.getTemperature(25, 29);
  for (int tick = 0; tick < 12; ++tick)
    world.update(1.0f / 60.0f);
  assert(world.getTemperature(25, 29) > coldNeighbor);

  std::cout << "Advanced chemistry tests passed\n";
}
