#include "World.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char **argv)
{
  std::srand(12345);
  auto &db = MaterialDB::getInstance();
  db.loadMaterials("");
  db.loadReactions("");
  const int width = argc > 1 ? std::clamp(std::atoi(argv[1]), 64, 1024) : 512;
  const int height = argc > 2 ? std::clamp(std::atoi(argv[2]), 64, 1024) : 512;
  World world(width, height);
  for (int x = 0; x < width; ++x)
    for (int y = std::max(0, height - 112); y < height; ++y)
      world.setParticle(x, y, db.getId("Stone"));
  for (int x = width * 18 / 100; x < width * 82 / 100; ++x)
    for (int y = height * 33 / 100; y < height * 46 / 100; ++y)
      world.setParticle(x, y, (x + y) % 2 ? db.getId("Lava") : db.getId("Water"));
  constexpr int steps = 180;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < steps; ++i)
    world.update(1.0f / 60.0f);
  const auto milliseconds = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - start)
                                .count();
  std::cout << std::fixed << std::setprecision(3)
            << "world=" << width << "x" << height
            << " steps=" << steps << " total_ms=" << milliseconds
            << " ms_per_tick=" << milliseconds / steps
            << " particles=" << world.getStats().particles << '\n';
  if (argc > 3)
    world.setGraphicsQuality(std::clamp(std::atoi(argv[3]), 0, 2));
  sf::RenderWindow window;
  constexpr int frames = 60;
  const auto renderStart = std::chrono::steady_clock::now();
  for (int i = 0; i < frames; ++i)
    world.render(window);
  const auto renderMilliseconds = std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - renderStart)
                                      .count();
  std::cout << "frames=" << frames
            << " graphics_cpu_ms_per_frame=" << renderMilliseconds / frames
            << '\n';
}
