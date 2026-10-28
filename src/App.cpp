#include "MaterialDB.h"
#include "World.h"
#include "Units.h"
#include "SimulationTuning.h"
#include "imgui-SFML.h"
#include "imgui.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace
{
  constexpr int INITIAL_WIDTH = 1280;
  constexpr int INITIAL_HEIGHT = 800;
  constexpr float PANEL_WIDTH = 310.0f;
  constexpr int INITIAL_WORLD_SIZE = 512;
  constexpr std::size_t HISTORY_LIMIT = 8;
  constexpr int FIRST_FORCE_TOOL = 9;
  constexpr int LAST_FORCE_TOOL = 16;
  bool fillCanvas = true;

  enum class GameScreen
  {
    MainMenu,
    Playing,
    PauseMenu
  };

  struct MaterialEntry
  {
    const char *key;
    const char *label;
    int category;
  };

  constexpr MaterialEntry MATERIALS[] = {
      {"Sand", "Areia", 0}, {"Soil", "Solo", 0}, {"Stone", "Pedra", 0}, {"Gravel", "Cascalho", 0}, {"Silt", "Silte", 0}, {"Salt", "Sal", 0}, {"Clay", "Argila", 0}, {"Mud", "Lama", 0}, {"Compost", "Composto", 0}, {"Sandstone", "Arenito", 0}, {"Limestone", "Calcario", 0}, {"Granite", "Granito", 0}, {"Shale", "Folhelho", 0}, {"IronOre", "Minerio de ferro", 0}, {"CopperOre", "Minerio de cobre", 0}, {"Coal", "Carvao mineral", 0}, {"Quartz", "Quartzo", 0}, {"FracturedRock", "Rocha fraturada", 0}, {"Peat", "Turfa", 0}, {"Seed", "Semente", 3}, {"Wood", "Madeira", 3}, {"Plant", "Planta", 3}, {"Moss", "Musgo", 3}, {"Grass", "Grama", 3}, {"Flower", "Flor", 3}, {"Algae", "Alga", 3}, {"Life", "Vida celular", 3}, {"PineSeed", "Semente de pinheiro", 3}, {"MangroveSeed", "Semente de mangue", 3}, {"ShrubSeed", "Semente de arbusto", 3}, {"PineNeedles", "Folhas de pinheiro", 3}, {"Shrub", "Arbusto", 3}, {"Mycelium", "Micelio", 3}, {"Mushroom", "Cogumelo", 3}, {"Ash", "Cinzas", 0}, {"Water", "Água", 1}, {"Saltwater", "Água salgada", 1}, {"Oil", "Óleo", 1}, {"Fuel", "Combustível", 1}, {"Resin", "Resina", 1}, {"Acid", "Ácido", 1}, {"Alkali", "Álcali", 1}, {"Lava", "Lava", 1}, {"Ice", "Gelo", 1}, {"Fire", "Fogo", 1}, {"Smoke", "Fumaça", 1}, {"Steam", "Vapor", 1}, {"ToxicGas", "Gás tóxico", 1}, {"SporeCloud", "Esporos", 1}, {"Charcoal", "Carvão", 1}, {"Wall", "Parede", 2}, {"Brick", "Tijolo", 2}, {"Metal", "Metal", 2}, {"Rust", "Ferrugem", 2}, {"Glass", "Vidro", 2}, {"Rubber", "Borracha", 2}, {"Fabric", "Tecido", 2}, {"Web", "Teia", 2}, {"Basalt", "Basalto", 0}, {"Obsidian", "Obsidiana", 0}, {"MoltenMetal", "Metal fundido", 1}, {"MoltenGlass", "Vidro fundido", 1}, {"Hydrogen", "Hidrogênio", 1}, {"CarbonDioxide", "Dióxido de carbono", 1}, {"Concrete", "Concreto", 2}, {"ReinforcedConcrete", "Concreto armado", 2}, {"Roots", "Raízes", 3}};

  const char *stateLabel(MatterState state)
  {
    switch (state)
    {
    case MatterState::Solid:
      return "Sólido";
    case MatterState::Granular:
      return "Granular";
    case MatterState::Liquid:
      return "Líquido";
    case MatterState::Gas:
      return "Gasoso";
    case MatterState::Plasma:
      return "Plasma";
    default:
      return "Especial";
    }
  }

  std::string lower(std::string value)
  {
    for (char &letter : value)
      letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    return value;
  }

  const char *materialLabel(int id)
  {
    const auto &db = MaterialDB::getInstance();
    for (const auto &item : MATERIALS)
      if (db.getId(item.key) == id)
        return item.label;
    return id == 0 ? "Vazio" : "Desconhecido";
  }

  void tooltip(const char *description)
  {
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", description);
  }

  void drawMaterialIcon(const MaterialDef &definition, int id, const ImVec2 start)
  {
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, ImVec2(start.x + 30, start.y + 30),
                        IM_COL32(26, 34, 43, 255));
    const auto base = definition.baseColor;
    for (int py = 0; py < 5; ++py)
      for (int px = 0; px < 5; ++px)
      {
        const unsigned hash = static_cast<unsigned>(id * 97 + px * 37 + py * 61 +
                                                    px * py * 19);
        if (hash % 9 == 0)
          continue;
        const int shade = static_cast<int>(hash % 5) * 10 - 18;
        const auto channel = [shade](sf::Uint8 value)
        {
          return static_cast<int>(std::clamp(static_cast<int>(value) + shade, 0, 255));
        };
        const ImU32 pixel = IM_COL32(channel(base.r), channel(base.g),
                                     channel(base.b), 255);
        draw->AddRectFilled(ImVec2(start.x + 5 + px * 4, start.y + 5 + py * 4),
                            ImVec2(start.x + 9 + px * 4, start.y + 9 + py * 4), pixel);
      }
  }

  void materialIcon(const MaterialDef &definition, int id)
  {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(30, 30));
    drawMaterialIcon(definition, id, start);
  }

  void toolIcon(int id)
  {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(25, 25));
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImU32 accent = id == 1 ? IM_COL32(255, 139, 78, 255) : id == 2           ? IM_COL32(108, 188, 255, 255)
                                                             : id == 3             ? IM_COL32(237, 118, 128, 255)
                                                             : id >= 9 && id <= 14 ? IM_COL32(250, 181, 90, 255)
                                                             : id >= 15            ? IM_COL32(171, 135, 242, 255)
                                                             : id >= 4 && id <= 6  ? IM_COL32(109, 219, 137, 255)
                                                                                   : IM_COL32(209, 219, 235, 255);
    static constexpr const char *patterns[][7] = {
        {".......", "......X", ".....XX", "....XXX", "...XX..", "..XX...", "XXX...."},
        {"...X...", ".X.X.X.", "..XXX..", "XXXXXXX", "..XXX..", ".X.X.X.", "...X..."},
        {"X..X..X", ".X.X.X.", "..XXX..", "XXXXXXX", "..XXX..", ".X.X.X.", "X..X..X"},
        {"XX...XX", ".XX.XX.", "..XXX..", "...X...", "..XXX..", ".XX.XX.", "XX...XX"},
        {".......", "...X...", "..XXX..", ".XXXXX.", "...X...", "..XXX..", "...X..."},
        {"...X...", "..XXX..", ".XXXXX.", "XXXXXXX", "..XXX..", "...X...", "..XXX.."},
        {"X......", ".XX..XX", "..XXXX.", "XX..XXX", ".XXXX..", "XX..XX.", "......X"},
        {".XXXX..", "XX...XX", "X.....X", "XX...XX", ".XXXX..", "....XX.", ".....XX"},
        {"..XXX..", ".XXXXX.", "XXXXXXX", "X.X.X.X", "..XXX..", ".X.X.X.", "X.....X"},
        {"...X...", "...XX..", "XXXXXXX", "...XX..", "...X...", "..X....", ".X....."},
        {"..XXX..", ".XX.XX.", "XX...XX", "X..X..X", "XX...XX", ".XX.XX.", "..XXX.."},
        {"...X...", "XXXXXXX", "XX.X.XX", "X..X..X", "...X...", "..XXX..", ".X.X.X."},
        {"X..X...", ".XX.X..", "..XXX..", "XXXXXXX", "..XXX..", ".XX.X..", "X..X..."},
        {"...X...", "..XXX..", ".X.X.X.", "X..X..X", ".X.X.X.", "..XXX..", "...X..."},
        {"X..X..X", ".X.X.X.", "..XXX..", "XXXXXXX", "..XXX..", ".X.X.X.", "X..X..X"},
        {"..XXX..", ".X...X.", "XX...XX", "XX...XX", "XXXXXXX", "XX...XX", "XXXXXXX"},
        {"..XXX..", ".X...X.", "X.....X", "XXXXXXX", "X.....X", "X.....X", "XXXXXXX"}};
    draw->AddRectFilled(pos, ImVec2(pos.x + 25, pos.y + 25),
                        IM_COL32(31, 42, 51, 255));
    const int pattern = std::clamp(id, 0, static_cast<int>(sizeof(patterns) / sizeof(patterns[0])) - 1);
    for (int py = 0; py < 7; ++py)
      for (int px = 0; px < 7; ++px)
        if (patterns[pattern][py][px] == 'X')
          draw->AddRectFilled(ImVec2(pos.x + 4 + px * 2.5f, pos.y + 4 + py * 2.5f),
                              ImVec2(pos.x + 6.5f + px * 2.5f,
                                     pos.y + 6.5f + py * 2.5f),
                              accent);
  }

  void pixelWindow()
  {
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, end, IM_COL32(14, 23, 34, 252));
    draw->AddRectFilled(ImVec2(pos.x + 2, pos.y + 2),
                        ImVec2(end.x - 2, end.y - 2), IM_COL32(31, 53, 67, 255));
    draw->AddRectFilled(ImVec2(pos.x + 5, pos.y + 5),
                        ImVec2(end.x - 5, end.y - 5), IM_COL32(17, 29, 42, 255));
    draw->AddRectFilled(ImVec2(pos.x + 8, pos.y + 5),
                        ImVec2(std::min(end.x - 8, pos.x + 52), pos.y + 8),
                        IM_COL32(117, 213, 173, 255));
    draw->AddRectFilled(ImVec2(end.x - 14, end.y - 9),
                        ImVec2(end.x - 9, end.y - 6), IM_COL32(88, 147, 166, 255));
  }

  bool pixelButton(const char *label, ImVec2 size = ImVec2(0.0f, 0.0f),
                   bool selected = false)
  {
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->ChannelsSplit(2);
    draw->ChannelsSetCurrent(1);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 outer = active ? IM_COL32(123, 165, 158, 255) : selected ? IM_COL32(97, 208, 146, 255)
                                                            : hovered    ? IM_COL32(110, 191, 206, 255)
                                                                         : IM_COL32(61, 108, 124, 255);
    const ImU32 fill = active ? IM_COL32(28, 70, 71, 255) : selected ? IM_COL32(32, 84, 65, 255)
                                                        : hovered    ? IM_COL32(37, 75, 91, 255)
                                                                     : IM_COL32(29, 48, 63, 255);
    draw->ChannelsSetCurrent(0);
    draw->AddRectFilled(ImVec2(min.x, min.y + 2),
                        ImVec2(max.x, max.y), IM_COL32(7, 15, 25, 255));
    draw->AddRectFilled(min, ImVec2(max.x, max.y - 2), outer);
    draw->AddRectFilled(ImVec2(min.x + 2, min.y + 2),
                        ImVec2(max.x - 2, max.y - 4), fill);
    draw->AddRectFilled(ImVec2(min.x + 5, min.y + 3),
                        ImVec2(max.x - 5, min.y + 5),
                        active ? IM_COL32(45, 113, 95, 255) : IM_COL32(98, 156, 159, 255));
    if (selected)
      draw->AddRectFilled(ImVec2(min.x + 4, max.y - 6),
                          ImVec2(max.x - 4, max.y - 4), IM_COL32(108, 236, 157, 255));
    draw->ChannelsMerge();
    return clicked;
  }

  void setupStyle()
  {
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 2.0f;
    style.ItemSpacing = ImVec2(7, 8);
    style.WindowPadding = ImVec2(10, 10);
    auto *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.09f, 0.13f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.24f, 0.28f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.39f, 0.42f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.43f, 0.33f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.16f, 0.21f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.13f, 0.37f, 0.30f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.42f, 0.88f, 0.65f, 1.0f);
  }

  void setupFont()
  {
    constexpr const char *paths[] = {
        "assets/fonts/PixelifySans.ttf",
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
    };
    bool loaded = false;
    for (const char *path : paths)
    {
      if (!std::filesystem::exists(path))
        continue;
      auto *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
          path, 17.0f, nullptr, ImGui::GetIO().Fonts->GetGlyphRangesDefault());
      if (font)
      {
        loaded = true;
        break;
      }
    }
    if (!loaded)
      ImGui::GetIO().Fonts->AddFontDefault();
    ImGui::SFML::UpdateFontTexture();
  }

  struct GameAudio
  {
    std::array<sf::SoundBuffer, 3> buffers;
    std::array<sf::Sound, 3> sounds;
    std::array<sf::Clock, 3> cooldowns;
    bool enabled = true;
    float volume = 26.0f;
    unsigned variation = 0;

    GameAudio()
    {
      constexpr unsigned sampleRate = 44100;
      constexpr float pi = 3.14159265358979323846f;
      for (int effect = 0; effect < 3; ++effect)
      {
        const float duration = effect == 2 ? 0.63f : effect == 1 ? 0.22f
                                                                 : 0.24f;
        const int count = static_cast<int>(duration * sampleRate);
        std::vector<sf::Int16> samples(static_cast<std::size_t>(count));
        std::uint32_t noise = 0x9e3779b9u + static_cast<std::uint32_t>(effect) * 0x7f4a7c15u;
        float filtered = 0.0f;
        float phase = 0.0f;
        for (int sample = 0; sample < count; ++sample)
        {
          const float time = static_cast<float>(sample) / sampleRate;
          const float progress = time / duration;
          noise ^= noise << 13;
          noise ^= noise >> 17;
          noise ^= noise << 5;
          const float hiss = (static_cast<float>(noise & 65535u) / 32767.5f) - 1.0f;
          filtered += (hiss - filtered) * (effect == 1 ? 0.075f : 0.026f);
          const float frequency = effect == 0 ? 160.0f * std::exp(-time * 7.0f) + 47.0f : effect == 1 ? 115.0f + 24.0f * std::sin(time * 18.0f)
                                                                                                      : 90.0f * std::exp(-time * 5.0f) + 29.0f;
          phase += 2.0f * pi * frequency / sampleRate;
          const float tone = std::sin(phase);
          const float attack = std::min(1.0f, time * (effect == 1 ? 160.0f : 360.0f));
          const float fade = std::min(1.0f, (duration - time) * 90.0f);
          const float decay = std::exp(-time * (effect == 2 ? 6.0f : effect == 1 ? 7.0f
                                                                                 : 17.0f));
          float mix = 0.0f;
          if (effect == 0)
            mix = tone * 0.67f + filtered * 0.43f + hiss * 0.055f;
          else if (effect == 1)
            mix = filtered * 0.66f + hiss * 0.09f + tone * 0.10f;
          else
            mix = tone * 0.56f + filtered * 0.66f + hiss * 0.075f * (1.0f - progress);
          samples[static_cast<std::size_t>(sample)] =
              static_cast<sf::Int16>(std::clamp(mix * attack * fade * decay * 17000.0f,
                                                -32760.0f, 32760.0f));
        }
        if (buffers[static_cast<std::size_t>(effect)].loadFromSamples(
                samples.data(), samples.size(), 1, sampleRate))
        {
          sounds[static_cast<std::size_t>(effect)].setBuffer(
              buffers[static_cast<std::size_t>(effect)]);
          sounds[static_cast<std::size_t>(effect)].setVolume(volume);
        }
      }
    }

    void play(int effect)
    {
      if (!enabled || effect < 0 || effect >= static_cast<int>(sounds.size()))
        return;
      const std::size_t index = static_cast<std::size_t>(effect);
      if (cooldowns[index].getElapsedTime().asSeconds() <
          (effect == 2 ? 0.50f : effect == 1 ? 0.18f
                                             : 0.12f))
        return;
      cooldowns[index].restart();
      sounds[index].setVolume(volume);
      const float pitch = 0.94f + static_cast<float>((variation++ * 17u) % 13u) * 0.01f;
      sounds[index].setPitch(pitch);
      sounds[index].play();
    }
  };

  struct History
  {
    std::deque<World::Snapshot> undo;
    std::deque<World::Snapshot> redo;

    static void add(std::deque<World::Snapshot> &stack, World::Snapshot snapshot)
    {
      const std::size_t limit = snapshot.grid.size() > 600000 ? 2 : snapshot.grid.size() > 300000 ? 4
                                                                                                  : HISTORY_LIMIT;
      while (stack.size() >= limit)
        stack.pop_front();
      stack.push_back(std::move(snapshot));
    }

    void checkpoint(const World &world)
    {
      add(undo, world.captureSnapshot());
      redo.clear();
    }

    bool goBack(World &world)
    {
      if (undo.empty())
        return false;
      add(redo, world.captureSnapshot());
      world.restoreSnapshot(undo.back());
      undo.pop_back();
      return true;
    }

    bool goForward(World &world)
    {
      if (redo.empty())
        return false;
      add(undo, world.captureSnapshot());
      world.restoreSnapshot(redo.back());
      redo.pop_back();
      return true;
    }
  };

  struct Preferences
  {
    std::vector<int> favorites;
    bool tips = true;
    bool showMaterials = false;
    bool showEnvironment = false;
    bool showTools = true;
    int graphicsQuality = 1;
    int temperatureUnit = 0;
    int pressureUnit = 0;
    int speedUnit = 0;
    SimulationTuning tuning;

    void load()
    {
      std::ifstream file("preferences.ini");
      if (!file)
        return;
      std::string setting;
      std::string rawValue;
      while (file >> setting >> rawValue)
      {
        char *remaining = nullptr;
        const float numeric = std::strtof(rawValue.c_str(), &remaining);
        if (remaining == rawValue.c_str() || *remaining != '\0' || !std::isfinite(numeric))
          continue;
        if (numeric > 100000.0f || numeric < -100000.0f)
          continue;
        const int value = static_cast<int>(numeric);
        if (setting == "dicas")
          tips = value != 0;
        else if (setting == "graficos")
          graphicsQuality = std::clamp(value, 0, 2);
        else if (setting == "materiais")
          showMaterials = value != 0;
        else if (setting == "ambiente")
          showEnvironment = value != 0;
        else if (setting == "ferramentas")
          showTools = value != 0;
        else if (setting == "temp_unidade")
          temperatureUnit = std::clamp(value, 0, 2);
        else if (setting == "pressao_unidade")
          pressureUnit = std::clamp(value, 0, 2);
        else if (setting == "velocidade_unidade")
          speedUnit = std::clamp(value, 0, 1);
        else if (setting == "tempo_simulacao")
          tuning.speed = numeric;
        else if (setting == "gravidade_simulacao")
          tuning.gravity = numeric;
        else if (setting == "frequencia_motor")
          tuning.physicsHz = value;
        else if (setting == "passos_maximos")
          tuning.maxUpdatesPerFrame = value;
        else if (setting == "orcamento_motor_ms")
          tuning.frameBudgetMs = numeric;
        else if (setting == "limite_fps")
          tuning.fpsLimit = value;
        else if (setting == "sincronizacao_vertical")
          tuning.verticalSync = value != 0;
        else if (setting == "halo_calor")
          tuning.heatGlow = value != 0;
        else if (setting == "intensidade_luz")
          tuning.lightStrength = numeric;
        else if (setting == "favorito" && value > 0 && value < 256 &&
                 MaterialDB::getInstance().getMaterial(value).id == value)
          favorites.push_back(value);
      }
      std::sort(favorites.begin(), favorites.end());
      favorites.erase(std::unique(favorites.begin(), favorites.end()),
                      favorites.end());
      tuning.sanitize();
    }

    void save() const
    {
      std::ofstream file("preferences.ini", std::ios::trunc);
      if (!file)
        return;
      file << "dicas " << static_cast<int>(tips) << '\n';
      file << "graficos " << graphicsQuality << '\n';
      file << "materiais " << showMaterials << '\n';
      file << "ambiente " << showEnvironment << '\n';
      file << "ferramentas " << showTools << '\n';
      file << "temp_unidade " << temperatureUnit << '\n';
      file << "pressao_unidade " << pressureUnit << '\n';
      file << "velocidade_unidade " << speedUnit << '\n';
      file << "tempo_simulacao " << tuning.speed << '\n';
      file << "gravidade_simulacao " << tuning.gravity << '\n';
      file << "frequencia_motor " << tuning.physicsHz << '\n';
      file << "passos_maximos " << tuning.maxUpdatesPerFrame << '\n';
      file << "orcamento_motor_ms " << tuning.frameBudgetMs << '\n';
      file << "limite_fps " << tuning.fpsLimit << '\n';
      file << "sincronizacao_vertical " << tuning.verticalSync << '\n';
      file << "halo_calor " << tuning.heatGlow << '\n';
      file << "intensidade_luz " << tuning.lightStrength << '\n';
      for (int id : favorites)
        file << "favorito " << id << '\n';
    }

    bool isFavorite(int id) const
    {
      return std::find(favorites.begin(), favorites.end(), id) != favorites.end();
    }

    void toggleFavorite(int id)
    {
      const auto found = std::find(favorites.begin(), favorites.end(), id);
      if (found == favorites.end())
        favorites.push_back(id);
      else
        favorites.erase(found);
      save();
    }
  };

  sf::Vector2f originFor(sf::Vector2u size, float panel, float zoom,
                         sf::Vector2f pan, const World &world)
  {
    const float area = std::max(1.0f, static_cast<float>(size.x) - panel);
    const float horizontal = area / world.getWidth();
    const float vertical = static_cast<float>(size.y) / world.getHeight();
    const float scale = std::max(0.05f, fillCanvas ? std::max(horizontal, vertical) : std::min(horizontal, vertical)) * zoom;
    const float verticalOrigin = fillCanvas
                                     ? static_cast<float>(size.y) * 0.72f - world.getGroundLevel() * scale
                                     : static_cast<float>(size.y) * 0.5f - world.getHeight() * scale * 0.5f;
    return sf::Vector2f(panel + area * 0.5f - world.getWidth() * scale * 0.5f + pan.x,
                        verticalOrigin + pan.y);
  }

  float scaleFor(sf::Vector2u size, float panel, float zoom, const World &world)
  {
    const float horizontal = std::max(1.0f, static_cast<float>(size.x) - panel) /
                             world.getWidth();
    const float vertical = static_cast<float>(size.y) / world.getHeight();
    return std::max(0.05f, fillCanvas ? std::max(horizontal, vertical) : std::min(horizontal, vertical)) * zoom;
  }

  sf::Vector2i worldAt(sf::Vector2i pointer, sf::Vector2u size,
                       float panel, float zoom, sf::Vector2f pan, const World &world)
  {
    const auto origin = originFor(size, panel, zoom, pan, world);
    const float scale = scaleFor(size, panel, zoom, world);
    return sf::Vector2i(static_cast<int>(std::floor((pointer.x - origin.x) / scale)),
                        static_cast<int>(std::floor((pointer.y - origin.y) / scale)));
  }

  bool overCanvas(sf::Vector2i pointer, sf::Vector2u size, float panel,
                  float zoom, sf::Vector2f pan, const World &world)
  {
    if (pointer.x < panel || pointer.x >= static_cast<int>(size.x) ||
        pointer.y < 0 || pointer.y >= static_cast<int>(size.y))
      return false;
    const auto cell = worldAt(pointer, size, panel, zoom, pan, world);
    return cell.x >= 0 && cell.y >= 0 && cell.x < world.getWidth() &&
           cell.y < world.getHeight();
  }

  void zoomAt(float delta, sf::Vector2i pointer, sf::Vector2u size,
              float panel, float &zoom, sf::Vector2f &pan, const World &world)
  {
    const float next = std::clamp(zoom * std::pow(1.14f, delta), 0.45f, 12.0f);
    if (next == zoom)
      return;
    const auto before = originFor(size, panel, zoom, pan, world);
    const float ratio = next / zoom;
    const sf::Vector2f desired(
        pointer.x - (pointer.x - before.x) * ratio,
        pointer.y - (pointer.y - before.y) * ratio);
    zoom = next;
    const auto after = originFor(size, panel, zoom, pan, world);
    pan.x += desired.x - after.x;
    pan.y += desired.y - after.y;
  }

  void paintStroke(World &world, sf::Vector2i start, sf::Vector2i end,
                   int radius, int material, int tool, float intensity, int seedSpecies)
  {
    const auto &db = MaterialDB::getInstance();
    const int seedId = db.getId("Seed"), grassId = db.getId("Grass");
    const int flowerId = db.getId("Flower"), plantId = db.getId("Plant");
    const int algaeId = db.getId("Algae"), waterId = db.getId("Water");
    const int saltwaterId = db.getId("Saltwater"), soilId = db.getId("Soil");
    const int mudId = db.getId("Mud"), compostId = db.getId("Compost");
    const int peatId = db.getId("Peat");
    const int chosenSeed = seedSpecies == 1 ? db.getId("PineSeed") : seedSpecies == 2 ? db.getId("MangroveSeed")
                                                                 : seedSpecies == 3   ? db.getId("ShrubSeed")
                                                                                      : seedId;
    const int left = std::max(0, std::min(start.x, end.x) - radius);
    const int right = std::min(world.getWidth() - 1,
                               std::max(start.x, end.x) + radius);
    const int top = std::max(0, std::min(start.y, end.y) - radius);
    const int bottom = std::min(world.getHeight() - 1,
                                std::max(start.y, end.y) + radius);
    const float dx = static_cast<float>(end.x - start.x);
    const float dy = static_cast<float>(end.y - start.y);
    const float lengthSquared = dx * dx + dy * dy;
    for (int y = top; y <= bottom; ++y)
    {
      for (int x = left; x <= right; ++x)
      {
        const float projection = lengthSquared > 0.0f
                                     ? std::clamp(((x - start.x) * dx + (y - start.y) * dy) /
                                                      lengthSquared,
                                                  0.0f, 1.0f)
                                     : 0.0f;
        const float distanceX = x - (start.x + projection * dx);
        const float distanceY = y - (start.y + projection * dy);
        if (distanceX * distanceX + distanceY * distanceY <= radius * radius)
        {
          if (tool == 0 || tool == 3)
            world.setParticle(x, y, tool == 3 ? 0 : material);
          else if (tool >= 4 && tool <= 6)
          {
            const unsigned scatter = static_cast<unsigned>(x * 73856093u) ^
                                     static_cast<unsigned>(y * 19349663u) ^
                                     static_cast<unsigned>(std::rand());
            if (scatter % 100 < (tool == 4 ? 9u : 27u))
            {
              const int existing = world.getParticle(x, y).matId;
              if (tool == 4 && existing == 0)
                world.setParticle(x, y, chosenSeed);
              else if (tool == 5 && existing == 0 && world.inBounds(x, y + 1))
              {
                const int ground = world.getParticle(x, y + 1).matId;
                if (ground == soilId || ground == mudId || ground == compostId ||
                    ground == peatId)
                  world.setParticle(x, y, scatter % 17 == 0 ? flowerId : scatter % 23 == 0 ? plantId
                                                                                           : grassId);
              }
              else if (tool == 6 && (existing == waterId || existing == saltwaterId ||
                                     (existing == 0 && world.inBounds(x, y + 1) &&
                                      (world.getParticle(x, y + 1).matId == waterId ||
                                       world.getParticle(x, y + 1).matId == saltwaterId))))
                world.setParticle(x, y, algaeId);
            }
          }
          else if (tool == 8)
          {
            const int existing = world.getParticle(x, y).matId;
            if ((existing == compostId || existing == peatId) && std::rand() % 100 < 17)
              world.setParticle(x, y, db.getId("Mycelium"));
            else if (existing == 0 && world.inBounds(x, y + 1) &&
                     world.getParticle(x, y + 1).matId == compostId &&
                     std::rand() % 100 < 12)
              world.setParticle(x, y, db.getId("Mushroom"));
          }
          else if (tool == 7)
          {
            continue;
          }
          else
            world.changeTemperature(x, y, (tool == 1 ? 1.0f : -1.0f) * intensity * (1.0f - 0.35f * std::sqrt(distanceX * distanceX + distanceY * distanceY) / std::max(1, radius)));
        }
      }
    }
  }

