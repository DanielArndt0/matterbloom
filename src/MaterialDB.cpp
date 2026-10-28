#include "MaterialDB.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <utility>
#include <algorithm>

using json = nlohmann::json;

MaterialDB::MaterialDB()
{

    fallbackMaterial.id = 0;
    fallbackMaterial.name = "Empty";
    fallbackMaterial.state = MatterState::Special;
    fallbackMaterial.baseColor = sf::Color::Black;
    fallbackMaterial.density = 0.0f;

    for (int i = 0; i < MAX_MATERIALS; ++i)
    {
        materialsArr[i] = fallbackMaterial;
        for (int j = 0; j < MAX_MATERIALS; ++j)
        {
            hasReactionArr[i][j] = false;
        }
    }

    materialsArr[0] = fallbackMaterial;
    nameToId["Empty"] = 0;
}

void MaterialDB::loadMaterials(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open material JSON: " << filepath << std::endl;

        return;
    }

    json j;
    try
    {
        file >> j;
    }
    catch (const json::parse_error &e)
    {
        std::cerr << "JSON Parse error: " << e.what() << std::endl;
        return;
    }

    if (!j.is_array())
    {
        std::cerr << "JSON error: Expected an array of materials at root." << std::endl;
        return;
    }

    std::vector<std::pair<int, json>> pendingTransitions;
    for (const auto &matObj : j)
    {
        MaterialDef def;

        def.id = matObj.value("id", -1);
        def.name = matObj.value("name", "Unknown");
        if (def.id <= 0)
        {
            std::cerr << "Skipping material with invalid ID <= 0: " << def.name << std::endl;
            continue;
        }

        std::string stateStr = matObj.value("state", "Solid");
        if (stateStr == "Solid")
            def.state = MatterState::Solid;
        else if (stateStr == "Granular")
            def.state = MatterState::Granular;
        else if (stateStr == "Liquid")
            def.state = MatterState::Liquid;
        else if (stateStr == "Gas")
            def.state = MatterState::Gas;
        else if (stateStr == "Plasma")
            def.state = MatterState::Plasma;
        else
            def.state = MatterState::Special;

        if (matObj.contains("color") && matObj["color"].is_array() && matObj["color"].size() >= 3)
        {
            auto &cArr = matObj["color"];
            def.baseColor = sf::Color(cArr[0], cArr[1], cArr[2], 255);
        }
        else
        {
            def.baseColor = sf::Color::Magenta;
        }

        def.colorVariance = matObj.value("colorVariance", 0);

        def.density = matObj.value("density", 1.0f);
        def.friction = matObj.value("friction", 0.5f);
        def.dispersionRate = matObj.value("dispersionRate", 1.0f);
        def.porosity = std::clamp(matObj.value("porosity", 0.0f), 0.0f, 1.0f);
        def.permeability = std::clamp(matObj.value("permeability", 0.0f), 0.0f, 1.0f);
        def.cohesion = std::clamp(matObj.value("cohesion", 0.8f), 0.0f, 1.0f);
        def.pressureResistance = std::clamp(matObj.value("pressureResistance", 0.8f), 0.0f, 1.0f);
        const float defaultViscosity = def.state == MatterState::Liquid ? 0.16f : def.state == MatterState::Gas ? 0.025f
                                                                                                                : 0.70f;
        def.viscosity = std::clamp(matObj.value("viscosity", defaultViscosity), 0.0f, 1.0f);
        def.surfaceTension = std::clamp(matObj.value("surfaceTension", def.state == MatterState::Liquid ? 0.27f : 0.0f), 0.0f, 1.0f);
        def.turbulence = std::clamp(matObj.value("turbulence", def.state == MatterState::Liquid ? 0.14f : 0.04f), 0.0f, 1.0f);
        def.compressibility = std::clamp(matObj.value("compressibility", def.state == MatterState::Gas ? 0.95f : 0.025f), 0.0f, 1.0f);
        def.pressureResponse = std::clamp(matObj.value("pressureResponse", 1.0f), 0.0f, 4.0f);
        def.lifetime = matObj.value("lifetime", -1);
        def.flammability = matObj.value("flammability", false);
        def.isAcid = matObj.value("isAcid", false);
        def.thermalConductivity = std::clamp(matObj.value("thermalConductivity", 0.12f), 0.01f, 1.0f);
        def.heatCapacity = std::clamp(matObj.value("heatCapacity", 1.0f), 0.1f, 6.0f);
        def.initialTemperature = std::clamp(matObj.value("initialTemperature", 2.0f), -1.0f, 2.0f);
        def.ignitionTemperature = std::clamp(matObj.value("ignitionTemperature", 2.0f), -1.0f, 2.0f);
        def.ignitionProbability = std::clamp(matObj.value("ignitionProbability", 0.0f), 0.0f, 1.0f);
        def.description = matObj.value("description", "");

        if (def.id >= 0 && def.id < MAX_MATERIALS)
        {
            materialsArr[def.id] = def;
            nameToId[def.name] = def.id;
            if (def.id > maxMaterialId)
                maxMaterialId = def.id;
            if (matObj.contains("transitions") && matObj["transitions"].is_array())
                pendingTransitions.emplace_back(def.id, matObj["transitions"]);
        }
        else
        {
            std::cerr << "Error: Material ID " << def.id << " exceeds MAX_MATERIALS.\n";
        }
    }

    for (auto &entry : pendingTransitions)
    {
        auto &transitions = transitionsArr[entry.first];
        transitions.clear();
        for (const auto &item : entry.second)
        {
            const std::string productName = item.value("product", "");
            const int result = getId(productName);
            if ((result == 0 && productName != "Empty") || productName.empty())
            {
                std::cerr << "Ignoring unknown phase product: " << productName << '\n';
                continue;
            }
            TransitionDef transition;
            transition.product = result;
            transition.minTemperature = std::clamp(item.value("minTemperature", -1.0f), -1.0f, 1.0f);
            transition.maxTemperature = std::clamp(item.value("maxTemperature", 1.0f), -1.0f, 1.0f);
            transition.probability = std::clamp(item.value("probability", 1.0f), 0.0f, 1.0f);
            transition.heatDelta = std::clamp(item.value("heatDelta", 0.0f), -0.5f, 0.5f);
            if (transition.minTemperature <= transition.maxTemperature)
                transitions.push_back(transition);
        }
        materialsArr[entry.first].hasTransitions = !transitions.empty();
    }

    std::cout << "Successfully loaded " << nameToId.size() << " materials from " << filepath << ".\n";
}

