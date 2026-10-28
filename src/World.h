#pragma once

#include "Chunk.h"
#include "MaterialDB.h"
#include "Particle.h"
#include <SFML/Graphics.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

struct WorldStats
{
  std::size_t particles = 0;
  std::size_t plants = 0;
  std::size_t lifeCells = 0;
  std::uint64_t reactions = 0;
  float daylight = 0.0f;
  float soilMoisture = 0.0f;
  float cloudCover = 0.0f;
  float rainIntensity = 0.0f;
  float stormIntensity = 0.0f;
  bool tornadoActive = false;
  std::size_t movingParticles = 0;
};

class World
{
public:
  enum class RenderMode
  {
    Normal,
    Temperature,
    Humidity,
    Nutrients,
    Saturation,
    Pressure,
    Stress,
    Velocity
  };
  enum class Biome
  {
    Temperate,
    Arid,
    Wetland
  };
  enum class Environment
  {
    Natural,
    Laboratory
  };
  enum class ForceMode
  {
    Launch,
    Impulse,
    Fan,
    Jet,
    Attraction,
    Explosion,
    Anchor,
    Release
  };

  struct Emitter
  {
    int x = 0, y = 0, materialId = 0;
    ForceMode mode = ForceMode::Jet;
    float intensity = 3.0f, radius = 4.0f;
    float directionX = 1.0f, directionY = 0.0f;
  };

  struct Snapshot
  {
    std::vector<Particle> grid;
    std::vector<float> temperature, humidity, nutrients;
    std::vector<float> soilWater, atmosphericVapor, atmosphericCloud;
    int width = 0, height = 0;
    int groundLevel = 0;
    float elapsedTime = 0.0f;
    float timeOfDay = 0.0f;
    float daylight = 0.7f;
    bool rainEnabled = false;
    bool dayNightEnabled = true;
    Biome biome = Biome::Temperate;
    Environment environment = Environment::Natural;
    std::vector<Emitter> emitters;
    std::uint32_t seed = 1;
    std::uint64_t reactions = 0;
    float cloudiness = 0.2f;
    float wind = 0.15f;
    bool automaticWeather = true;
    float stormIntensity = 0.0f;
    int stormTicks = 0;
    float tornadoX = 0.0f;
    int tornadoTicks = 0;
    float climateOffset = 0.0f;
    float dayLengthSeconds = 120.0f;
  };

  World(int width, int height);
  void resize(int width, int height);
  void setParticle(int x, int y, int matId);
  Particle getParticle(int x, int y) const;
  Particle &getParticleRef(int x, int y);
  void clear();
  void createEcosystem(Biome biome = Biome::Temperate,
                       std::uint32_t seed = 1);
  void createLaboratory(std::uint32_t seed = 1);
  void update(float dt);
  void render(sf::RenderWindow &window);
  bool save(const std::filesystem::path &path) const;
  bool load(const std::filesystem::path &path);
  Snapshot captureSnapshot() const;
  void restoreSnapshot(const Snapshot &snapshot);
  WorldStats getStats() const;

  void setScale(float scaleX, float scaleY);
  void setPosition(float x, float y);
  void setBackgroundPan(float panWorldX);
  void setGraphicsQuality(int quality);
  void setRenderTuning(bool glow, float lighting);
  void setGravityScale(float scale);
  float getGravityScale() const { return gravityScale; }
  sf::Vector2f getSpriteScale() const;
  sf::Vector2f getSpritePosition() const;
  int getWidth() const { return width; }
  int getHeight() const { return height; }
  Biome getBiome() const { return biome; }
  Environment getEnvironment() const { return environment; }
  std::uint32_t getSeed() const { return seed; }
  bool inBounds(int x, int y) const;
  int getIndex(int x, int y) const;
  float getTemperature(int x, int y) const;
  float getHumidity(int x, int y) const;
  float getNutrients(int x, int y) const;
  float getSoilSaturation(int x, int y) const;
  float getPressure(int x, int y) const;
  int getGroundLevel() const { return groundLevel; }
  float getRainIntensity() const { return rainIntensity; }
  bool loadBackgrounds(const std::filesystem::path &assetRoot);
  void changeNutrients(int x, int y, float amount);
  void changeHumidity(int x, int y, float amount);
  void changeTemperature(int x, int y, float amount);
  int applyForce(int x, int y, float radius, float intensity, ForceMode mode,
                 float directionX = 1.0f, float directionY = 0.0f,
                 int selectedMaterial = 0, bool directed = false);
  int emitMaterial(int x, int y, int materialId, int amount,
                   float intensity, float directionX, float directionY);
  bool installEmitter(const Emitter &emitter);
  void clearEmitters();
  std::size_t getEmitterCount() const { return emitters.size(); }
  float getClockHour() const;
  void setClockHour(float hour);
  float dayLengthSeconds = 120.0f;
  float getStormIntensity() const { return stormIntensity; }
  const std::deque<std::string> &getEvents() const { return recentEvents; }
  void consumeSoilWater(int x, int y, float amount);
  void releaseVapor(int x, float amount);

