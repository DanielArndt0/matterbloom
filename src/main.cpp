#include "imgui-SFML.h"
#include "imgui.h"

#include "MaterialDB.h"
#include "World.h"
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window.hpp>
#include <iostream>

int main()
{
  const int WINDOW_WIDTH = 1000;
  const int WINDOW_HEIGHT = 800;
  const int SIM_WIDTH = 512;
  const int SIM_HEIGHT = 512;

  sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
                          "Pixel Physics Simulator - Extended");
  window.setFramerateLimit(60);

  ImGui::SFML::Init(window);

  World world(SIM_WIDTH, SIM_HEIGHT);

  float scaleX = static_cast<float>(WINDOW_WIDTH) / SIM_WIDTH;
  float scaleY = static_cast<float>(WINDOW_HEIGHT) / SIM_HEIGHT;
  world.setScale(scaleX, scaleY);

  MaterialDB::getInstance().loadMaterials("materials.json");
  MaterialDB::getInstance().loadReactions("reactions.json");

  int currentBrush = MaterialDB::getInstance().getId("Sand");
  int emptyBrush = MaterialDB::getInstance().getId("Empty");
  int brushSize = 5;
  bool isPaused = false;
  bool stepFrame = false;
  float simSpeed = 1.0f;

  const float TIME_STEP = 1.0f / 60.0f;
  float accumulatedTime = 0.0f;

  sf::Clock deltaClock;

  while (window.isOpen())
  {
    sf::Time dtTime = deltaClock.restart();
    float dt = dtTime.asSeconds();

    sf::Event event;
    while (window.pollEvent(event))
    {
      ImGui::SFML::ProcessEvent(window, event);

      if (event.type == sf::Event::Closed)
        window.close();

      ImGuiIO &io = ImGui::GetIO();
      if (!io.WantCaptureMouse && !io.WantCaptureKeyboard)
      {
        if (event.type == sf::Event::KeyPressed)
        {
          if (event.key.code == sf::Keyboard::Space)
            isPaused = !isPaused;
        }

        if (event.type == sf::Event::MouseWheelScrolled)
        {
          if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel)
          {
            brushSize += static_cast<int>(event.mouseWheelScroll.delta);
            if (brushSize < 1)
              brushSize = 1;
            if (brushSize > 50)
              brushSize = 50;
          }
        }
      }
    }

    ImGui::SFML::Update(window, dtTime);

    ImGui::Begin("Toolbar");

    ImGui::Text("Brush Settings");
    ImGui::SliderInt("Size", &brushSize, 1, 50);

    ImGui::Separator();
    ImGui::Text("Powders & Sands");
    if (ImGui::Button("Sand", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Sand");
    ImGui::SameLine();
    if (ImGui::Button("Soil", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Soil");
    ImGui::SameLine();
    if (ImGui::Button("Gravel", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Gravel");
    ImGui::SameLine();
    if (ImGui::Button("Silt", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Silt");

    if (ImGui::Button("Salt", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Salt");
    ImGui::SameLine();
    if (ImGui::Button("Ash", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Ash");
    ImGui::SameLine();
    if (ImGui::Button("Charcoal", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Charcoal");
    ImGui::SameLine();
    if (ImGui::Button("Seed", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Seed");

    ImGui::Separator();
    ImGui::Text("Solids & Structures");
    if (ImGui::Button("Wall", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Wall");
    ImGui::SameLine();
    if (ImGui::Button("Stone", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Stone");
    ImGui::SameLine();
    if (ImGui::Button("Wood", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Wood");
    ImGui::SameLine();
    if (ImGui::Button("Clay", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Clay");

    if (ImGui::Button("Brick", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Brick");
    ImGui::SameLine();
    if (ImGui::Button("Glass", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Glass");
    ImGui::SameLine();
    if (ImGui::Button("Metal", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Metal");
    ImGui::SameLine();
    if (ImGui::Button("Rust", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Rust");

    if (ImGui::Button("Rubber", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Rubber");
    ImGui::SameLine();
    if (ImGui::Button("Fabric", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Fabric");
    ImGui::SameLine();
    if (ImGui::Button("Web", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Web");

    ImGui::Separator();
    ImGui::Text("Liquids");
    if (ImGui::Button("Water", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Water");
    ImGui::SameLine();
    if (ImGui::Button("Mud", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Mud");
    ImGui::SameLine();
    if (ImGui::Button("Saltwater", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Saltwater");
    ImGui::SameLine();
    if (ImGui::Button("Fuel", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Fuel");

    if (ImGui::Button("Resin", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Resin");
    ImGui::SameLine();
    if (ImGui::Button("Acid", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Acid");
    ImGui::SameLine();
    if (ImGui::Button("Alkali", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Alkali");
    ImGui::SameLine();
    if (ImGui::Button("Lava", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Lava");

    ImGui::Separator();
    ImGui::Text("Gases & Energy");
    if (ImGui::Button("Fire", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Fire");
    ImGui::SameLine();
    if (ImGui::Button("Smoke", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Smoke");
    ImGui::SameLine();
    if (ImGui::Button("Steam", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("Steam");
    ImGui::SameLine();
    if (ImGui::Button("ToxicGas", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("ToxicGas");

    if (ImGui::Button("Spore", ImVec2(70, 25)))
      currentBrush = MaterialDB::getInstance().getId("SporeCloud");

    ImGui::Separator();
    ImGui::Text("Cellular Automata");
    if (ImGui::Button("Game of Life", ImVec2(100, 30)))
      currentBrush = MaterialDB::getInstance().getId("Life");

    ImGui::Separator();
    ImGui::Text("Tools");
    if (ImGui::Button("Eraser", ImVec2(100, 30)))
      currentBrush = emptyBrush;

    ImGui::Spacing();
    if (ImGui::Button(isPaused ? "Resume (Space)" : "Pause (Space)",
                      ImVec2(150, 40)))
    {
      isPaused = !isPaused;
    }

    ImGui::Text("Simulation Speed");
    if (ImGui::Button("0.25x"))
      simSpeed = 0.25f;
    ImGui::SameLine();
    if (ImGui::Button("0.5x"))
      simSpeed = 0.5f;
    ImGui::SameLine();
    if (ImGui::Button("1.0x"))
      simSpeed = 1.0f;
    ImGui::SameLine();
    if (ImGui::Button("2.0x"))
      simSpeed = 2.0f;
    ImGui::SameLine();
    if (ImGui::Button("4.0x"))
      simSpeed = 4.0f;

    ImGui::Separator();
    ImGui::Text("Render Options");
    static int currentRenderMode = 0;
    const char *renderModes[] = {"Normal", "Temperature Map", "Humidity Map"};
    if (ImGui::Combo("View", &currentRenderMode, renderModes,
                     IM_ARRAYSIZE(renderModes)))
    {
      world.setRenderMode(static_cast<World::RenderMode>(currentRenderMode));
    }

    if (isPaused)
    {
      if (ImGui::Button("Step Frame (N)", ImVec2(150, 30)))
      {
        stepFrame = true;
      }
    }

    if (ImGui::Button("Clear Screen", ImVec2(150, 40)))
    {
      for (int y = 0; y < SIM_HEIGHT; ++y)
      {
        for (int x = 0; x < SIM_WIDTH; ++x)
        {
          world.setParticle(x, y, emptyBrush);
        }
      }
    }

    ImGui::End();

    bool isWindowFocused = window.hasFocus();
    ImGuiIO &ioState = ImGui::GetIO();

    if (isWindowFocused && !ioState.WantCaptureMouse &&
        sf::Mouse::isButtonPressed(sf::Mouse::Left))
    {
      sf::Vector2i mousePos = sf::Mouse::getPosition(window);

      if (mousePos.x >= 0 && mousePos.x < WINDOW_WIDTH && mousePos.y >= 0 &&
          mousePos.y < WINDOW_HEIGHT)
      {

        sf::Vector2f scale = world.getSpriteScale();
        float scaleX = scale.x;
        float scaleY = scale.y;

        float simX = mousePos.x / scaleX;
        float simY = mousePos.y / scaleY;

        int cx = static_cast<int>(simX);
        int cy = static_cast<int>(simY);

        for (int y = cy - brushSize; y <= cy + brushSize; ++y)
        {
          for (int x = cx - brushSize; x <= cx + brushSize; ++x)
          {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <=
                brushSize * brushSize)
            {

              world.setParticle(x, y, currentBrush);
            }
          }
        }
      }
    }

    if (!isPaused)
    {
      accumulatedTime += dt * simSpeed;

      if (accumulatedTime > TIME_STEP * 10.0f)
      {
        accumulatedTime = TIME_STEP * 10.0f;
      }

      while (accumulatedTime >= TIME_STEP)
      {
        world.update(TIME_STEP);
        accumulatedTime -= TIME_STEP;
      }
    }
    else if (stepFrame)
    {
      world.update(TIME_STEP);
      stepFrame = false;
    }

    window.clear();

    world.render(window);

    ImGui::SFML::Render(window);
    window.display();
  }

  ImGui::SFML::Shutdown();
  return 0;
}
