#pragma once

struct ImVec2
{
  float x, y;
  ImVec2(float a = 0, float b = 0) : x(a), y(b) {}
};
struct ImVec4
{
  float x, y, z, w;
  ImVec4(float a = 0, float b = 0, float c = 0, float d = 0)
      : x(a), y(b), z(c), w(d) {}
};
using ImU32 = unsigned int;
using ImTextureID = void *;
#define IM_COL32(R, G, B, A) ((static_cast<ImU32>(A) << 24) | \
                              (static_cast<ImU32>(B) << 16) | \
                              (static_cast<ImU32>(G) << 8) | static_cast<ImU32>(R))
struct ImDrawList
{
  void AddRectFilled(ImVec2, ImVec2, ImU32, float = 0.0f) {}
  void AddLine(ImVec2, ImVec2, ImU32, float = 1.0f) {}
  void AddCircle(ImVec2, float, ImU32, int = 0, float = 1.0f) {}
  void AddCircleFilled(ImVec2, float, ImU32, int = 0) {}
  void AddImage(ImTextureID, ImVec2, ImVec2, ImVec2, ImVec2, ImU32 = 0) {}
  void ChannelsSplit(int) {}
  void ChannelsSetCurrent(int) {}
  void ChannelsMerge() {}
};
enum ImGuiCol_
{
  ImGuiCol_WindowBg,
  ImGuiCol_Button,
  ImGuiCol_ButtonHovered,
  ImGuiCol_ButtonActive,
  ImGuiCol_FrameBg,
  ImGuiCol_Header,
  ImGuiCol_CheckMark,
  ImGuiCol_COUNT
};
enum ImGuiCond_
{
  ImGuiCond_Always,
  ImGuiCond_FirstUseEver
};
enum ImGuiTreeNodeFlags_
{
  ImGuiTreeNodeFlags_DefaultOpen = 1
};
enum ImGuiWindowFlags_
{
  ImGuiWindowFlags_NoTitleBar = 1,
  ImGuiWindowFlags_NoResize = 2,
  ImGuiWindowFlags_NoMove = 4,
  ImGuiWindowFlags_NoCollapse = 8,
  ImGuiWindowFlags_AlwaysAutoResize = 16
};
struct ImFont
{
};
struct ImFontAtlas
{
  ImFont *AddFontFromFileTTF(const char *, float, void *, const unsigned short *)
  {
    static ImFont f;
    return &f;
  }
  const unsigned short *GetGlyphRangesDefault() const { return nullptr; }
  ImFont *AddFontDefault()
  {
    static ImFont font;
    return &font;
  }
};
struct ImGuiStyle
{
  float WindowRounding = 0;
  float FrameRounding = 0;
  float WindowBorderSize = 0;
  float FrameBorderSize = 0;
  ImVec2 ItemSpacing, WindowPadding;
  ImVec4 Colors[ImGuiCol_COUNT];
};
struct ImGuiIO
{
  bool WantCaptureKeyboard = false;
  bool WantCaptureMouse = false;
  ImFontAtlas *Fonts = nullptr;
};
struct ImGuiListClipper
{
  int DisplayStart = 0, DisplayEnd = 0;
  bool pending = false;
  void Begin(int count, float)
  {
    DisplayStart = 0;
    DisplayEnd = count;
    pending = true;
  }
  bool Step()
  {
    const bool value = pending;
    pending = false;
    return value;
  }
};
#define IM_ARRAYSIZE(array) (sizeof(array) / sizeof(*array))
namespace ImGui
{
  inline ImGuiStyle &GetStyle()
  {
    static ImGuiStyle style;
    return style;
  }
  inline ImGuiIO &GetIO()
  {
    static ImFontAtlas atlas;
    static ImGuiIO io;
    io.Fonts = &atlas;
    return io;
  }
  inline void StyleColorsDark() {}
  inline ImVec2 GetCursorScreenPos() { return {}; }
  inline ImVec2 GetWindowPos() { return {}; }
  inline ImVec2 GetWindowSize() { return {}; }
  inline ImVec2 GetItemRectMin() { return {}; }
  inline ImVec2 GetItemRectMax() { return {}; }
  inline ImDrawList *GetWindowDrawList()
  {
    static ImDrawList list;
    return &list;
  }
  inline ImDrawList *GetForegroundDrawList() { return GetWindowDrawList(); }
  inline void Dummy(ImVec2) {}
  inline void PushStyleColor(ImGuiCol_, ImVec4) {}
  inline void PopStyleColor(int = 1) {}
  inline bool Button(const char *, ImVec2 = {}) { return false; }
  inline void SameLine() {}
  inline void SetNextItemWidth(float) {}
  inline void SetNextWindowPos(ImVec2, ImGuiCond_) {}
  inline void SetNextWindowSize(ImVec2, ImGuiCond_) {}
  inline void Begin(const char *, void *, int) {}
  inline void End() {}
  inline void TextColored(ImVec4, const char *, ...) {}
  inline void TextDisabled(const char *, ...) {}
  inline void Text(const char *, ...) {}
  inline void Separator() {}
  inline void Spacing() {}
  inline bool SliderInt(const char *, int *, int, int) { return false; }
  inline bool SliderFloat(const char *, float *, float, float, const char *) { return false; }
  inline bool BeginTabBar(const char *) { return true; }
  inline bool BeginTabItem(const char *) { return true; }
  inline void EndTabBar() {}
  inline void EndTabItem() {}
  inline void BeginChild(const char *, ImVec2, bool) {}
  inline void EndChild() {}
  inline bool Combo(const char *, int *, const char *const *, int) { return false; }
  inline bool Checkbox(const char *, bool *) { return false; }
  inline bool CollapsingHeader(const char *, int = 0) { return true; }
  inline bool InputInt(const char *, int *) { return false; }
  inline bool IsItemHovered() { return false; }
  inline bool IsItemActive() { return false; }
  inline void SetTooltip(const char *, ...) {}
  inline void PushID(int) {}
  inline void PopID() {}
  inline bool SmallButton(const char *) { return false; }
  inline bool Selectable(const char *, bool = false) { return false; }
  inline void InputTextWithHint(const char *, const char *, char *, std::size_t) {}
  inline void InputText(const char *, char *, std::size_t) {}
  inline void TextWrapped(const char *, ...) {}
  inline void OpenPopup(const char *) {}
  inline bool BeginPopupModal(const char *, void *, int) { return false; }
  inline void CloseCurrentPopup() {}
  inline void EndPopup() {}
}
