#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace sf
{
  using Uint8 = std::uint8_t;
  struct Color
  {
    Uint8 r = 0, g = 0, b = 0, a = 255;
    Color() = default;
    Color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
    static const Color Transparent;
    static const Color Black;
    static const Color Red;
    static const Color Magenta;
  };
  inline const Color Color::Transparent = Color(0, 0, 0, 0);
  inline const Color Color::Black = Color(0, 0, 0);
  inline const Color Color::Red = Color(255, 0, 0);
  inline const Color Color::Magenta = Color(255, 0, 255);
  struct Vector2f
  {
    float x = 0, y = 0;
    Vector2f() = default;
    Vector2f(float a, float b) : x(a), y(b) {}
  };
  class Texture
  {
    unsigned width = 0, height = 0;

  public:
    inline static std::vector<Uint8> lastPixels;
    bool create(unsigned w, unsigned h)
    {
      width = w;
      height = h;
      return true;
    }
    bool loadFromFile(const std::string &) { return true; }
    void setSmooth(bool) {}
    unsigned getNativeHandle() const { return 1; }
    void update(const Uint8 *pixels)
    {
      lastPixels.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
    }
  };
  class Sprite
  {
    Vector2f scale{1, 1};
    Vector2f position{0, 0};

  public:
    void setTexture(const Texture &) {}
    void setScale(float a, float b) { scale = {a, b}; }
    void setPosition(float a, float b) { position = {a, b}; }
    Vector2f getScale() const { return scale; }
    Vector2f getPosition() const { return position; }
  };
  class RectangleShape
  {
  public:
    explicit RectangleShape(Vector2f) {}
    void setPosition(float, float) {}
    void setFillColor(Color) {}
  };
  class CircleShape
  {
    float radius;

  public:
    explicit CircleShape(float r) : radius(r) {}
    float getRadius() const { return radius; }
    void setOrigin(float, float) {}
    void setPosition(float, float) {}
    void setFillColor(Color) {}
    void setOutlineColor(Color) {}
    void setOutlineThickness(float) {}
  };
  struct Vector2u
  {
    unsigned x = 1280, y = 800;
  };
  class Image
  {
  public:
    bool loadFromFile(const std::string &) { return false; }
    Vector2u getSize() const { return {}; }
    const Uint8 *getPixelsPtr() const { return nullptr; }
  };
  struct FloatRect
  {
    float a, b, c, d;
    FloatRect(float x, float y, float w, float h) : a(x), b(y), c(w), d(h) {}
  };
  struct View
  {
    explicit View(FloatRect) {}
  };
  struct VideoMode
  {
    unsigned width, height;
    VideoMode(unsigned w, unsigned h) : width(w), height(h) {}
  };
  class RenderWindow
  {
  public:
    RenderWindow() = default;
    RenderWindow(struct VideoMode, const std::string &, unsigned = 0) {}
    template <typename T>
    void draw(const T &) {}
    void setFramerateLimit(unsigned) {}
    void setVerticalSyncEnabled(bool) {}
    bool isOpen() const { return false; }
    bool pollEvent(struct Event &) { return false; }
    bool hasFocus() const { return true; }
    void close() {}
    void clear(Color = Color::Black) {}
    void display() {}
    Vector2u getSize() const { return {}; }
    void setView(const View &) {}
  };
  namespace Style
  {
    constexpr unsigned Titlebar = 1;
    constexpr unsigned Close = 2;
    constexpr unsigned Default = 3;
  }
  struct Time
  {
    float asSeconds() const { return 1.0f / 60.0f; }
  };
  struct Clock
  {
    Time restart() { return {}; }
    Time getElapsedTime() const { return {}; }
  };
  struct Vector2i
  {
    int x, y;
    Vector2i(int a = 0, int b = 0) : x(a), y(b) {}
  };
  struct Keyboard
  {
    enum Key
    {
      Space,
      N,
      B,
      C,
      H,
      E,
      I,
      G,
      S,
      O,
      Z,
      Y,
      Tab,
      Home,
      LBracket,
      RBracket,
      F1,
      F2,
      F5,
      F9,
      Escape,
      LControl,
      RControl,
      LAlt,
      RAlt,
      LShift,
      RShift
    };
    static bool isKeyPressed(Key) { return false; }
  };
  struct Mouse
  {
    enum Button
    {
      Left,
      Right,
      Middle
    };
    static bool isButtonPressed(Button) { return false; }
    static Vector2i getPosition(const RenderWindow &) { return {}; }
  };
  struct Event
  {
    enum EventType
    {
      Closed,
      KeyPressed,
      MouseWheelScrolled,
      Resized,
      MouseButtonPressed,
      MouseButtonReleased,
      MouseMoved,
      LostFocus
    };
    EventType type{};
    struct
    {
      Keyboard::Key code{};
      bool control = false, shift = false;
    } key;
    struct
    {
      float delta = 0;
      int x = 0, y = 0;
    } mouseWheelScroll;
    struct
    {
      unsigned width = 1280, height = 800;
    } size;
    struct
    {
      int x = 0, y = 0;
      Mouse::Button button = Mouse::Left;
    } mouseButton;
    struct
    {
      int x = 0, y = 0;
    } mouseMove;
  };
}
