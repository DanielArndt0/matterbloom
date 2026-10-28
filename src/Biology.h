#pragma once

#include "Particle.h"

class World;

class Biology
{
public:
  static void processPlant(World *world, int x, int y, Particle &p);
};