  RenderMode renderMode = RenderMode::Normal;
  bool rainEnabled = false;
  bool dayNightEnabled = true;
  float daylight = 0.7f;
  float elapsedTime = 0.0f;
  float timeOfDay = 0.0f;
  float cloudiness = 0.2f;
  float wind = 0.15f;
  bool automaticWeather = true;
  float climateOffset = 0.0f;

private:
  int width;
  int height;
  int chunksX;
  int chunksY;
  std::vector<Particle> grid;
  std::vector<Chunk> chunks;
  std::vector<sf::Uint8> pixelData;
  std::vector<sf::Color> skyRows;
  std::vector<int> farHeight, nearHeight, cloudBand;
  std::vector<float> cloudWeight;
  sf::Texture texture;
  sf::Sprite sprite;
  std::vector<float> tempA, tempB;
  std::vector<float> humidA, humidB;
  std::vector<float> nutA, nutB;
  std::vector<float> soilWater, soilWaterNext, pressureField;
  std::vector<float> atmosphericVapor, atmosphericCloud;
  float rainIntensity = 0.0f;
  float stormIntensity = 0.0f;
  int stormTicks = 0;
  float tornadoX = 0.0f;
  int tornadoTicks = 0;
  int groundLevel = 360;
  int hydrologyTickCounter = 0;
  int pressureTickCounter = 0;
  struct BackgroundLayer
  {
    int width = 0, height = 0;
    float parallax = 0.0f;
    std::vector<sf::Uint8> rgba;
  };
  std::vector<BackgroundLayer> backgroundLayers;
  std::filesystem::path backgroundRoot;
  bool usingBufferA = true;
  int fieldTickCounter = 0;
  int lifeTickCounter = 0;
  int weatherTickCounter = 0;
  int structuralTickCounter = 0;
  int structuralActivityTicks = 0;
  bool hasLife = false;
  std::uint64_t reactionEvents = 0;
  Biome biome = Biome::Temperate;
  Environment environment = Environment::Natural;
  std::uint32_t seed = 1;
  float ambientTemperature = 0.06f;
  float ambientHumidity = 0.09f;
  float backgroundPanWorld = 0.0f;
  int graphicsQuality = 1;
  bool heatGlowEnabled = true;
  float lightingStrength = 1.0f;
  float gravityScale = 0.70f;
  unsigned graphicsFrameCounter = 0;
  std::vector<unsigned char> glowTiles;
  std::vector<unsigned char> glowBuffer;
  std::vector<unsigned char> structuralSupport;
  std::vector<unsigned char> structuralVisited;
  std::vector<std::uint32_t> structuralGroupIds;
  std::vector<Emitter> emitters;
  int emitterTickCounter = 0;
  std::deque<std::string> recentEvents;

  void wakeAt(int x, int y);
  void swapParticles(int x1, int y1, int x2, int y2);
  bool tryMove(int x, int y, int nx, int ny, const MaterialDef &def);
  bool reactAt(int x, int y, int materialId);
  bool transformAt(int x, int y, int materialId);
  void updateFields();
  void updateLife();
  void updateWeather();
  void updateHydrology();
  void updatePressure();
  void updateStructures();
  void updateEmitters();
  bool isStructural(int materialId) const;
  bool integrateMotion(int x, int y, const MaterialDef &material);
  void updateFluidMotion(int x, int y, const MaterialDef &material);
  void damageStructure(int x, int y, float amount);
  void logEvent(const std::string &message);
  float porosity(int materialId) const;
  void prepareAtmosphere();
  float neighborAverage(const std::vector<float> &values, int x, int y) const;
};
