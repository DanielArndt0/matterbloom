#pragma once
#include "SFML/Graphics.hpp"
namespace ImGui
{
  namespace SFML
  {
    inline void Init(sf::RenderWindow &) {}
    inline bool UpdateFontTexture() { return true; }
    inline void Shutdown() {}
    inline void ProcessEvent(sf::RenderWindow &, const sf::Event &) {}
    inline void Update(sf::RenderWindow &, sf::Time) {}
    inline void Render(sf::RenderWindow &) {}
  }
}
