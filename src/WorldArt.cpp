#include "World.h"
#include <array>
#include <string>

bool World::loadBackgrounds(const std::filesystem::path &assetRoot)
{
  backgroundRoot = assetRoot;
  backgroundLayers.clear();
  constexpr std::array<const char *, 5> names = {
      "temperate-hills.png", "temperate-forest.png", "wetland-pines.png",
      "wetland-trees.png", "arid-rocks.png"};
  bool loaded = false;
  for (const char *name : names)
  {
    sf::Image image;
    if (!image.loadFromFile((assetRoot / name).string()))
    {
      backgroundLayers.emplace_back();
      continue;
    }
    const auto size = image.getSize();
    BackgroundLayer layer;
    layer.width = static_cast<int>(size.x);
    layer.height = static_cast<int>(size.y);
    layer.parallax = name[0] == 'a' ? 0.22f : std::string(name).find("forest") != std::string::npos || std::string(name).find("trees") != std::string::npos ? 0.11f
                                                                                                                                                            : 0.21f;
    const auto *pixels = image.getPixelsPtr();
    layer.rgba.assign(pixels, pixels + static_cast<std::size_t>(size.x) * size.y * 4);
    backgroundLayers.push_back(std::move(layer));
    loaded = true;
  }
  return loaded;
}
