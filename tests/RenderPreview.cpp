#include "../src/World.h"
#include <fstream>
#include <iostream>

void savePreview(const World &world, const char *path)
{
  std::ofstream file(path, std::ios::binary);
  file << "P6\n"
       << world.getWidth() << " " << world.getHeight() << "\n255\n";
  const auto &pixels = sf::Texture::lastPixels;
  for (std::size_t i = 0; i < pixels.size(); i += 4)
    file.write(reinterpret_cast<const char *>(&pixels[i]), 3);
}

int main()
{
  MaterialDB::getInstance().loadMaterials("");
  MaterialDB::getInstance().loadReactions("");
  sf::RenderWindow window;
  World world(512, 512);
  world.setGraphicsQuality(2);
  world.createEcosystem(World::Biome::Temperate, 20261025);
  world.daylight = 0.92f;
  world.render(window);
  savePreview(world, "/tmp/matterbloom-070-floresta.ppm");
  world.daylight = 0.12f;
  world.render(window);
  savePreview(world, "/tmp/matterbloom-070-noite.ppm");
  world.createEcosystem(World::Biome::Arid, 20261025);
  world.daylight = 0.92f;
  world.render(window);
  savePreview(world, "/tmp/matterbloom-070-deserto.ppm");
  world.createEcosystem(World::Biome::Wetland, 20261026);
  world.daylight = 0.68f;
  world.render(window);
  savePreview(world, "/tmp/matterbloom-070-pantano.ppm");
  std::cout << "Visual previews written to /tmp\n";
}
