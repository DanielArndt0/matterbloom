#pragma once

#include <algorithm>
#include <cmath>

struct SimulationTuning
{
  float speed = 0.70f;
  float gravity = 0.70f;
  int physicsHz = 60;
  int maxUpdatesPerFrame = 3;
  float frameBudgetMs = 10.0f;
  int fpsLimit = 60;
  bool verticalSync = false;
  bool heatGlow = true;
  float lightStrength = 1.0f;

  void sanitize()
  {
    if (!std::isfinite(speed))
      speed = 0.70f;
    if (!std::isfinite(gravity))
      gravity = 0.70f;
    if (!std::isfinite(frameBudgetMs))
      frameBudgetMs = 10.0f;
    if (!std::isfinite(lightStrength))
      lightStrength = 1.0f;
    speed = std::clamp(speed, 0.10f, 3.0f);
    gravity = std::clamp(gravity, 0.10f, 2.0f);
    physicsHz = std::clamp(physicsHz, 20, 120);
    maxUpdatesPerFrame = std::clamp(maxUpdatesPerFrame, 1, 8);
    frameBudgetMs = std::clamp(frameBudgetMs, 2.0f, 25.0f);
    fpsLimit = std::clamp(fpsLimit, 30, 144);
    lightStrength = std::clamp(lightStrength, 0.35f, 1.65f);
  }
  float timeStep() const { return 1.0f / static_cast<float>(physicsHz); }
};

struct SimulationPacer
{
  double debt = 0.0;

  void push(float frameSeconds, const SimulationTuning &settings)
  {
    if (!std::isfinite(frameSeconds) || frameSeconds <= 0.0f)
      return;
    const double step = settings.timeStep();
    debt = std::min(debt + std::min<double>(frameSeconds, 0.10) * settings.speed,
                    step * settings.maxUpdatesPerFrame * 1.5);
  }
  bool ready(const SimulationTuning &settings) const
  {
    return debt + 1e-8 >= static_cast<double>(settings.timeStep());
  }
  void consume(const SimulationTuning &settings)
  {
    debt = std::max(0.0, debt - settings.timeStep());
  }
  void reset() { debt = 0.0; }
};
