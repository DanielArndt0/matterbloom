#include "../src/MaterialDB.h"
#include <algorithm>
#include <utility>

MaterialDB::MaterialDB()
{
  fallbackMaterial.id = 0;
  fallbackMaterial.name = "Empty";
  fallbackMaterial.state = MatterState::Special;
  fallbackMaterial.baseColor = sf::Color::Black;
  fallbackMaterial.density = 0;
  for (int i = 0; i < MAX_MATERIALS; ++i)
  {
    materialsArr[i] = fallbackMaterial;
    for (int j = 0; j < MAX_MATERIALS; ++j)
      hasReactionArr[i][j] = false;
  }
  nameToId["Empty"] = 0;
}

void MaterialDB::loadMaterials(const std::string &)
{
  struct Entry
  {
    int id;
    const char *name;
    MatterState state;
    float density;
  };
  const Entry entries[] = {
      {1, "Sand", MatterState::Granular, 1.5f},
      {2, "Water", MatterState::Liquid, 1.0f},
      {3, "Wall", MatterState::Solid, 1.0f},
      {4, "Wood", MatterState::Solid, 1.0f},
      {5, "Fire", MatterState::Plasma, -1.0f},
      {6, "Smoke", MatterState::Gas, -0.5f},
      {7, "Oil", MatterState::Liquid, 0.8f},
      {8, "Acid", MatterState::Liquid, 1.1f},
      {9, "Stone", MatterState::Solid, 2.5f},
      {10, "Life", MatterState::Special, 0.0f},
      {11, "Soil", MatterState::Granular, 1.4f},
      {12, "Gravel", MatterState::Granular, 2.0f},
      {13, "Silt", MatterState::Granular, 1.1f},
      {14, "Salt", MatterState::Granular, 1.3f},
      {15, "Ash", MatterState::Granular, 0.5f},
      {16, "Charcoal", MatterState::Granular, 1.2f},
      {17, "Seed", MatterState::Granular, 1.0f},
      {18, "Plant", MatterState::Solid, 0.4f},
      {19, "Compost", MatterState::Granular, 1.2f},
      {20, "Clay", MatterState::Solid, 2.0f},
      {21, "Brick", MatterState::Solid, 2.5f},
      {22, "Glass", MatterState::Solid, 2.2f},
      {23, "Metal", MatterState::Solid, 5.0f},
      {24, "Rust", MatterState::Solid, 4.5f},
      {25, "Rubber", MatterState::Solid, 1.2f},
      {26, "Fabric", MatterState::Solid, 0.3f},
      {27, "Web", MatterState::Solid, 0.1f},
      {28, "Moss", MatterState::Solid, 0.3f},
      {29, "Ice", MatterState::Solid, 0.9f},
      {30, "Mud", MatterState::Liquid, 1.6f},
      {31, "Saltwater", MatterState::Liquid, 1.1f},
      {32, "Fuel", MatterState::Liquid, 0.7f},
      {33, "Resin", MatterState::Liquid, 1.2f},
      {34, "Alkali", MatterState::Liquid, 1.0f},
      {35, "Lava", MatterState::Liquid, 3.0f},
      {40, "Steam", MatterState::Gas, -0.8f},
      {41, "ToxicGas", MatterState::Gas, -0.4f},
      {42, "SporeCloud", MatterState::Gas, -0.2f},
      {43, "Basalt", MatterState::Solid, 3.0f},
      {44, "Obsidian", MatterState::Solid, 2.4f},
      {45, "MoltenMetal", MatterState::Liquid, 5.1f},
      {46, "Hydrogen", MatterState::Gas, -0.95f},
      {47, "CarbonDioxide", MatterState::Gas, -0.15f},
      {48, "MoltenGlass", MatterState::Liquid, 2.2f},
      {49, "Algae", MatterState::Solid, 0.32f},
      {50, "Grass", MatterState::Solid, 0.25f},
      {51, "Flower", MatterState::Solid, 0.23f},
      {52, "Sandstone", MatterState::Solid, 2.1f},
      {53, "Limestone", MatterState::Solid, 2.5f},
      {54, "Granite", MatterState::Solid, 2.7f},
      {55, "Shale", MatterState::Solid, 2.4f},
      {56, "IronOre", MatterState::Solid, 4.0f},
      {57, "CopperOre", MatterState::Solid, 3.6f},
      {58, "Coal", MatterState::Solid, 1.5f},
      {59, "Quartz", MatterState::Solid, 2.6f},
      {60, "PineSeed", MatterState::Granular, 0.8f},
      {61, "MangroveSeed", MatterState::Granular, 0.8f},
      {62, "PineNeedles", MatterState::Solid, 0.2f},
      {63, "Mushroom", MatterState::Solid, 0.4f},
      {64, "Mycelium", MatterState::Solid, 0.4f},
      {65, "ShrubSeed", MatterState::Granular, 0.7f},
      {66, "Shrub", MatterState::Solid, 0.5f},
      {67, "FracturedRock", MatterState::Granular, 2.4f},
      {68, "Peat", MatterState::Granular, 0.8f},
      {69, "Concrete", MatterState::Solid, 2.5f},
      {70, "ReinforcedConcrete", MatterState::Solid, 3.1f},
      {71, "Roots", MatterState::Solid, 0.5f}};
  for (const auto &entry : entries)
  {
    auto &m = materialsArr[entry.id];
    m.id = entry.id;
    m.name = entry.name;
    m.state = entry.state;
    m.density = entry.density;
    m.baseColor = sf::Color(80, 150, 100);
    if (m.name == "Sand")
      m.baseColor = sf::Color(219, 195, 114);
    else if (m.name == "Soil")
      m.baseColor = sf::Color(90, 60, 40);
    else if (m.name == "Stone")
      m.baseColor = sf::Color(123, 139, 149);
    else if (m.name == "Water")
      m.baseColor = sf::Color(58, 143, 203);
    else if (m.name == "Wood")
      m.baseColor = sf::Color(125, 87, 53);
    else if (m.name == "Plant")
      m.baseColor = sf::Color(57, 164, 78);
    else if (m.name == "Seed")
      m.baseColor = sf::Color(149, 171, 55);
    else if (m.name == "Lava")
      m.baseColor = sf::Color(242, 91, 29);
    else if (m.name == "Fire")
      m.baseColor = sf::Color(255, 139, 39);
    else if (m.name == "Ice")
      m.baseColor = sf::Color(178, 219, 241);
    else if (m.name == "Grass")
      m.baseColor = sf::Color(105, 197, 78);
    else if (m.name == "Flower")
      m.baseColor = sf::Color(231, 116, 169);
    else if (m.name == "Algae")
      m.baseColor = sf::Color(48, 158, 122);
    m.dispersionRate = 2;
    nameToId[m.name] = m.id;
  }
  materialsArr[5].lifetime = 40;
  materialsArr[6].lifetime = 100;
  materialsArr[40].lifetime = 200;
  materialsArr[35].initialTemperature = 1.0f;
  materialsArr[5].initialTemperature = 1.0f;
  materialsArr[29].initialTemperature = -0.70f;
  materialsArr[40].initialTemperature = 0.72f;
  materialsArr[45].initialTemperature = 0.95f;
  materialsArr[48].initialTemperature = 0.91f;
  materialsArr[2].heatCapacity = 3.8f;
  materialsArr[2].thermalConductivity = 0.36f;
  for (int id : {1, 11, 12, 13, 19, 20, 52, 55, 68})
  {
    materialsArr[id].porosity = id == 11 ? 0.45f : id == 68 ? 0.68f
                                                            : 0.22f;
    materialsArr[id].permeability = id == 20 ? 0.08f : id == 11 ? 0.55f
                                                                : 0.37f;
  }
  materialsArr[4].flammability = true;
  materialsArr[4].ignitionTemperature = 0.55f;
  materialsArr[4].ignitionProbability = 0.08f;
  transitionsArr[2] = {{0.57f, 1.0f, 40, 0.018f, -0.12f},
                       {-1.0f, -0.46f, 29, 0.12f, 0.04f}};
  transitionsArr[29] = {{-0.15f, 1.0f, 2, 0.16f, -0.06f}};
  transitionsArr[40] = {{-1.0f, 0.21f, 2, 0.055f, 0.06f}};
  transitionsArr[35] = {{-1.0f, 0.47f, 43, 0.035f, 0.0f}};
  transitionsArr[23] = {{0.87f, 1.0f, 45, 0.07f, 0.0f}};
  transitionsArr[1] = {{0.82f, 1.0f, 48, 0.025f, 0.0f}};
  transitionsArr[22] = {{0.91f, 1.0f, 48, 0.012f, 0.0f}};
  transitionsArr[45] = {{-1.0f, 0.63f, 23, 0.13f, 0.0f}};
  transitionsArr[48] = {{-1.0f, 0.57f, 22, 0.15f, 0.0f}};
  for (int id = 0; id < MAX_MATERIALS; ++id)
    materialsArr[id].hasTransitions = !transitionsArr[id].empty();
}
void MaterialDB::loadReactions(const std::string &)
{
  auto add = [&](ReactionDef reaction)
  {
    reactionsArr[reaction.reagent1][reaction.reagent2] = reaction;
    hasReactionArr[reaction.reagent1][reaction.reagent2] = true;
    std::swap(reaction.reagent1, reaction.reagent2);
    std::swap(reaction.product1, reaction.product2);
    reactionsArr[reaction.reagent1][reaction.reagent2] = reaction;
    hasReactionArr[reaction.reagent1][reaction.reagent2] = true;
  };
  add({2, 35, 40, 44, 1.0f});
  add({14, 2, 0, 31, 1.0f});
  add({35, 1, 35, 48, 1.0f});
  add({5, 20, 5, 21, 1.0f, 0.35f});
  add({8, 23, 46, 24, 1.0f});
  add({5, 46, 5, 5, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.18f});
}
const MaterialDef &MaterialDB::getMaterial(int id) const
{
  return id >= 0 && id < MAX_MATERIALS ? materialsArr[id] : fallbackMaterial;
}
const MaterialDef &MaterialDB::getMaterial(const std::string &name) const
{
  return getMaterial(getId(name));
}
int MaterialDB::getId(const std::string &name) const
{
  const auto it = nameToId.find(name);
  return it == nameToId.end() ? 0 : it->second;
}
size_t MaterialDB::getMaterialCount() const { return nameToId.size(); }
const ReactionDef *MaterialDB::getReaction(int a, int b) const
{
  if (a < 0 || a >= MAX_MATERIALS || b < 0 || b >= MAX_MATERIALS)
    return nullptr;
  return hasReactionArr[a][b] ? &reactionsArr[a][b] : nullptr;
}
const std::vector<TransitionDef> &MaterialDB::getTransitions(int materialId) const
{
  static const std::vector<TransitionDef> empty;
  return materialId >= 0 && materialId < MAX_MATERIALS ? transitionsArr[materialId] : empty;
}