#ifdef _WIN32
  std::optional<std::filesystem::path> chooseFile(sf::RenderWindow &window, bool save)
  {
    std::array<wchar_t, 32768> filename{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window.getSystemHandle();
    dialog.lpstrFilter = L"Matterbloom (*.mbw)\0*.mbw\0Mundos legados (*.agw)\0*.agw\0Todos os arquivos (*.*)\0*.*\0";
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.lpstrDefExt = L"mbw";
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))
      return std::filesystem::path(filename.data());
    return std::nullopt;
  }
#endif
}

int main()
{
  std::srand(static_cast<unsigned>(std::time(nullptr)));
  sf::RenderWindow window(sf::VideoMode(INITIAL_WIDTH, INITIAL_HEIGHT),
                          "Matterbloom", sf::Style::Default);
  ImGui::SFML::Init(window);
  setupStyle();
  setupFont();
  GameAudio audio;

  auto &db = MaterialDB::getInstance();
  db.loadMaterials("materials.json");
  db.loadReactions("reactions.json");
  if (db.getMaterialCount() < 10)
  {
    ImGui::SFML::Shutdown();
    return 1;
  }

  World world(INITIAL_WORLD_SIZE, INITIAL_WORLD_SIZE);
  int generationSeed = static_cast<int>(std::time(nullptr) & 0x7fffffffu);
  int generationBiome = 0;
  int generationEnvironment = 0;
  int generationWeather = 0;
  int generationDimensions = 0;
  int initialExperiment = 0;
  bool randomSeed = true;
  world.createEcosystem(World::Biome::Temperate,
                        static_cast<std::uint32_t>(generationSeed));
  world.loadBackgrounds(std::filesystem::path("assets") / "backgrounds");
  world.rainEnabled = false;
  Preferences preferences;
  preferences.load();
  world.setGraphicsQuality(preferences.graphicsQuality);
  world.setGravityScale(preferences.tuning.gravity);
  world.setRenderTuning(preferences.tuning.heatGlow, preferences.tuning.lightStrength);
  window.setVerticalSyncEnabled(preferences.tuning.verticalSync);
  window.setFramerateLimit(preferences.tuning.verticalSync ? 0u : static_cast<unsigned>(preferences.tuning.fpsLimit));
  History history;
  std::filesystem::path activeFile;
  std::string status = "Mundo pronto";
  int brush = db.getId("Sand");
  int radius = 4;
  int tool = 0;
  int toolGroup = 0;
  bool dockMaterials = false;
  int materialPage = 0;
  int seedSpecies = 0;
  float thermalIntensity = 0.10f;
  float forceIntensity = 3.0f;
  float forceAngle = 35.0f;
  int view = 0;
  bool showSettings = false;
  int category = 0;
  char search[96] = {};
#ifndef _WIN32
  char pathInput[512] = "mundo.mbw";
#endif
  bool paused = true;
  GameScreen screen = GameScreen::MainMenu;
  int challenge = 0;
  bool challengeComplete = false;
  std::size_t challengeInitialPlants = 0;
  char slotName[64] = "Meu-mundo";
  std::vector<std::filesystem::path> saveSlots;
  int continuousToolFrames = 0;
  bool forceApplied = false;
  bool stepOnce = false;
  bool showPanel = true;
  bool showAdvancedPanel = false;
  bool stroking = false;
  bool panning = false;
  bool picking = false;
  bool aiming = false;
  bool aimFiring = false;
  bool continuousFire = true;
  bool placeEmitterMode = false;
  int aimContinuousFrames = 0;
  sf::Vector2i aimAnchorScreen(0, 0), aimCursorScreen(0, 0), aimAnchorWorld(0, 0);
  float zoom = 1.0f;
  sf::Vector2f pan(0, 0);
  sf::Vector2i previous(-1, -1);
  sf::Vector2i panPrevious(-1, -1);
  SimulationPacer pacer;
  float statsTimer = 0.0f;
  float fps = 60.0f;
  float simulationMs = 0.0f;
  float graphicsMs = 0.0f;
  float effectiveTicksPerSecond = 0.0f;
  WorldStats stats = world.getStats();
  sf::Clock clock;

  auto undo = [&]
  {
    if (history.goBack(world))
    {
      paused = true;
      pacer.reset();
      status = "Ação desfeita. Simulação pausada";
    }
  };
  auto redo = [&]
  {
    if (history.goForward(world))
    {
      paused = true;
      pacer.reset();
      status = "Ação refeita. Simulação pausada";
    }
  };
  auto refreshSaves = [&]
  {
    saveSlots.clear();
    std::error_code error;
    if (!std::filesystem::exists("saves", error))
      return;
    for (const auto &entry : std::filesystem::directory_iterator("saves", error))
    {
      if (error || saveSlots.size() >= 64)
        break;
      if (entry.is_regular_file(error) && entry.path().extension() == ".mbw")
        saveSlots.push_back(entry.path());
    }
    std::sort(saveSlots.begin(), saveSlots.end());
  };
  refreshSaves();
  auto saveFile = [&](bool forceDialog)
  {
#ifdef _WIN32
    const auto target = forceDialog || activeFile.empty()
                            ? chooseFile(window, true)
                            : std::optional(activeFile);
#else
    const std::optional<std::filesystem::path> target =
        forceDialog || activeFile.empty() ? std::filesystem::path(pathInput)
                                          : activeFile;
#endif
    if (target)
    {
      if (world.save(*target))
      {
        activeFile = *target;
        status = "Mundo salvo: " + activeFile.filename().u8string();
      }
      else
      {
        status = "Não foi possível salvar o mundo";
      }
    }
  };
  auto acceptLoadedWorld = [&](const std::filesystem::path &path)
  {
    history = History{};
    generationBiome = static_cast<int>(world.getBiome());
    generationEnvironment = world.getEnvironment() == World::Environment::Laboratory ? 1 : 0;
    generationSeed = static_cast<int>(world.getSeed() & 0x7fffffffu);
    activeFile = path;
    paused = true;
    screen = GameScreen::Playing;
    pacer.reset();
    challenge = 0;
    status = "Mundo carregado: " + activeFile.filename().u8string();
  };
  auto openFile = [&]
  {
#ifdef _WIN32
    const auto target = chooseFile(window, false);
#else
    const std::optional<std::filesystem::path> target = std::filesystem::path(pathInput);
#endif
    if (!target)
      return;
    if (world.load(*target))
    {
      acceptLoadedWorld(*target);
    }
    else
    {
      status = "Arquivo inválido ou incompatível";
    }
  };
  auto saveSlot = [&]
  {
    std::string safe;
    for (const unsigned char character : std::string(slotName))
    {
      if (std::isalnum(character) || character == '-' || character == '_')
        safe += static_cast<char>(character);
      else if (character == ' ')
        safe += '_';
    }
    if (safe.empty())
      safe = "mundo-" + std::to_string(world.getSeed());
    std::error_code error;
    std::filesystem::create_directories("saves", error);
    if (error)
    {
      status = "Nao foi possivel criar a pasta saves";
      return;
    }
    const auto path = std::filesystem::path("saves") / (safe + ".mbw");
    if (std::filesystem::exists(path, error))
    {
      const auto backup = std::filesystem::path("saves") / (safe + ".mbw.bak");
      std::filesystem::copy_file(path, backup,
                                 std::filesystem::copy_options::overwrite_existing, error);
      if (error)
      {
        status = "Nao foi possivel fazer backup do save anterior";
        return;
      }
    }
    if (world.save(path))
    {
      activeFile = path;
      status = "Salvo em " + path.u8string();
      refreshSaves();
    }
    else
    {
      status = "Erro ao salvar mundo";
    }
  };
  auto loadSlot = [&](const std::filesystem::path &path)
  {
    if (world.load(path))
      acceptLoadedWorld(path);
    else
      status = "Nao foi possivel abrir " + path.filename().u8string();
  };

  auto generateWorld = [&]
  {
    if (randomSeed)
    {
      const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
      generationSeed = static_cast<int>((static_cast<std::uint64_t>(tick) ^
                                         static_cast<std::uint64_t>(std::rand())) &
                                        0x7fffffffu);
      generationSeed = std::max(1, generationSeed);
    }
    history.checkpoint(world);
    const std::array<sf::Vector2i, 4> sizes = {
        sf::Vector2i(512, 512), sf::Vector2i(512, 768),
        sf::Vector2i(768, 768), sf::Vector2i(1024, 768)};
    const auto worldSize = sizes[static_cast<std::size_t>(generationDimensions)];
    if (worldSize.x != world.getWidth() || worldSize.y != world.getHeight())
      world.resize(worldSize.x, worldSize.y);
    if (generationEnvironment == 1)
      world.createLaboratory(static_cast<std::uint32_t>(generationSeed));
    else
      world.createEcosystem(static_cast<World::Biome>(generationBiome),
                            static_cast<std::uint32_t>(generationSeed));
    if (generationEnvironment == 0)
    {
      if (generationWeather == 0)
        world.cloudiness = std::min(world.cloudiness, 0.13f);
      else if (generationWeather == 1)
        world.cloudiness = std::max(world.cloudiness, 0.68f);
      else
      {
        world.cloudiness = std::max(world.cloudiness, 0.85f);
        world.rainEnabled = true;
      }
    }
    const int center = world.getWidth() / 2;
    const int ground = world.getGroundLevel();
    if (initialExperiment == 1)
    {
      const int wall = db.getId("ReinforcedConcrete");
      const int water = db.getId("Water");
      for (int dy = 0; dy < 82; ++dy)
      {
        const int y = ground - dy;
        if (y < 5)
          break;
        for (int x = center - 111; x < center - 104; ++x)
          world.setParticle(x, y, wall);
        for (int x = center; x < center + 8; ++x)
          world.setParticle(x, y, wall);
        if (dy < 65)
          for (int x = center - 103; x < center; ++x)
            if (world.getParticle(x, y).matId == 0)
              world.setParticle(x, y, water);
      }
    }
    else if (initialExperiment == 2)
    {
      const int brick = db.getId("Brick");
      const int stone = db.getId("Stone");
      for (int y = ground - 42; y < ground - 4; ++y)
        for (int x = center + 14; x < center + 22; ++x)
          world.setParticle(x, y, brick);
      world.setParticle(center - 100, ground - 25, stone);
      world.applyForce(center - 100, ground - 25, 3.0f, 8.0f,
                       World::ForceMode::Launch, 1.0f, -0.03f);
    }
    else if (initialExperiment == 3)
    {
      const int startY = generationEnvironment == 1 ? ground - 34 : ground + 9;
      const int endY = generationEnvironment == 1 ? ground - 9 : ground + 34;
      for (int y = startY; y < endY; ++y)
        for (int x = center - 18; x <= center + 18; ++x)
          if (world.inBounds(x, y) &&
              (x - center) * (x - center) +
                      (y - (startY + endY) / 2) * (y - (startY + endY) / 2) <
                  370)
            world.setParticle(x, y, db.getId("Lava"));
    }
    else if (initialExperiment == 4)
    {
      world.rainEnabled = true;
      world.cloudiness = 0.96f;
      world.wind = 1.55f;
    }
    activeFile.clear();
    paused = false;
    screen = GameScreen::Playing;
    challengeComplete = false;
    pacer.reset();
    zoom = 1.0f;
    pan = sf::Vector2f(0, 0);
    view = 0;
    world.renderMode = World::RenderMode::Normal;
    status = std::string(generationEnvironment == 1 ? "Laboratorio" : "Mundo natural") +
             " | seed " + std::to_string(generationSeed);
    stats = world.getStats();
    challengeInitialPlants = stats.plants;
  };

  auto fireAim = [&]
  {
    if (!aiming || tool < FIRST_FORCE_TOOL || tool > 14)
      return;
    const float dx = static_cast<float>(aimAnchorScreen.x - aimCursorScreen.x);
    const float dy = static_cast<float>(aimAnchorScreen.y - aimCursorScreen.y);
    const float length = std::sqrt(dx * dx + dy * dy);
    const float angle = forceAngle * 3.1415926536f / 180.0f;
    const float ux = length >= 5.0f ? dx / length : std::cos(angle);
    const float uy = length >= 5.0f ? dy / length : -std::sin(angle);
    const float drawnPower = std::clamp(length / 120.0f, 0.15f, 1.0f);
    const float power = std::clamp(forceIntensity * drawnPower, 0.1f, 12.0f);
    if (placeEmitterMode)
    {
      World::Emitter device;
      device.x = aimAnchorWorld.x;
      device.y = aimAnchorWorld.y;
      device.materialId = brush;
      device.mode = static_cast<World::ForceMode>(tool - FIRST_FORCE_TOOL);
      device.intensity = power;
      device.radius = static_cast<float>(std::min(35, radius));
      device.directionX = ux;
      device.directionY = uy;
      status = world.installEmitter(device) ? "Emissor instalado" : "Limite de emissores ou posicao ocupada";
      if (status == "Emissor instalado")
        audio.play(0);
      return;
    }
    int x = aimAnchorWorld.x, y = aimAnchorWorld.y;
    const float r = std::min(96.0f, radius * 1.9f + 2.0f);
    if (tool == 13 || tool == 14)
    {
      const float range = 24.0f + power * 8.0f;
      x = std::clamp(x + static_cast<int>(std::round(ux * range)), 1, world.getWidth() - 2);
      y = std::clamp(y + static_cast<int>(std::round(uy * range)), 1, world.getHeight() - 2);
    }
    if (tool == 9)
    {
      const int projectile = brush > 0 ? brush : db.getId("Stone");
      const int emitted = world.emitMaterial(x, y, projectile,
                                             std::clamp(radius * 2, 4, 24), power, ux, uy);
      if (!emitted)
        world.applyForce(x, y, r, power, World::ForceMode::Launch, ux, uy);
      status = "Projetil lancado | " + std::to_string(emitted) + " celulas";
      audio.play(0);
    }
    else
    {
      const auto mode = static_cast<World::ForceMode>(tool - FIRST_FORCE_TOOL);
      const int affected = world.applyForce(x, y, r, power, mode, ux, uy, brush, true);
      status = "Ferramenta de mira: " + std::to_string(affected) + " celulas";
      if (affected > 0 && (tool == 12 || tool == 14))
        audio.play(tool == 14 ? 2 : 1);
    }
    if (challenge == 3 && !challengeComplete)
    {
      challengeComplete = true;
      status = "Desafio concluido: laboratorio de fisica";
    }
  };

  while (window.isOpen())
  {
    const sf::Time frame = clock.restart();
    const float dt = std::min(frame.asSeconds(), 0.10f);
    fps = fps * 0.94f + (dt > 0 ? 1.0f / dt : fps) * 0.06f;
    const float panel = 0.0f;
    sf::Event event;
    while (window.pollEvent(event))
    {
      ImGui::SFML::ProcessEvent(window, event);
      if (event.type == sf::Event::Closed)
        window.close();
      if (event.type == sf::Event::Resized)
      {
        window.setView(sf::View(sf::FloatRect(0, 0,
                                              static_cast<float>(event.size.width),
                                              static_cast<float>(event.size.height))));
      }
      if (event.type == sf::Event::KeyPressed && !ImGui::GetIO().WantCaptureKeyboard)
      {
        if (event.key.code == sf::Keyboard::Escape)
        {
          if (aiming)
          {
            aiming = false;
            aimFiring = false;
          }
          else if (screen == GameScreen::Playing)
          {
            screen = GameScreen::PauseMenu;
            paused = true;
            refreshSaves();
          }
          else if (screen == GameScreen::PauseMenu)
          {
            screen = GameScreen::Playing;
          }
          stroking = false;
          panning = false;
        }
        else if (screen == GameScreen::Playing)
        {
          const bool control = event.key.control;
          if (control && event.key.code == sf::Keyboard::S)
            saveFile(false);
          else if (control && event.key.code == sf::Keyboard::O)
            openFile();
          else if (control && event.key.code == sf::Keyboard::Z)
            event.key.shift ? redo() : undo();
          else if (control && event.key.code == sf::Keyboard::Y)
            redo();
          else if (event.key.code == sf::Keyboard::Space)
            paused = !paused;
          else if (event.key.code == sf::Keyboard::LShift ||
                   event.key.code == sf::Keyboard::RShift)
          {
            continuousFire = !continuousFire;
            status = continuousFire ? "Disparo continuo ativo" : "Disparo unico ativo";
          }
          else if (event.key.code == sf::Keyboard::N && paused)
            stepOnce = true;
          else if (event.key.code == sf::Keyboard::B)
            tool = 0;
          else if (event.key.code == sf::Keyboard::H)
            tool = 1;
          else if (event.key.code == sf::Keyboard::C)
            tool = 2;
          else if (event.key.code == sf::Keyboard::E)
            tool = 3;
          else if (event.key.code == sf::Keyboard::I)
            tool = 7;
          else if (event.key.code == sf::Keyboard::G)
            generateWorld();
          else if (event.key.code == sf::Keyboard::Tab)
            showPanel = !showPanel;
          else if (event.key.code == sf::Keyboard::F2)
            showAdvancedPanel = !showAdvancedPanel;
          else if (event.key.code == sf::Keyboard::Home)
          {
            zoom = 1.0f;
            pan = sf::Vector2f(0, 0);
          }
          else if (event.key.code == sf::Keyboard::LBracket)
            radius = std::max(1, radius - 1);
          else if (event.key.code == sf::Keyboard::RBracket)
            radius = std::min(35, radius + 1);
          else if (event.key.code == sf::Keyboard::F1)
          {
            preferences.tips = !preferences.tips;
            preferences.save();
          }
          else if (event.key.code == sf::Keyboard::F5)
            saveSlot();
          else if (event.key.code == sf::Keyboard::F9)
          {
            refreshSaves();
            if (!activeFile.empty())
              loadSlot(activeFile);
            else if (!saveSlots.empty())
              loadSlot(saveSlots.back());
          }
        }
      }
      if (event.type == sf::Event::MouseWheelScrolled &&
          screen == GameScreen::Playing &&
          !ImGui::GetIO().WantCaptureMouse)
      {
        const sf::Vector2i pointer(event.mouseWheelScroll.x, event.mouseWheelScroll.y);
        if (overCanvas(pointer, window.getSize(), panel, zoom, pan, world))
        {
          if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) ||
              sf::Keyboard::isKeyPressed(sf::Keyboard::RControl))
            zoomAt(event.mouseWheelScroll.delta, pointer, window.getSize(), panel, zoom, pan, world);
          else if (aiming && tool >= FIRST_FORCE_TOOL && tool <= 14)
            forceIntensity = std::clamp(forceIntensity + event.mouseWheelScroll.delta * 0.35f,
                                        0.2f, 12.0f);
          else
            radius = std::clamp(radius +
                                    static_cast<int>(std::round(event.mouseWheelScroll.delta)),
                                1, 35);
        }
      }
      if (event.type == sf::Event::MouseButtonPressed)
      {
        const sf::Vector2i pointer(event.mouseButton.x, event.mouseButton.y);
        if (screen == GameScreen::Playing && !ImGui::GetIO().WantCaptureMouse &&
            overCanvas(pointer, window.getSize(), panel, zoom, pan, world))
        {
          if (aiming && event.mouseButton.button == sf::Mouse::Left)
          {
            history.checkpoint(world);
            aimFiring = true;
            aimContinuousFrames = 0;
            fireAim();
          }
          else if (event.mouseButton.button == sf::Mouse::Right &&
                   tool >= FIRST_FORCE_TOOL && tool <= 14)
          {
            aiming = true;
            aimFiring = false;
            aimAnchorScreen = aimCursorScreen = pointer;
            aimAnchorWorld = worldAt(pointer, window.getSize(), panel, zoom, pan, world);
            stroking = false;
          }
          else if (event.mouseButton.button == sf::Mouse::Middle)
          {
            panning = true;
            panPrevious = pointer;
          }
          else if (event.mouseButton.button == sf::Mouse::Left &&
                   (sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt)))
          {
            const auto cell = worldAt(pointer, window.getSize(), panel, zoom, pan, world);
            brush = world.getParticle(cell.x, cell.y).matId;
            tool = 0;
            status = std::string("Selecionado: ") + materialLabel(brush);
            picking = true;
          }
          else if (tool != 7 && (event.mouseButton.button == sf::Mouse::Left ||
                                 event.mouseButton.button == sf::Mouse::Right))
          {
            history.checkpoint(world);
            stroking = true;
            forceApplied = false;
            continuousToolFrames = 0;
            previous = sf::Vector2i(-1, -1);
          }
        }
      }
      if (event.type == sf::Event::MouseMoved && aiming)
        aimCursorScreen = sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
      if (event.type == sf::Event::MouseMoved && panning)
      {
        const sf::Vector2i current(event.mouseMove.x, event.mouseMove.y);
        pan.x += static_cast<float>(current.x - panPrevious.x);
        pan.y += static_cast<float>(current.y - panPrevious.y);
        panPrevious = current;
      }
      if (event.type == sf::Event::MouseButtonReleased)
      {
        if (event.mouseButton.button == sf::Mouse::Middle)
          panning = false;
        if (event.mouseButton.button == sf::Mouse::Right)
        {
          aiming = false;
          aimFiring = false;
        }
        if (event.mouseButton.button == sf::Mouse::Left && aiming)
          aimFiring = false;
        if (event.mouseButton.button == sf::Mouse::Left ||
            event.mouseButton.button == sf::Mouse::Right)
        {
          stroking = false;
          forceApplied = false;
          picking = false;
          previous = sf::Vector2i(-1, -1);
        }
      }
      if (event.type == sf::Event::LostFocus)
      {
        stroking = false;
        picking = false;
        panning = false;
        aiming = false;
        aimFiring = false;
      }
    }
    ImGui::SFML::Update(window, frame);

    const sf::Vector2u size = window.getSize();
    if (showPanel && screen == GameScreen::Playing)
    {
      if (preferences.showMaterials)
      {
        const float paletteWidth = 253.0f;
        const float paletteHeight = std::max(180.0f, std::min(490.0f, size.y - 215.0f));
        ImGui::SetNextWindowPos(ImVec2(14, 67), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(paletteWidth, paletteHeight), ImGuiCond_Always);
        ImGui::Begin("Materiais##hud", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        pixelWindow();
        ImGui::TextColored(ImVec4(0.59f, 0.95f, 0.76f, 1), "MATERIAIS");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", materialLabel(brush));
        ImGui::InputTextWithHint("##filtrar-material", "Procurar material...", search,
                                 sizeof(search));
        const char *groups[] = {"Todos", "Terreno", "Fluidos", "Estruturas", "Vegetacao"};
        ImGui::Combo("##grupo-material", &category, groups, IM_ARRAYSIZE(groups));
        ImGui::Separator();
        ImGui::BeginChild("cartas-materiais", ImVec2(0, paletteHeight - 119.0f), false);
        const std::string filter = lower(search);
        const auto matches = [&](const MaterialEntry &entry)
        {
          const int id = db.getId(entry.key);
          return id > 0 && (category == 0 || entry.category == category - 1) &&
                 (filter.empty() || lower(entry.key).find(filter) != std::string::npos ||
                  lower(entry.label).find(filter) != std::string::npos);
        };
        const auto card = [&](const MaterialEntry &entry)
        {
          const int id = db.getId(entry.key);
          ImGui::PushID(id);
          materialIcon(db.getMaterial(id), id);
          ImGui::SameLine();
          if (ImGui::Selectable(entry.label, brush == id))
          {
            brush = id;
            if (tool != 9 && tool != 12)
              tool = 0;
          }
          ImGui::SameLine();
          if (ImGui::SmallButton(preferences.isFavorite(id) ? "*" : "+"))
            preferences.toggleFavorite(id);
          tooltip("Adicionar ou remover dos favoritos");
          ImGui::PopID();
        };
        std::vector<const MaterialEntry *> visibleMaterials;
        for (int favoritesFirst = 1; favoritesFirst >= 0; --favoritesFirst)
          for (const auto &entry : MATERIALS)
            if (matches(entry) &&
                preferences.isFavorite(db.getId(entry.key)) == (favoritesFirst != 0))
              visibleMaterials.push_back(&entry);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visibleMaterials.size()), 38.0f);
        while (clipper.Step())
          for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
            card(*visibleMaterials[static_cast<std::size_t>(index)]);
        ImGui::EndChild();
        ImGui::End();
      }

      const float topWidth = std::min(1050.0f, size.x * 0.96f);
      ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f - topWidth * 0.5f, 10),
                              ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(topWidth, 52), ImGuiCond_Always);
      ImGui::Begin("HUD##top", nullptr,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
      pixelWindow();
      ImGui::TextColored(ImVec4(0.63f, 0.95f, 0.75f, 1), "MATTERBLOOM");
      ImGui::SameLine();
      ImGui::TextDisabled("%s | %s | %s | %.0f FPS",
                          world.getEnvironment() == World::Environment::Laboratory ? "Laboratorio" : world.getBiome() == World::Biome::Arid  ? "Deserto"
                                                                                                 : world.getBiome() == World::Biome::Wetland ? "Pantano"
                                                                                                                                             : "Temperado",
                          world.rainEnabled || world.cloudiness > 0.75f ? "Chuva" : world.cloudiness > 0.4f ? "Nublado"
                                                                                                            : "Ceu aberto",
                          Units::clock24(world.getClockHour()).c_str(), fps);
      ImGui::SameLine();
      if (ImGui::SmallButton(paused ? "Jogar" : "Pausar"))
        paused = !paused;
      ImGui::SameLine();
      if (ImGui::SmallButton("Menu"))
      {
        paused = true;
        screen = GameScreen::PauseMenu;
        refreshSaves();
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Ajustes"))
        showSettings = !showSettings;
      ImGui::SameLine();
      if (ImGui::SmallButton(preferences.showMaterials ? "- Materiais" : "+ Materiais"))
      {
        preferences.showMaterials = !preferences.showMaterials;
        preferences.save();
      }
      ImGui::SameLine();
      if (ImGui::SmallButton(preferences.showEnvironment ? "- Mundo" : "+ Mundo"))
      {
        preferences.showEnvironment = !preferences.showEnvironment;
        preferences.save();
      }
      ImGui::SameLine();
      if (ImGui::SmallButton(preferences.showTools ? "- Ferramentas" : "+ Ferramentas"))
      {
        preferences.showTools = !preferences.showTools;
        preferences.save();
      }
      ImGui::End();

      if (preferences.showEnvironment)
      {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(size.x) - 293, 67),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(279, std::max(260.0f,
                                                      std::min(510.0f, size.y - 210.0f))),
                                 ImGuiCond_Always);
        ImGui::Begin("Mundo##hud", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        pixelWindow();
        ImGui::TextColored(ImVec4(0.59f, 0.85f, 1.0f, 1), "MUNDO & AMBIENTE");
        ImGui::Separator();
        const char *environmentTypes[] = {"Mundo natural", "Laboratorio"};
        ImGui::Combo("Tipo de partida", &generationEnvironment,
                     environmentTypes, IM_ARRAYSIZE(environmentTypes));
        const char *biomes[] = {"Temperado", "Deserto", "Pantano"};
        if (generationEnvironment == 0)
          ImGui::Combo("Bioma", &generationBiome, biomes, IM_ARRAYSIZE(biomes));
        const char *worldSizes[] = {"512 x 512", "512 x 768 | Profundo",
                                    "768 x 768 | Amplo", "1024 x 768 | Extenso"};
        ImGui::Combo("Tamanho", &generationDimensions, worldSizes,
                     IM_ARRAYSIZE(worldSizes));
        const char *weathers[] = {"Ceu limpo", "Nublado", "Chuva"};
        if (generationEnvironment == 0)
          ImGui::Combo("Clima inicial", &generationWeather, weathers,
                       IM_ARRAYSIZE(weathers));
        const char *experimentsNatural[] = {"Sem experimento", "Barragem", "Teste de impacto",
                                            "Camara vulcanica", "Tempestade"};
        const char *experimentsLaboratory[] = {"Vazio", "Tanque de pressao", "Teste balistico",
                                               "Camara termica", "Teste de vento"};
        ImGui::Combo("Experimento", &initialExperiment,
                     generationEnvironment == 1 ? experimentsLaboratory : experimentsNatural, 5);
        ImGui::Checkbox("Seed aleatoria", &randomSeed);
        if (!randomSeed)
          ImGui::InputInt("Seed", &generationSeed);
        if (pixelButton("GERAR NOVO MUNDO", ImVec2(248, 34)))
          generateWorld();
        tooltip("Gera outro terreno, vegetacao, ceu e clima conforme as opcoes");
        ImGui::Separator();
        ImGui::Checkbox("Clima autonomo", &world.automaticWeather);
        ImGui::Checkbox("Chuva forcada", &world.rainEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Ciclo dia/noite", &world.dayNightEnabled);
        if (!world.dayNightEnabled)
        {
          float hour = world.getClockHour();
          if (ImGui::SliderFloat("Hora do mundo", &hour, 0.0f, 24.0f, "%.1f h"))
            world.setClockHour(hour);
        }
        float dayMinutes = world.dayLengthSeconds / 60.0f;
        if (ImGui::SliderFloat("Dia (min reais)", &dayMinutes, 0.5f, 12.0f, "%.1f"))
          world.dayLengthSeconds = dayMinutes * 60.0f;
        float cloudsPercent = world.cloudiness * 100.0f;
        if (ImGui::SliderFloat("Nuvens (%)", &cloudsPercent, 0.0f, 100.0f, "%.0f"))
          world.cloudiness = cloudsPercent / 100.0f;
        const auto windUnit = static_cast<Units::Speed>(preferences.speedUnit);
        float displayedWind = Units::windSpeed(world.wind, windUnit);
        if (ImGui::SliderFloat("Vento", &displayedWind,
                               Units::windSpeed(-2.0f, windUnit),
                               Units::windSpeed(2.0f, windUnit), "%.1f"))
          world.wind = displayedWind / (windUnit == Units::Speed::KilometersPerHour ? 18.0f : 5.0f);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", Units::speedName(windUnit));
        auto thermalUnit = static_cast<Units::Temperature>(preferences.temperatureUnit);
        float thermalDegrees = Units::thermalDelta(world.climateOffset, thermalUnit);
        if (ImGui::SliderFloat("Clima (delta)", &thermalDegrees,
                               Units::thermalDelta(-0.60f, thermalUnit),
                               Units::thermalDelta(0.60f, thermalUnit), "%.1f"))
          world.climateOffset = Units::fromThermalDelta(thermalDegrees, thermalUnit);
        ImGui::TextDisabled("Variação climatica em %s", Units::temperatureName(thermalUnit));
        const char *temperatureUnits[] = {"Celsius", "Fahrenheit", "Kelvin"};
        const char *pressureUnits[] = {"kPa", "bar", "psi"};
        const char *speedUnits[] = {"m/s", "km/h"};
        if (ImGui::Combo("Temperatura", &preferences.temperatureUnit,
                         temperatureUnits, IM_ARRAYSIZE(temperatureUnits)))
          preferences.save();
        if (ImGui::Combo("Pressao", &preferences.pressureUnit,
                         pressureUnits, IM_ARRAYSIZE(pressureUnits)))
          preferences.save();
        if (ImGui::Combo("Velocidade", &preferences.speedUnit,
                         speedUnits, IM_ARRAYSIZE(speedUnits)))
          preferences.save();
        const char *views[] = {"Normal", "Temperatura", "Umidade", "Nutrientes",
                               "Saturacao do solo", "Pressao", "Tensoes", "Velocidade"};
        if (ImGui::Combo("Visualizar", &view, views, IM_ARRAYSIZE(views)))
          world.renderMode = static_cast<World::RenderMode>(view);
        const char *qualities[] = {"Leve", "Equilibrado", "Detalhado"};
        if (ImGui::Combo("Graficos", &preferences.graphicsQuality,
                         qualities, IM_ARRAYSIZE(qualities)))
        {
          world.setGraphicsQuality(preferences.graphicsQuality);
          preferences.save();
        }
        ImGui::Checkbox("Preencher tela", &fillCanvas);
        if (pixelButton("Salvar mundo", ImVec2(120, 29)))
          saveFile(false);
        ImGui::SameLine();
        if (pixelButton("Abrir mundo", ImVec2(120, 29)))
          openFile();
#ifndef _WIN32
        ImGui::InputText("Arquivo", pathInput, sizeof(pathInput));
#endif
        if (pixelButton("Desfazer", ImVec2(120, 27)))
          undo();
        ImGui::SameLine();
        if (pixelButton("Refazer", ImVec2(120, 27)))
          redo();
        ImGui::TextDisabled("Seed %u | %d x %d", world.getSeed(),
                            world.getWidth(), world.getHeight());
        ImGui::TextDisabled("Solo %.0f%% | Nuvens %.0f%% | Chuva %.1f mm/h",
                            stats.soilMoisture * 100, stats.cloudCover * 100,
                            Units::rainfallMmPerHour(stats.rainIntensity));
        ImGui::TextDisabled("Tempestade %.0f%% | Particulas em movimento: %zu",
                            stats.stormIntensity * 100, stats.movingParticles);
        if (stats.tornadoActive)
          ImGui::TextColored(ImVec4(1, 0.7f, 0.45f, 1), "ALERTA: TORNADO ATIVO");
        ImGui::Separator();
        constexpr const char *challenges[] = {"Sandbox livre", "Reflorestar +40",
                                              "Restaurar umidade", "Testar fisica"};
        if (ImGui::Combo("Desafio", &challenge, challenges, IM_ARRAYSIZE(challenges)))
        {
          challengeComplete = false;
          challengeInitialPlants = stats.plants;
        }
        if (challenge)
          ImGui::TextColored(ImVec4(0.66f, 0.93f, 0.71f, 1.0f), "%s",
                             challengeComplete ? "DESAFIO CONCLUIDO!" : "DESAFIO EM ANDAMENTO");
        if (ImGui::CollapsingHeader("Eventos do mundo"))
          for (const auto &entry : world.getEvents())
            ImGui::TextWrapped("%s", entry.c_str());
        ImGui::TextDisabled("%s", status.c_str());
        ImGui::End();
      }

      if (preferences.showTools)
      {
        constexpr const char *toolNames[] = {"Pintar", "Calor", "Frio", "Apagar",
                                             "Sementes", "Jardim", "Algas", "Inspecao", "Fungos",
                                             "Lancador", "Impulso", "Ventilador", "Jato", "Atracao",
                                             "Explosao", "Fixar", "Soltar"};
        constexpr const char *toolGroups[] = {"Natureza", "Fisica e forcas", "Estruturas"};
        const bool compactDock = size.x < 930;
        const float dockWidth = compactDock ? 490.0f : 980.0f;
        const int toolsPerRow = compactDock ? 4 : 7;
        const float dockHeight = compactDock ? 239.0f : 209.0f;
        ImGui::SetNextWindowPos(ImVec2(std::max(8.0f, size.x * 0.5f - dockWidth * 0.5f),
                                       std::max(65.0f, static_cast<float>(size.y) -
                                                           dockHeight - 10.0f)),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(std::min(dockWidth, size.x - 16.0f),
                                        dockHeight),
                                 ImGuiCond_Always);
        ImGui::Begin("Ferramentas##dock", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        pixelWindow();
        if (ImGui::SmallButton(dockMaterials ? "FERRAMENTAS" : "> FERRAMENTAS"))
          dockMaterials = false;
        ImGui::SameLine();
        if (ImGui::SmallButton(dockMaterials ? "> MATERIAIS" : "MATERIAIS"))
          dockMaterials = true;
        ImGui::SameLine();
        ImGui::TextDisabled("%s", dockMaterials ? materialLabel(brush) : toolNames[tool]);
        if (dockMaterials)
        {
          const char *categories[] = {"Todos", "Terreno", "Fluidos", "Construcoes", "Vida"};
          ImGui::Combo("##tipo-material-dock", &category, categories, IM_ARRAYSIZE(categories));
          std::vector<const MaterialEntry *> filtered;
          const std::string searchText = lower(search);
          for (const auto &entry : MATERIALS)
            if ((category == 0 || entry.category == category - 1) &&
                (searchText.empty() || lower(entry.label).find(searchText) != std::string::npos ||
                 lower(entry.key).find(searchText) != std::string::npos) &&
                db.getId(entry.key) > 0)
              filtered.push_back(&entry);
          const int iconsPerPage = compactDock ? 12 : 20;
          const int pages = std::max(1, static_cast<int>((filtered.size() + iconsPerPage - 1) / iconsPerPage));
          materialPage = std::clamp(materialPage, 0, pages - 1);
          if (ImGui::SmallButton("< ANTERIOR"))
            materialPage = std::max(0, materialPage - 1);
          ImGui::SameLine();
          ImGui::Text("%d/%d", materialPage + 1, pages);
          ImGui::SameLine();
          if (ImGui::SmallButton("PROXIMA >"))
            materialPage = std::min(pages - 1, materialPage + 1);
          ImGui::SameLine();
          ImGui::TextDisabled("Clique no material para equipar");
          const int first = materialPage * iconsPerPage;
          const int slotsPerRow = compactDock ? 6 : 10;
          for (int index = first; index < std::min(first + iconsPerPage, static_cast<int>(filtered.size())); ++index)
          {
            const auto *entry = filtered[static_cast<std::size_t>(index)];
            const int id = db.getId(entry->key);
            if ((index - first) % slotsPerRow)
              ImGui::SameLine();
            ImGui::PushID(id + 3000);
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const bool clicked = pixelButton("##icone-material", ImVec2(43.0f, 43.0f), brush == id);
            drawMaterialIcon(db.getMaterial(id), id, ImVec2(at.x + 6.5f, at.y + 6.5f));
            if (clicked)
            {
              brush = id;
              if (tool != 9 && tool != 12)
                tool = 0;
            }
            if (ImGui::IsItemHovered())
              ImGui::SetTooltip("%s", entry->label);
            ImGui::PopID();
          }
          ImGui::SliderInt("Raio", &radius, 1, 35);
        }
        else
        {
          ImGui::Combo("##familia-ferramenta", &toolGroup, toolGroups,
                       IM_ARRAYSIZE(toolGroups));
          int visibleCount = 0;
          for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(toolNames)); ++i)
          {
            if ((toolGroup == 0 && i >= 9) ||
                (toolGroup == 1 && (i < 9 || i > 14)) ||
                (toolGroup == 2 && i < 15))
              continue;
            if (visibleCount && visibleCount % toolsPerRow != 0)
              ImGui::SameLine();
            ImGui::PushID(i + 1000);
            toolIcon(i);
            ImGui::SameLine();
            const bool selected = tool == i;
            if (pixelButton(toolNames[i], ImVec2(70, 29), selected))
              tool = i;
            tooltip(i == 9 ? "Lancar o material equipado com mira e potencia" : i == 10 ? "Impulso radial por clique ou direcional com a mira"
                                                                            : i == 11   ? "Soprar vento e deslocar materiais"
                                                                            : i == 12   ? "Projetar agua ou o liquido selecionado"
                                                                            : i == 13   ? "Atrair em um ponto apontado pela mira"
                                                                            : i == 14   ? "Detonar explosao no ponto indicado pela mira"
                                                                            : i == 15   ? "Ancorar paredes e estruturas"
                                                                            : i == 16   ? "Soltar estruturas ancoradas"
                                                                            : i == 4    ? "Dispersar sementes da especie selecionada"
                                                                            : i == 5    ? "Cultivar grama, flores e folhas sobre solo"
                                                                            : i == 6    ? "Cultivar algas na agua"
                                                                            : i == 7    ? "Inspecionar a celula diretamente no mundo"
                                                                            : i == 8    ? "Colonizar solo organico com micelio e cogumelos"
                                                                                        : "Selecionar ferramenta");
            ImGui::PopID();
            ++visibleCount;
          }
          ImGui::SliderInt("Raio", &radius, 1, 35);
          if (tool == 4)
          {
            const char *species[] = {"Arvore comum", "Pinheiro", "Mangue", "Arbusto"};
            ImGui::Combo("Especie", &seedSpecies, species, IM_ARRAYSIZE(species));
          }
          if (tool == 1 || tool == 2)
          {
            const auto unit = static_cast<Units::Temperature>(preferences.temperatureUnit);
            float intensityDegrees = Units::thermalDelta(thermalIntensity, unit);
            if (ImGui::SliderFloat("Calor / frio", &intensityDegrees,
                                   Units::thermalDelta(0.02f, unit),
                                   Units::thermalDelta(0.50f, unit), "%.1f"))
              thermalIntensity = Units::fromThermalDelta(intensityDegrees, unit);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Units::temperatureName(unit));
          }
          if (tool >= FIRST_FORCE_TOOL)
          {
            ImGui::SliderFloat("Forca", &forceIntensity, 0.2f, 12.0f, "%.1f");
            if (tool <= 14)
            {
              ImGui::Checkbox("Disparo continuo", &continuousFire);
              ImGui::Checkbox("Instalar emissor", &placeEmitterMode);
              ImGui::TextDisabled("Emissores ativos: %zu", world.getEmitterCount());
              if (world.getEmitterCount() && ImGui::SmallButton("Remover emissores"))
              {
                history.checkpoint(world);
                world.clearEmitters();
              }
              ImGui::TextDisabled("Direito: mira | Esquerdo: dispara | Roda: forca");
            }
          }
        }
        ImGui::End();
      }
    }

    if (showAdvancedPanel && showPanel && screen == GameScreen::Playing)
    {
      ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(PANEL_WIDTH - 16,
                                      std::max(120.0f, size.y - 16.0f)),
                               ImGuiCond_Always);
      ImGui::Begin("Mundo vivo", nullptr,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
      pixelWindow();
      ImGui::TextColored(ImVec4(0.43f, 0.92f, 0.68f, 1), "MUNDO VIVO");
      ImGui::TextDisabled("Materia em movimento");
      ImGui::Separator();
      const char *tools[] = {"Pincel de materiais", "Aquecer", "Resfriar"};
      ImGui::Combo("Ferramenta", &tool, tools, IM_ARRAYSIZE(tools));
      if (tool == 0)
        ImGui::Text("Material: %s", materialLabel(brush));
      else
      {
        const auto unit = static_cast<Units::Temperature>(preferences.temperatureUnit);
        float temperatureDelta = Units::thermalDelta(thermalIntensity, unit);
        if (ImGui::SliderFloat("Intensidade", &temperatureDelta,
                               Units::thermalDelta(0.02f, unit),
                               Units::thermalDelta(0.25f, unit), "%.1f"))
          thermalIntensity = Units::fromThermalDelta(temperatureDelta, unit);
        tooltip("Variação de temperatura na unidade selecionada.");
      }
      ImGui::SliderInt("Pincel", &radius, 1, 35);

      ImGui::InputTextWithHint("##buscar", "Buscar material...", search, sizeof(search));
      const char *groups[] = {"Todos", "Terreno", "Fluidos", "Estruturas", "Vegetacao"};
      ImGui::Combo("Categoria", &category, groups, IM_ARRAYSIZE(groups));
      ImGui::BeginChild("materiais", ImVec2(0, 185), true);
      const std::string filter = lower(search);
      const auto showItem = [&](const MaterialEntry &entry)
      {
        const int id = db.getId(entry.key);
        return id > 0 && (category == 0 || entry.category == category - 1) &&
               (filter.empty() || lower(entry.key).find(filter) != std::string::npos ||
                lower(entry.label).find(filter) != std::string::npos);
      };
      bool hasFavorites = false;
      for (const auto &entry : MATERIALS)
        hasFavorites |= showItem(entry) && preferences.isFavorite(db.getId(entry.key));
      if (hasFavorites)
      {
        ImGui::TextDisabled("Favoritos");
        ImGui::Separator();
      }
      const auto materialRow = [&](const MaterialEntry &entry)
      {
        const int id = db.getId(entry.key);
        ImGui::PushID(id);
        if (ImGui::SmallButton(preferences.isFavorite(id) ? "*" : "+"))
          preferences.toggleFavorite(id);
        tooltip("Adicionar ou remover dos favoritos");
        ImGui::SameLine();
        if (ImGui::Selectable(entry.label, brush == id))
        {
          brush = id;
          if (tool != 9 && tool != 12)
            tool = 0;
        }
        ImGui::PopID();
      };
      for (const auto &entry : MATERIALS)
        if (showItem(entry) && preferences.isFavorite(db.getId(entry.key)))
          materialRow(entry);
      if (hasFavorites)
      {
        ImGui::Spacing();
        ImGui::TextDisabled("Outros materiais");
        ImGui::Separator();
      }
      for (const auto &entry : MATERIALS)
        if (showItem(entry) && !preferences.isFavorite(db.getId(entry.key)))
          materialRow(entry);
      ImGui::EndChild();

      if (ImGui::CollapsingHeader("Catálogo científico"))
      {
        const auto &def = db.getMaterial(brush);
        if (brush)
        {
          ImGui::Text("%s (%s)", materialLabel(brush), stateLabel(def.state));
          ImGui::Text("Densidade relativa: %.2f", def.density);
          ImGui::Text("Condutividade relativa: %.0f%%", def.thermalConductivity * 100.0f);
          ImGui::Text("Capacidade termica relativa: %.0f%%", def.heatCapacity * 100.0f);
          ImGui::Text("Viscosidade %.0f%% | Coesao %.0f%%",
                      def.viscosity * 100.0f, def.surfaceTension * 100.0f);
          ImGui::Text("Turbulencia %.0f%%", def.turbulence * 100.0f);
          if (def.flammability)
            ImGui::TextDisabled("Inflamável");
          if (def.isAcid)
            ImGui::TextDisabled("Corrosivo");
          if (def.ignitionTemperature <= 1.0f)
            ImGui::Text("Ignicao: %.1f %s",
                        Units::temperature(def.ignitionTemperature,
                                           static_cast<Units::Temperature>(preferences.temperatureUnit)),
                        Units::temperatureName(static_cast<Units::Temperature>(preferences.temperatureUnit)));
          if (!def.description.empty())
            ImGui::TextWrapped("%s", def.description.c_str());
          for (const auto &transition : db.getTransitions(brush))
          {
            ImGui::TextWrapped("De %.1f a %.1f %s: vira %s",
                               Units::temperature(transition.minTemperature,
                                                  static_cast<Units::Temperature>(preferences.temperatureUnit)),
                               Units::temperature(transition.maxTemperature,
                                                  static_cast<Units::Temperature>(preferences.temperatureUnit)),
                               Units::temperatureName(static_cast<Units::Temperature>(preferences.temperatureUnit)),
                               materialLabel(transition.product));
          }
          ImGui::TextDisabled("Medidas aproximadas do modelo de jogo.");
        }
        else
        {
          ImGui::TextDisabled("Selecione um material para examinar.");
        }
      }

      if (pixelButton("Borracha", ImVec2(126, 29)))
        brush = 0, tool = 0;
      tooltip("Apaga qualquer material (botão direito também apaga)");
      ImGui::SameLine();
      if (pixelButton(paused ? "Continuar" : "Pausar", ImVec2(126, 29)))
        paused = !paused;
      if (pixelButton("Desfazer", ImVec2(126, 29)))
        undo();
      tooltip("Ctrl+Z; a simulação fica pausada após desfazer");
      ImGui::SameLine();
      if (pixelButton("Refazer", ImVec2(126, 29)))
        redo();
      tooltip("Ctrl+Y");
      if (pixelButton("Avançar", ImVec2(126, 29)))
      {
        paused = true;
        stepOnce = true;
      }
      tooltip("Executar um quadro (N)");
      ImGui::SameLine();
      if (pixelButton("Centralizar", ImVec2(126, 29)))
      {
        zoom = 1.0f;
        pan = sf::Vector2f(0, 0);
      }
      tooltip("Restaurar zoom e posição (Home)");

      if (ImGui::SliderFloat("Tempo", &preferences.tuning.speed, 0.10f, 3.0f,
                             "%.2fx"))
        preferences.save();
      tooltip("Multiplicador do tempo. Ajuste a intensidade da gravidade em Ajustes.");
      const char *views[] = {"Mundo", "Temperatura", "Umidade", "Nutrientes",
                             "Saturacao do solo", "Pressao", "Tensoes", "Velocidade"};
      if (ImGui::Combo("Visualização", &view, views, IM_ARRAYSIZE(views)))
        world.renderMode = static_cast<World::RenderMode>(view);
      if (ImGui::CollapsingHeader("Ambiente e gráficos"))
      {
        ImGui::Checkbox("Clima automatico", &world.automaticWeather);
        ImGui::Checkbox("Chuva forcada", &world.rainEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Dia e noite", &world.dayNightEnabled);
        const char *qualities[] = {"Básico", "Equilibrado", "Detalhado"};
        if (ImGui::Combo("Qualidade", &preferences.graphicsQuality,
                         qualities, IM_ARRAYSIZE(qualities)))
        {
          world.setGraphicsQuality(preferences.graphicsQuality);
          preferences.save();
        }
        tooltip("Controla a iluminação local. Básico desativa os halos de calor.");
      }
      if (ImGui::CollapsingHeader("Inspecionar partícula"))
      {
        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        if (overCanvas(mouse, size, panel, zoom, pan, world))
        {
          const auto cell = worldAt(mouse, size, panel, zoom, pan, world);
          const int id = world.getParticle(cell.x, cell.y).matId;
          ImGui::Text("Material: %s", id ? materialLabel(id) : "Vazio");
          ImGui::Text("Coordenadas: %d, %d", cell.x, cell.y);
          const auto tunit = static_cast<Units::Temperature>(preferences.temperatureUnit);
          const auto punit = static_cast<Units::Pressure>(preferences.pressureUnit);
          const auto vunit = static_cast<Units::Speed>(preferences.speedUnit);
          ImGui::Text("Temperatura: %.1f %s", Units::temperature(world.getTemperature(cell.x, cell.y), tunit), Units::temperatureName(tunit));
          ImGui::Text("Umidade: %.0f%%", Units::humidityPercent(world.getHumidity(cell.x, cell.y)));
          ImGui::Text("Nutrientes: %.0f ppm", Units::nutrientsPpm(world.getNutrients(cell.x, cell.y)));
          ImGui::Text("Solo: %.0f%%", world.getSoilSaturation(cell.x, cell.y) * 100);
          ImGui::Text("Pressao: %.2f %s", Units::pressure(world.getPressure(cell.x, cell.y), punit), Units::pressureName(punit));
          ImGui::Text("Velocidade X/Y: %.1f / %.1f %s",
                      Units::velocity(world.getParticle(cell.x, cell.y).velocityX, vunit),
                      Units::velocity(world.getParticle(cell.x, cell.y).velocityY, vunit), Units::speedName(vunit));
          ImGui::Text("Dano: %.0f%% | Fixado: %s",
                      std::clamp(world.getParticle(cell.x, cell.y).damage * 50.0f, 0.0f, 100.0f),
                      world.getParticle(cell.x, cell.y).anchored ? "sim" : "nao");
          if (id)
          {
            const auto &def = db.getMaterial(id);
            ImGui::Separator();
            ImGui::Text("Estado: %s | Densidade relativa: %.2f", stateLabel(def.state), def.density);
            ImGui::Text("Condutividade %.0f%% | Capacidade %.0f%%",
                        def.thermalConductivity * 100.0f, def.heatCapacity * 100.0f);
            if (def.ignitionTemperature <= 1.0f)
              ImGui::Text("Ignicao: %.1f %s", Units::temperature(def.ignitionTemperature, tunit), Units::temperatureName(tunit));
            if (!def.description.empty())
              ImGui::TextWrapped("%s", def.description.c_str());
            if (!db.getTransitions(id).empty())
              ImGui::TextDisabled("Transformações térmicas: %zu", db.getTransitions(id).size());
          }
        }
        else
        {
          ImGui::TextDisabled("Aponte para uma partícula do mundo.");
        }
      }

      ImGui::Separator();
      if (ImGui::CollapsingHeader("Gerar mundo"))
      {
        const char *biomes[] = {"Temperado", "Árido", "Pântano"};
        ImGui::Combo("Ambiente", &generationBiome, biomes, IM_ARRAYSIZE(biomes));
        ImGui::InputInt("Seed", &generationSeed);
        generationSeed = std::max(0, generationSeed);
        if (pixelButton("Nova seed", ImVec2(126, 29)))
          generationSeed = generationSeed < 2147483647 ? generationSeed + 1 : 1;
        ImGui::SameLine();
        if (pixelButton("Gerar mundo", ImVec2(126, 29)))
        {
          generateWorld();
        }
      }
      if (pixelButton("Salvar como...", ImVec2(126, 29)))
        saveFile(true);
      tooltip("Escolher arquivo Matterbloom .mbw no disco");
      ImGui::SameLine();
      if (pixelButton("Abrir mundo...", ImVec2(126, 29)))
        openFile();
      tooltip("Abrir mundo .mbw ou mundo antigo .agw");
#ifndef _WIN32
      ImGui::InputText("Arquivo", pathInput, sizeof(pathInput));
#endif
      if (pixelButton("Limpar mundo", ImVec2(262, 29)))
        ImGui::OpenPopup("Confirmar limpeza");

      if (ImGui::BeginPopupModal("Confirmar limpeza", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize))
      {
        pixelWindow();
        ImGui::Text("Apagar todas as partículas deste mundo?");
        ImGui::TextDisabled("Você poderá desfazer esta ação.");
        if (pixelButton("Cancelar", ImVec2(125, 30)))
          ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (pixelButton("Limpar", ImVec2(125, 30)))
        {
          history.checkpoint(world);
          world.clear();
          world.rainEnabled = false;
          activeFile.clear();
          status = "Mundo limpo";
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
      ImGui::TextWrapped("%s", status.c_str());
      ImGui::Text("Partículas: %zu  Plantas: %zu", stats.particles, stats.plants);
      ImGui::TextDisabled("Transformações acumuladas: %llu",
                          static_cast<unsigned long long>(stats.reactions));
      ImGui::TextDisabled("FPS: %.0f | Zoom: %.0f%%", fps, zoom * 100);
      ImGui::TextDisabled("Motor: %.1f ms | Gráficos: %.1f ms", simulationMs, graphicsMs);
      ImGui::TextDisabled("Atualizacoes: %.0f/s | alvo %d Hz",
                          effectiveTicksPerSecond, preferences.tuning.physicsHz);
      ImGui::TextDisabled("Movimento: %zu | Tempestade: %.0f%%",
                          stats.movingParticles, stats.stormIntensity * 100);
      if (preferences.tips)
      {
        ImGui::Separator();
        ImGui::TextWrapped("Clique: desenhar | Direito: apagar | Roda: pincel");
        ImGui::TextWrapped("Ctrl+roda: zoom | Meio: mover | Alt+clique: selecionar");
        ImGui::TextWrapped("Espaço: pausar | N: passo | Tab: ocultar painel");
        ImGui::TextWrapped("Ctrl+S/O: salvar/abrir | F1: ocultar dicas");
        ImGui::TextWrapped("B: materiais | H: aquecer | C: resfriar");
      }
      ImGui::End();
    }

    if (screen != GameScreen::Playing)
    {
      const bool mainMenu = screen == GameScreen::MainMenu;
      const float menuWidth = std::min(440.0f, size.x - 32.0f);
      const float menuHeight = std::min(650.0f, size.y - 42.0f);
      ImGui::SetNextWindowPos(ImVec2((size.x - menuWidth) * 0.5f,
                                     (size.y - menuHeight) * 0.5f),
                              ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_Always);
      ImGui::Begin(mainMenu ? "Inicio##menu" : "Pausa##menu", nullptr,
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
      pixelWindow();
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.63f, 0.95f, 0.76f, 1), "MATTERBLOOM");
      ImGui::TextDisabled("Sandbox cientifico em pixel art");
      ImGui::TextWrapped("Crie ecossistemas. Experimente com materia, forcas e clima.");
      ImGui::Checkbox("Som", &audio.enabled);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(200.0f);
      ImGui::SliderFloat("Volume", &audio.volume, 0.0f, 70.0f, "%.0f %%");
      if (pixelButton("CONFIGURACOES DE FISICA E VIDEO", ImVec2(menuWidth - 32.0f, 28.0f)))
        showSettings = !showSettings;
      ImGui::Separator();
      if (pixelButton(mainMenu ? "ENTRAR NO MUNDO" : "CONTINUAR JOGO",
                      ImVec2(menuWidth - 32.0f, 38.0f)))
      {
        screen = GameScreen::Playing;
        paused = false;
      }
      const char *biomes[] = {"Temperado", "Deserto", "Pantano"};
      const char *environmentTypes[] = {"Mundo natural", "Laboratorio"};
      ImGui::Combo("Tipo de partida", &generationEnvironment,
                   environmentTypes, IM_ARRAYSIZE(environmentTypes));
      if (generationEnvironment == 0)
        ImGui::Combo("Novo bioma", &generationBiome, biomes, IM_ARRAYSIZE(biomes));
      const char *sizes[] = {"512 x 512", "512 x 768", "768 x 768", "1024 x 768"};
      ImGui::Combo("Dimensoes", &generationDimensions, sizes, IM_ARRAYSIZE(sizes));
      const char *scenesNatural[] = {"Sem experimento", "Barragem", "Teste de impacto",
                                     "Camara vulcanica", "Tempestade"};
      const char *scenesLaboratory[] = {"Vazio", "Tanque de pressao", "Teste balistico",
                                        "Camara termica", "Teste de vento"};
      ImGui::Combo("Experimento", &initialExperiment,
                   generationEnvironment == 1 ? scenesLaboratory : scenesNatural, 5);
      if (pixelButton("GERAR NOVO MUNDO", ImVec2(menuWidth - 32.0f, 35.0f)))
        generateWorld();
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.75f, 0.88f, 1.0f, 1.0f), "MUNDOS SALVOS");
      ImGui::InputText("Nome do save", slotName, sizeof(slotName));
      if (pixelButton("SALVAR NESTE SLOT", ImVec2(menuWidth - 32.0f, 32.0f)))
        saveSlot();
      if (pixelButton("Abrir arquivo...", ImVec2(182, 30)))
        openFile();
      ImGui::SameLine();
      if (pixelButton("Atualizar lista", ImVec2(180, 30)))
        refreshSaves();
      ImGui::BeginChild("lista-de-mundos", ImVec2(0, 116), true);
      if (saveSlots.empty())
        ImGui::TextDisabled("Nenhum mundo salvo na pasta saves/");
      for (const auto &path : saveSlots)
        if (ImGui::Selectable(path.filename().u8string().c_str()))
          loadSlot(path);
      ImGui::EndChild();
      ImGui::TextWrapped("%s", status.c_str());
      ImGui::Separator();
      if (!mainMenu && pixelButton("Voltar ao menu principal", ImVec2(240, 30)))
      {
        screen = GameScreen::MainMenu;
        paused = true;
      }
      if (pixelButton("SAIR DO JOGO", ImVec2(menuWidth - 32.0f, 30.0f)))
        window.close();
      ImGui::End();
    }

    if (showSettings)
    {
      const float settingsWidth = std::min(482.0f, size.x - 32.0f);
      ImGui::SetNextWindowPos(ImVec2((size.x - settingsWidth) * 0.5f,
                                     std::max(28.0f, (size.y - 590.0f) * 0.5f)),
                              ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowSize(ImVec2(settingsWidth, std::min(590.0f, size.y - 50.0f)),
                               ImGuiCond_FirstUseEver);
      ImGui::Begin("Configuracoes##simulador", &showSettings,
                   ImGuiWindowFlags_NoCollapse);
      pixelWindow();
      ImGui::TextColored(ImVec4(0.63f, 0.95f, 0.76f, 1), "PAINEL DE SIMULACAO");
      ImGui::Separator();
      bool tuningChanged = false;
      auto &tuning = preferences.tuning;
      if (ImGui::CollapsingHeader("FISICA E TEMPO", ImGuiTreeNodeFlags_DefaultOpen))
      {
        tuningChanged |= ImGui::SliderFloat("Tempo global", &tuning.speed,
                                            0.10f, 3.00f, "%.2fx");
        tooltip("0.7x e o padrao equilibrado; este controle altera o ritmo de tudo.");
        tuningChanged |= ImGui::SliderFloat("Gravidade dos solidos",
                                            &tuning.gravity, 0.10f, 2.0f, "%.2fx");
        tooltip("Altera a aceleracao de pedras, concreto e materiais granulares.");
        tuningChanged |= ImGui::SliderInt("Frequencia do motor (Hz)",
                                          &tuning.physicsHz, 20, 120);
        tooltip("Atualizacoes discretas por segundo. A fisica celular ainda e dependente dos ticks; mais Hz nao significa automaticamente mais realismo.");
        tuningChanged |= ImGui::SliderInt("Maximo de passos/quadro",
                                          &tuning.maxUpdatesPerFrame, 1, 8);
        tuningChanged |= ImGui::SliderFloat("Orcamento CPU (ms)",
                                            &tuning.frameBudgetMs, 2.0f, 25.0f, "%.0f ms");
        ImGui::TextDisabled("Motor: %.1f ms/quadro | efetivos %.0f ticks/s",
                            simulationMs, effectiveTicksPerSecond);
      }
      if (ImGui::CollapsingHeader("VIDEO E PIXEL ART", ImGuiTreeNodeFlags_DefaultOpen))
      {
        const char *qualities[] = {"Leve", "Equilibrado", "Detalhado"};
        if (ImGui::Combo("Qualidade", &preferences.graphicsQuality,
                         qualities, IM_ARRAYSIZE(qualities)))
        {
          world.setGraphicsQuality(preferences.graphicsQuality);
          tuningChanged = true;
        }
        tuningChanged |= ImGui::Checkbox("Luz difusa de fontes quentes", &tuning.heatGlow);
        tuningChanged |= ImGui::SliderFloat("Intensidade de iluminacao",
                                            &tuning.lightStrength, 0.35f, 1.65f, "%.2fx");
        bool syncChanged = ImGui::Checkbox("Sincronizacao vertical", &tuning.verticalSync);
        bool limitChanged = ImGui::SliderInt("Limite de FPS", &tuning.fpsLimit, 30, 144);
        if (syncChanged || limitChanged)
        {
          tuningChanged = true;
          window.setVerticalSyncEnabled(tuning.verticalSync);
          window.setFramerateLimit(tuning.verticalSync ? 0u : static_cast<unsigned>(tuning.fpsLimit));
        }
        ImGui::TextDisabled("Pixels nitidos: filtro nearest-neighbor mantido.");
        ImGui::TextDisabled("Render: %.1f ms/quadro | %.0f FPS", graphicsMs, fps);
      }
      if (tuningChanged)
      {
        tuning.sanitize();
        world.setGravityScale(tuning.gravity);
        world.setRenderTuning(tuning.heatGlow, tuning.lightStrength);
        pacer.reset();
        preferences.save();
      }
      ImGui::Separator();
      if (pixelButton("RESTAURAR PADROES", ImVec2(195.0f, 33.0f)))
      {
        preferences.tuning = SimulationTuning{};
        world.setGravityScale(tuning.gravity);
        world.setRenderTuning(tuning.heatGlow, tuning.lightStrength);
        window.setVerticalSyncEnabled(tuning.verticalSync);
        window.setFramerateLimit(static_cast<unsigned>(tuning.fpsLimit));
        pacer.reset();
        preferences.save();
      }
      ImGui::SameLine();
      if (pixelButton("FECHAR", ImVec2(110.0f, 33.0f)))
        showSettings = false;
      ImGui::End();
    }

    const sf::Vector2i pointer = sf::Mouse::getPosition(window);
    if (aimFiring && aiming && continuousFire && !placeEmitterMode &&
        tool >= FIRST_FORCE_TOOL && tool <= 14 &&
        sf::Mouse::isButtonPressed(sf::Mouse::Left) &&
        sf::Mouse::isButtonPressed(sf::Mouse::Right) &&
        (++aimContinuousFrames % (tool == 14 ? 16 : tool == 9 ? 3
                                                              : 5)) == 0)
      fireAim();
    if (showPanel && screen == GameScreen::Playing && tool == 7 && window.hasFocus() &&
        !ImGui::GetIO().WantCaptureMouse &&
        overCanvas(pointer, size, panel, zoom, pan, world))
    {
      const auto cell = worldAt(pointer, size, panel, zoom, pan, world);
      const int inspected = world.getParticle(cell.x, cell.y).matId;
      const float inspectorX = std::min(static_cast<float>(pointer.x + 22),
                                        std::max(8.0f, size.x - 256.0f));
      const float inspectorY = std::min(static_cast<float>(pointer.y + 18),
                                        std::max(8.0f, size.y - 225.0f));
      ImGui::SetNextWindowPos(ImVec2(inspectorX, inspectorY), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(244, 213), ImGuiCond_Always);
      ImGui::Begin("Lupa##particula", nullptr,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
      pixelWindow();
      ImGui::TextColored(ImVec4(0.63f, 0.95f, 0.78f, 1), "%s",
                         inspected ? materialLabel(inspected) : "Espaco vazio");
      ImGui::TextDisabled("Posicao %d : %d", cell.x, cell.y);
      ImGui::Separator();
      const auto tunit = static_cast<Units::Temperature>(preferences.temperatureUnit);
      const auto punit = static_cast<Units::Pressure>(preferences.pressureUnit);
      const auto vunit = static_cast<Units::Speed>(preferences.speedUnit);
      ImGui::Text("Temperatura  %.1f %s", Units::temperature(world.getTemperature(cell.x, cell.y), tunit), Units::temperatureName(tunit));
      ImGui::Text("Umidade      %.0f%%", Units::humidityPercent(world.getHumidity(cell.x, cell.y)));
      ImGui::Text("Nutrientes   %.0f ppm", Units::nutrientsPpm(world.getNutrients(cell.x, cell.y)));
      ImGui::Text("Solo         %.0f%%", world.getSoilSaturation(cell.x, cell.y) * 100);
      ImGui::Text("Pressao      %.2f %s", Units::pressure(world.getPressure(cell.x, cell.y), punit), Units::pressureName(punit));
      const auto inspectedCell = world.getParticle(cell.x, cell.y);
      ImGui::Text("Vel. X/Y %.1f/%.1f %s", Units::velocity(inspectedCell.velocityX, vunit), Units::velocity(inspectedCell.velocityY, vunit), Units::speedName(vunit));
      ImGui::Text("Dano %.0f%% %s", std::clamp(inspectedCell.damage * 50.0f, 0.0f, 100.0f),
                  inspectedCell.anchored ? "| FIXADO" : "");
      ImGui::End();
    }
    const bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left) ||
                         sf::Mouse::isButtonPressed(sf::Mouse::Right);
    if (screen == GameScreen::Playing && stroking && pressed && !picking &&
        tool != 7 && window.hasFocus() &&
        !ImGui::GetIO().WantCaptureMouse &&
        overCanvas(pointer, size, panel, zoom, pan, world))
    {
      const auto current = worldAt(pointer, size, panel, zoom, pan, world);
      const int paintId = sf::Mouse::isButtonPressed(sf::Mouse::Right) ? 0 : brush;
      const int appliedTool = sf::Mouse::isButtonPressed(sf::Mouse::Right) ? 0 : tool;
      if (appliedTool >= FIRST_FORCE_TOOL && appliedTool <= LAST_FORCE_TOOL)
      {
        const bool oneShot = appliedTool <= 14 && !continuousFire;
        const int interval = appliedTool == 14 ? 16 : appliedTool == 9 ? 3
                                                                       : 5;
        if ((!oneShot || !forceApplied) &&
            (oneShot || (++continuousToolFrames % interval == 1)))
        {
          const float radians = forceAngle * 3.14159265f / 180.0f;
          const float fx = std::cos(radians), fy = -std::sin(radians);
          const float forceRadius = std::min(96.0f, radius * 1.9f + 2.0f);
          const auto mode = static_cast<World::ForceMode>(appliedTool - FIRST_FORCE_TOOL);
          const int affected = appliedTool == 9
                                   ? world.emitMaterial(current.x, current.y,
                                                        brush > 0 ? brush : db.getId("Stone"),
                                                        std::clamp(radius * 2, 4, 24), forceIntensity, fx, fy)
                                   : world.applyForce(current.x, current.y, forceRadius,
                                                      forceIntensity, mode, fx, fy, brush, true);
          if (affected > 0)
            status = std::string("Ferramenta aplicada: ") +
                     std::to_string(affected) + " celulas";
          if (affected > 0 && (appliedTool == 9 || appliedTool == 12 || appliedTool == 14))
            audio.play(appliedTool == 14 ? 2 : appliedTool == 12 ? 1
                                                                 : 0);
          if (challenge == 3 && affected >= 12 &&
              (appliedTool == 9 || appliedTool == 10 || appliedTool == 14) &&
              !challengeComplete)
          {
            challengeComplete = true;
            status = "Desafio concluido: laboratorio de fisica";
          }
          forceApplied = true;
        }
      }
      else
      {
        paintStroke(world, previous.x < 0 ? current : previous, current,
                    radius, paintId, appliedTool, thermalIntensity, seedSpecies);
      }
      previous = current;
    }
    else
    {
      previous = sf::Vector2i(-1, -1);
    }

    if (!window.isOpen())
      break;

    if (!paused && screen == GameScreen::Playing)
      pacer.push(dt, preferences.tuning);
    const auto simulationStart = std::chrono::steady_clock::now();
    int stepsThisFrame = 0;
    while (pacer.ready(preferences.tuning) &&
           stepsThisFrame < preferences.tuning.maxUpdatesPerFrame &&
           (stepsThisFrame == 0 ||
            std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - simulationStart)
                    .count() <
                preferences.tuning.frameBudgetMs))
    {
      world.update(preferences.tuning.timeStep());
      pacer.consume(preferences.tuning);
      ++stepsThisFrame;
    }
    if (stepOnce)
    {
      world.update(preferences.tuning.timeStep());
      stepOnce = false;
    }
    if (dt > 0.0001f)
      effectiveTicksPerSecond = effectiveTicksPerSecond * 0.9f +
                                (stepsThisFrame / dt) * 0.1f;
    simulationMs = simulationMs * 0.9f +
                   std::chrono::duration<float, std::milli>(
                       std::chrono::steady_clock::now() - simulationStart)
                           .count() *
                       0.1f;
    statsTimer += dt;
    if (statsTimer > 0.5f)
    {
      stats = world.getStats();
      statsTimer = 0.0f;
      if (!challengeComplete &&
          ((challenge == 1 && stats.plants >= challengeInitialPlants + 40) ||
           (challenge == 2 && stats.soilMoisture > 0.42f &&
            stats.rainIntensity > 0.02f)))
      {
        challengeComplete = true;
        status = "Desafio concluido: ecossistema em equilibrio";
      }
    }

    const float scale = scaleFor(size, panel, zoom, world);
    const auto origin = originFor(size, panel, zoom, pan, world);
    world.setScale(scale, scale);
    world.setPosition(origin.x, origin.y);
    world.setBackgroundPan(pan.x / std::max(scale, 0.01f));
    window.clear(sf::Color(12, 15, 19));
    const auto graphicsStart = std::chrono::steady_clock::now();
    world.render(window);
    graphicsMs = graphicsMs * 0.9f +
                 std::chrono::duration<float, std::milli>(
                     std::chrono::steady_clock::now() - graphicsStart)
                         .count() *
                     0.1f;
    if (overCanvas(pointer, size, panel, zoom, pan, world) && window.hasFocus() &&
        !ImGui::GetIO().WantCaptureMouse)
    {
      const float r = static_cast<float>(radius) * scale;
      sf::CircleShape preview(std::max(r, 1.0f));
      preview.setOrigin(preview.getRadius(), preview.getRadius());
      preview.setPosition(origin.x + (worldAt(pointer, size, panel, zoom, pan, world).x + 0.5f) * scale,
                          origin.y + (worldAt(pointer, size, panel, zoom, pan, world).y + 0.5f) * scale);
      preview.setFillColor(tool == 1 ? sf::Color(255, 105, 60, 38) : tool == 2 ? sf::Color(95, 174, 255, 38)
                                                                               : sf::Color(80, 225, 160, 35));
      preview.setOutlineColor(tool == 1 ? sf::Color(255, 146, 84, 210) : tool == 2 ? sf::Color(120, 202, 255, 210)
                                                                                   : sf::Color(110, 245, 185, 190));
      preview.setOutlineThickness(1.0f);
      window.draw(preview);
    }
    if (aiming && screen == GameScreen::Playing)
    {
      const float dx = static_cast<float>(aimAnchorScreen.x - aimCursorScreen.x);
      const float dy = static_cast<float>(aimAnchorScreen.y - aimCursorScreen.y);
      const float drag = std::sqrt(dx * dx + dy * dy);
      const float radians = forceAngle * 3.14159265f / 180.0f;
      const float ux = drag < 5.0f ? std::cos(radians) : dx / drag;
      const float uy = drag < 5.0f ? -std::sin(radians) : dy / drag;
      const float power = std::clamp(forceIntensity * std::clamp(drag / 120.0f,
                                                                 0.15f, 1.0f),
                                     0.1f, 12.0f);
      const float length = (tool == 13 || tool == 14)
                               ? (24.0f + power * 8.0f) * scale
                               : 46.0f + power * 25.0f;
      ImDrawList *draw = ImGui::GetForegroundDrawList();
      const ImVec2 anchor(static_cast<float>(aimAnchorScreen.x),
                          static_cast<float>(aimAnchorScreen.y));
      const ImVec2 cursor(static_cast<float>(aimCursorScreen.x),
                          static_cast<float>(aimCursorScreen.y));
      const ImU32 aimColor = IM_COL32(255, 220, 126, 255);
      draw->AddLine(anchor, cursor, IM_COL32(225, 154, 112, 190), 2.0f);
      draw->AddCircle(anchor, 8.0f, aimColor, 12, 2.0f);
      draw->AddLine(ImVec2(anchor.x - 11, anchor.y), ImVec2(anchor.x + 11, anchor.y), aimColor, 1.0f);
      draw->AddLine(ImVec2(anchor.x, anchor.y - 11), ImVec2(anchor.x, anchor.y + 11), aimColor, 1.0f);
      ImVec2 previousDot = anchor;
      for (int sample = 1; sample <= 20; ++sample)
      {
        const float fraction = static_cast<float>(sample) / 20.0f;
        const float distance = length * fraction;
        const float gravityOffset = (tool == 9 || tool == 12)
                                        ? fraction * fraction * 35.0f / std::max(power, 0.5f)
                                        : 0.0f;
        const ImVec2 dot(anchor.x + ux * distance, anchor.y + uy * distance + gravityOffset);
        if (sample % 2 == 0)
          draw->AddLine(previousDot, dot, aimColor, 2.0f);
        previousDot = dot;
      }
      const ImVec2 destination(anchor.x + ux * length,
                               anchor.y + uy * length +
                                   ((tool == 9 || tool == 12) ? 35.0f / std::max(power, 0.5f) : 0.0f));
      draw->AddCircleFilled(destination, 4.0f, aimColor);
      ImGui::SetNextWindowPos(ImVec2(std::clamp(anchor.x + 20.0f, 6.0f, static_cast<float>(size.x) - 205.0f),
                                     std::clamp(anchor.y - 67.0f, 6.0f, static_cast<float>(size.y) - 72.0f)),
                              ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(194.0f, 57.0f), ImGuiCond_Always);
      ImGui::Begin("Mira##pixel", nullptr,
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
      pixelWindow();
      ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.50f, 1.0f), "MIRA ATIVA");
      ImGui::TextDisabled("Forca %.1f / 12  |  Esquerdo: disparar", power);
      ImGui::End();
    }
    ImGui::SFML::Render(window);
    window.display();
  }
  ImGui::SFML::Shutdown();
  return 0;
}
