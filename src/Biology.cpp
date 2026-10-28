#include "Biology.h"
#include "MaterialDB.h"
#include "World.h"
#include <SFML/Graphics/Color.hpp>
#include <cstdlib>
#include <algorithm>

void Biology::processPlant(World *world, int x, int y, Particle &p)
{

  const MaterialDef &def = MaterialDB::getInstance().getMaterial(p.matId);

  if (def.name != "Seed" && def.name != "Wood" &&
      def.name != "Plant" && def.name != "Moss" &&
      def.name != "Algae" && def.name != "Grass" &&
      def.name != "Flower" && def.name != "PineSeed" &&
      def.name != "MangroveSeed" && def.name != "PineNeedles" &&
      def.name != "Mushroom" && def.name != "Mycelium" &&
      def.name != "ShrubSeed" && def.name != "Shrub" &&
      def.name != "Roots")
    return;

  if (rand() % 100 > 5)
    return;

  float temp = world->getTemperature(x, y);
  float humidity = world->getHumidity(x, y);
  float nutrients = world->getNutrients(x, y);
  if (world->inBounds(x, y + 1))
  {
    humidity = std::max(humidity, world->getHumidity(x, y + 1) * 0.6f);
    nutrients = std::max(nutrients, world->getNutrients(x, y + 1) * 0.6f);
  }
  if (def.name == "Wood" || def.name == "Roots")
  {
    const int root = MaterialDB::getInstance().getId("Roots");
    const int soil = MaterialDB::getInstance().getId("Soil");
    const int peat = MaterialDB::getInstance().getId("Peat");
    const int mud = MaterialDB::getInstance().getId("Mud");
    const int belowY = y + 1;
    if (world->inBounds(x, belowY) && p.age < 12 && rand() % 9 == 0)
    {
      const int dx = def.name == "Roots" ? rand() % 3 - 1 : 0;
      const int nx = x + dx;
      if (world->inBounds(nx, belowY))
      {
        const int target = world->getParticle(nx, belowY).matId;
        if (target == soil || target == peat || target == mud)
        {
          const float moisture = world->getSoilSaturation(nx, belowY);
          if (moisture > 0.025f && nutrients > 0.03f)
          {
            world->setParticle(nx, belowY, root);
            world->getParticleRef(nx, belowY).age = p.age + 1;
            world->consumeSoilWater(nx, belowY, 0.006f);
            world->changeNutrients(nx, belowY, -0.012f);
            world->releaseVapor(nx, 0.0013f);
            return;
          }
        }
      }
    }
    if (def.name == "Roots")
    {
      if (humidity < 0.004f && rand() % 120 == 0)
        world->setParticle(x, y, MaterialDB::getInstance().getId("Compost"));
      return;
    }
  }

  if (def.name == "Seed" || def.name == "PineSeed" ||
      def.name == "MangroveSeed" || def.name == "ShrubSeed")
  {

    if (world->inBounds(x, y + 1))
    {
      const Particle &below = world->getParticle(x, y + 1);
      const MaterialDef &belowDef =
          MaterialDB::getInstance().getMaterial(below.matId);

      if (belowDef.name == "Soil" || belowDef.name == "Mud" ||
          belowDef.name == "Compost" || belowDef.name == "Peat" ||
          (belowDef.name == "Sand" && def.name == "ShrubSeed"))
      {
        const float saturation = world->getSoilSaturation(x, y + 1);
        const bool temperatureSuitable =
            def.name == "PineSeed" ? temp < 0.55f && temp > -0.65f : def.name == "MangroveSeed" ? temp > -0.15f && temp < 0.57f
                                                                 : def.name == "ShrubSeed"      ? temp > -0.05f && temp < 0.76f
                                                                                                : temp > -0.42f && temp < 0.62f;
        const float need = def.name == "MangroveSeed" ? 0.48f : def.name == "ShrubSeed" ? 0.022f
                                                                                        : 0.12f;
        if (temperatureSuitable && saturation > need &&
            humidity > 0.028f && nutrients > 0.025f)
        {

          world->setParticle(x, y, MaterialDB::getInstance().getId(def.name == "ShrubSeed" ? "Shrub" : "Wood"));
          if (def.name != "ShrubSeed")
            world->getParticleRef(x, y).species =
                def.name == "PineSeed" ? 1 : def.name == "MangroveSeed" ? 2
                                                                        : 0;

          world->changeHumidity(x, y, -0.03f);
          world->changeNutrients(x, y, -0.02f);
        }
      }
    }
  }
  else if (def.name == "Wood")
  {

    const int maximumHeight = p.species == 1 ? 22 : p.species == 2 ? 12
                                                                   : 15;
    if (humidity > 0.055f && nutrients > 0.04f && p.age < maximumHeight)
    {

      if (world->inBounds(x, y - 1))
      {
        Particle &above = world->getParticleRef(x, y - 1);
        if (above.matId == MaterialDB::getInstance().getId("Empty") ||
            above.matId == MaterialDB::getInstance().getId("Water"))
        {

          int chance = rand() % 100;
          if (chance < 60)
          {

            world->setParticle(x, y - 1,
                               MaterialDB::getInstance().getId("Wood"));
            Particle &aboveNew = world->getParticleRef(x, y - 1);
            aboveNew.age = p.age + 1;
            aboveNew.species = p.species;
          }
          else if (chance < 90)
          {

            world->setParticle(x, y - 1,
                               MaterialDB::getInstance().getId(
                                   p.species == 1 ? "PineNeedles" : "Plant"));
          }
          world->changeHumidity(x, y, -0.02f);
          world->changeNutrients(x, y, -0.01f);
          world->releaseVapor(x, 0.0015f);
          return;
        }
      }

      int dx = (rand() % 3) - 1;
      if (dx != 0 && world->inBounds(x + dx, y))
      {
        Particle &side = world->getParticleRef(x + dx, y);
        if (side.matId == MaterialDB::getInstance().getId("Empty"))
        {
          if (rand() % 100 < 10)
          {
            world->setParticle(x + dx, y,
                               MaterialDB::getInstance().getId("Wood"));
            Particle &sideNew = world->getParticleRef(x + dx, y);
            sideNew.age = p.age + 1;
            sideNew.species = p.species;
          }
        }
      }
    }

    if (temp > 0.8f)
    {
      world->setParticle(x, y, MaterialDB::getInstance().getId("Fire"));
      Particle &burningWood = world->getParticleRef(x, y);
      burningWood.lifetime_counter = 60;
    }
  }
  else if (def.name == "Plant" || def.name == "PineNeedles")
  {

    if (humidity < (def.name == "PineNeedles" ? 0.007f : 0.012f) && rand() % 1000 < 5)
    {
      world->setParticle(
          x, y, MaterialDB::getInstance().getId("Compost"));
    }

    if (temp > 0.6f)
    {
      world->setParticle(x, y, MaterialDB::getInstance().getId("Fire"));
      Particle &burningLeaf = world->getParticleRef(x, y);
      burningLeaf.lifetime_counter = 30;
    }
    if (humidity > 0.14f && nutrients > 0.08f && rand() % 200 < 2)
    {
      int nx = x + (rand() % 7) - 3;
      if (world->inBounds(nx, y - 1) && world->getParticle(nx, y - 1).matId == 0)
        world->setParticle(nx, y - 1,
                           MaterialDB::getInstance().getId(
                               def.name == "PineNeedles" ? "PineSeed" : "Seed"));
    }
  }
  else if (def.name == "Moss")
  {
    if (temp > 0.75f || humidity < 0.005f)
    {
      if (rand() % 100 < 3)
        world->setParticle(x, y, MaterialDB::getInstance().getId("Compost"));
    }
    else if (humidity > 0.15f && nutrients > 0.04f && rand() % 100 < 8)
    {
      int nx = x + (rand() % 3) - 1;
      if (world->inBounds(nx, y) && world->getParticle(nx, y).matId == 0 &&
          world->inBounds(nx, y + 1) &&
          world->getParticle(nx, y + 1).matId != 0)
        world->setParticle(nx, y, MaterialDB::getInstance().getId("Moss"));
    }
  }
  else if (def.name == "Algae")
  {
    const int water = MaterialDB::getInstance().getId("Water");
    const int saltwater = MaterialDB::getInstance().getId("Saltwater");
    const bool submerged =
        (world->inBounds(x, y + 1) &&
         (world->getParticle(x, y + 1).matId == water ||
          world->getParticle(x, y + 1).matId == saltwater)) ||
        (world->inBounds(x, y - 1) &&
         (world->getParticle(x, y - 1).matId == water ||
          world->getParticle(x, y - 1).matId == saltwater));
    if (temp > 0.65f || temp < -0.38f || (!submerged && humidity < 0.025f))
    {
      if (rand() % 100 < 8)
        world->setParticle(x, y, MaterialDB::getInstance().getId("Compost"));
    }
    else if (submerged && nutrients > 0.025f && rand() % 100 < 35)
    {
      const int nx = x + rand() % 3 - 1;
      const int ny = y + rand() % 3 - 1;
      if (world->inBounds(nx, ny))
      {
        const int target = world->getParticle(nx, ny).matId;
        if (target == water || target == saltwater || target == 0)
        {
          world->setParticle(nx, ny, MaterialDB::getInstance().getId("Algae"));
          world->changeNutrients(x, y, -0.012f);
        }
      }
    }
  }
  else if (def.name == "Grass" || def.name == "Flower")
  {
    if (temp > 0.73f || temp < -0.45f || humidity < 0.008f)
    {
      if (rand() % 100 < 7)
        world->setParticle(x, y, MaterialDB::getInstance().getId("Compost"));
    }
    else if (humidity > 0.045f && nutrients > 0.065f)
    {
      if (def.name == "Grass" && rand() % 100 < 30)
      {
        const int nx = x + (rand() % 5) - 2;
        if (world->inBounds(nx, y) && world->inBounds(nx, y + 1) &&
            world->getParticle(nx, y).matId == 0)
        {
          const int below = world->getParticle(nx, y + 1).matId;
          if (below == MaterialDB::getInstance().getId("Soil") ||
              below == MaterialDB::getInstance().getId("Mud") ||
              below == MaterialDB::getInstance().getId("Compost") ||
              below == MaterialDB::getInstance().getId("Peat"))
            world->setParticle(nx, y, MaterialDB::getInstance().getId("Grass"));
        }
      }
      else if (def.name == "Flower" && rand() % 100 < 12)
      {
        const int nx = x + rand() % 11 - 5;
        if (world->inBounds(nx, y - 1) &&
            world->getParticle(nx, y - 1).matId == 0)
          world->setParticle(nx, y - 1, MaterialDB::getInstance().getId("Seed"));
      }
    }
  }
  else if (def.name == "Shrub")
  {
    if (temp > 0.80f || temp < -0.35f || humidity < 0.006f)
    {
      if (rand() % 100 < 8)
        world->setParticle(x, y, MaterialDB::getInstance().getId("Compost"));
    }
    else if (rand() % 100 < 15)
    {
      const int nx = x + rand() % 9 - 4;
      if (world->inBounds(nx, y - 1) &&
          world->getParticle(nx, y - 1).matId == 0)
        world->setParticle(nx, y - 1,
                           MaterialDB::getInstance().getId("ShrubSeed"));
    }
  }
  else if (def.name == "Mushroom" || def.name == "Mycelium")
  {
    const bool suitable = humidity > 0.13f && temp > -0.24f && temp < 0.56f;
    if (!suitable && rand() % 100 < 5)
    {
      world->setParticle(x, y, MaterialDB::getInstance().getId("Compost"));
    }
    else if (suitable && nutrients > 0.08f && rand() % 100 < 21)
    {
      const int dx = rand() % 3 - 1;
      const int dy = rand() % 3 - 1;
      const int nx = x + dx, ny = y + dy;
      if (!world->inBounds(nx, ny))
        return;
      const int target = world->getParticle(nx, ny).matId;
      const int compost = MaterialDB::getInstance().getId("Compost");
      const int rottingWood = MaterialDB::getInstance().getId("Wood");
      if (target == compost ||
          (target == rottingWood && rand() % 100 < 2))
      {
        world->setParticle(nx, ny,
                           MaterialDB::getInstance().getId("Mycelium"));
        world->changeNutrients(nx, ny, 0.035f);
      }
      else if (def.name == "Mycelium" && target == 0 &&
               world->inBounds(nx, ny + 1) &&
               world->getParticle(nx, ny + 1).matId == compost &&
               rand() % 100 < 12)
        world->setParticle(nx, ny,
                           MaterialDB::getInstance().getId("Mushroom"));
    }
  }
}
