#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include <cstdint>

enum class MatterState
{
    Solid,
    Granular,
    Liquid,
    Gas,
    Plasma,
    Special
};

struct MaterialDef
{
    int id = 0;
    std::string name;
    MatterState state = MatterState::Solid;

    sf::Color baseColor;
    int colorVariance = 0;

    float density = 1.0f;
    float friction = 0.5f;
    float dispersionRate = 1;
    float porosity = 0.0f;
    float permeability = 0.0f;
    float cohesion = 0.8f;
    float pressureResistance = 0.8f;
    float viscosity = 0.20f;
    float surfaceTension = 0.25f;
    float turbulence = 0.16f;
    float compressibility = 0.04f;
    float pressureResponse = 1.0f;

    int lifetime = -1;
    bool flammability = false;
    bool isAcid = false;
    float thermalConductivity = 0.12f;
    float heatCapacity = 1.0f;
    float initialTemperature = 2.0f;
    float ignitionTemperature = 2.0f;
    float ignitionProbability = 0.0f;
    bool hasTransitions = false;
    std::string description;
};

struct TransitionDef
{
    float minTemperature = -1.0f;
    float maxTemperature = 1.0f;
    int product = 0;
    float probability = 1.0f;
    float heatDelta = 0.0f;
};

struct ReactionDef
{
    int reagent1;
    int reagent2;
    int product1;
    int product2;
    float probability;
    float minTemperature = -1.0f;
    float maxTemperature = 1.0f;
    float minHumidity = 0.0f;
    float maxHumidity = 1.0f;
    float heatDelta = 0.0f;
};

class MaterialDB
{
public:
    static MaterialDB &getInstance()
    {
        static MaterialDB instance;
        return instance;
    }

    void loadMaterials(const std::string &filepath);
    void loadReactions(const std::string &filepath);

    const MaterialDef &getMaterial(int id) const;
    const MaterialDef &getMaterial(const std::string &name) const;

    const ReactionDef *getReaction(int mat1, int mat2) const;
    const std::vector<TransitionDef> &getTransitions(int materialId) const;

    int getId(const std::string &name) const;

    size_t getMaterialCount() const;

private:
    MaterialDB();
    ~MaterialDB() = default;

    static const int MAX_MATERIALS = 256;
    MaterialDef materialsArr[MAX_MATERIALS];
    ReactionDef reactionsArr[MAX_MATERIALS][MAX_MATERIALS];
    bool hasReactionArr[MAX_MATERIALS][MAX_MATERIALS];
    std::vector<TransitionDef> transitionsArr[MAX_MATERIALS];

    std::unordered_map<std::string, int> nameToId;
    int maxMaterialId = 0;

    MaterialDef fallbackMaterial;
};