void MaterialDB::loadReactions(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open reaction JSON: " << filepath << std::endl;
        return;
    }

    json j;
    try
    {
        file >> j;
    }
    catch (const json::parse_error &e)
    {
        std::cerr << "JSON Parse error (reactions): " << e.what() << std::endl;
        return;
    }

    if (!j.is_array())
    {
        std::cerr << "JSON error: Expected an array of reactions at root." << std::endl;
        return;
    }

    int count = 0;
    for (const auto &rxnObj : j)
    {
        ReactionDef def;
        std::string r1Str = rxnObj.value("reagent1", "");
        std::string r2Str = rxnObj.value("reagent2", "");
        std::string p1Str = rxnObj.value("product1", "Empty");
        std::string p2Str = rxnObj.value("product2", "Empty");

        def.reagent1 = getId(r1Str);
        def.reagent2 = getId(r2Str);
        def.product1 = getId(p1Str);
        def.product2 = getId(p2Str);
        def.probability = rxnObj.value("probability", 1.0f);
        def.probability = std::clamp(def.probability, 0.0f, 1.0f);
        def.minTemperature = std::clamp(rxnObj.value("minTemperature", -1.0f), -1.0f, 1.0f);
        def.maxTemperature = std::clamp(rxnObj.value("maxTemperature", 1.0f), -1.0f, 1.0f);
        def.minHumidity = std::clamp(rxnObj.value("minHumidity", 0.0f), 0.0f, 1.0f);
        def.maxHumidity = std::clamp(rxnObj.value("maxHumidity", 1.0f), 0.0f, 1.0f);
        def.heatDelta = std::clamp(rxnObj.value("heatDelta", 0.0f), -0.5f, 0.5f);

        if (def.reagent1 == 0 && r1Str != "Empty")
        {
            std::cerr << "Warning: Reagent1 '" << r1Str << "' not found in MaterialDB.\n";
        }
        if (def.reagent2 == 0 && r2Str != "Empty")
        {
            std::cerr << "Warning: Reagent2 '" << r2Str << "' not found in MaterialDB.\n";
        }

        if (def.minTemperature > def.maxTemperature || def.minHumidity > def.maxHumidity ||
            (def.reagent1 == 0 && r1Str != "Empty") ||
            (def.reagent2 == 0 && r2Str != "Empty") ||
            (def.product1 == 0 && p1Str != "Empty") ||
            (def.product2 == 0 && p2Str != "Empty"))
        {
            std::cerr << "Skipping invalid reaction: " << r1Str << " / " << r2Str << "\n";
            continue;
        }

        if (def.reagent1 >= 0 && def.reagent1 < MAX_MATERIALS && def.reagent2 >= 0 && def.reagent2 < MAX_MATERIALS)
        {
            reactionsArr[def.reagent1][def.reagent2] = def;
            hasReactionArr[def.reagent1][def.reagent2] = true;

            ReactionDef mirror = def;
            std::swap(mirror.reagent1, mirror.reagent2);
            std::swap(mirror.product1, mirror.product2);

            reactionsArr[mirror.reagent1][mirror.reagent2] = mirror;
            hasReactionArr[mirror.reagent1][mirror.reagent2] = true;
        }

        count++;
    }

    std::cout << "Successfully loaded " << count << " reactions from " << filepath << ".\n";
}

const MaterialDef &MaterialDB::getMaterial(int id) const
{
    if (id >= 0 && id < MAX_MATERIALS)
        return materialsArr[id];
    return fallbackMaterial;
}

const MaterialDef &MaterialDB::getMaterial(const std::string &name) const
{
    auto it = nameToId.find(name);
    if (it != nameToId.end())
        return getMaterial(it->second);
    return fallbackMaterial;
}

int MaterialDB::getId(const std::string &name) const
{
    auto it = nameToId.find(name);
    if (it != nameToId.end())
        return it->second;
    return 0;
}

size_t MaterialDB::getMaterialCount() const
{
    return nameToId.size();
}

const ReactionDef *MaterialDB::getReaction(int mat1, int mat2) const
{
    if (mat1 >= 0 && mat1 < MAX_MATERIALS && mat2 >= 0 && mat2 < MAX_MATERIALS)
    {
        if (hasReactionArr[mat1][mat2])
        {
            return &reactionsArr[mat1][mat2];
        }
    }
    return nullptr;
}

const std::vector<TransitionDef> &MaterialDB::getTransitions(int materialId) const
{
    static const std::vector<TransitionDef> empty;
    return materialId >= 0 && materialId < MAX_MATERIALS ? transitionsArr[materialId] : empty;
}
