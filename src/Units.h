#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace Units
{
  constexpr float CellMeters = 0.08f;
  constexpr float SimulationHz = 60.0f;

  enum class Temperature
  {
    Celsius,
    Fahrenheit,
    Kelvin
  };
  enum class Pressure
  {
    Kilopascal,
    Bar,
    Psi
  };
  enum class Speed
  {
    MetersPerSecond,
    KilometersPerHour
  };

  inline float celsius(float value)
  {
    value = std::clamp(value, -1.0f, 1.0f);
    if (value < -0.46f)
      return (value + 0.46f) * 100.0f;
    if (value < 0.0f)
      return (value + 0.46f) * (20.0f / 0.46f);
    if (value <= 0.57f)
      return 20.0f + value * (80.0f / 0.57f);
    return 100.0f + (value - 0.57f) * (1100.0f / 0.43f);
  }

  inline float temperature(float value, Temperature unit)
  {
    const float c = celsius(value);
    return unit == Temperature::Fahrenheit ? c * 1.8f + 32.0f : unit == Temperature::Kelvin ? c + 273.15f
                                                                                            : c;
  }

  inline const char *temperatureName(Temperature unit)
  {
    return unit == Temperature::Fahrenheit ? "°F" : unit == Temperature::Kelvin ? "K"
                                                                                : "°C";
  }

  inline float thermalDelta(float value, Temperature unit)
  {
    return value * 140.0f * (unit == Temperature::Fahrenheit ? 1.8f : 1.0f);
  }

  inline float fromThermalDelta(float value, Temperature unit)
  {
    return value / (140.0f * (unit == Temperature::Fahrenheit ? 1.8f : 1.0f));
  }

  inline float pressure(float value, Pressure unit)
  {
    const float kpa = 101.325f + std::max(0.0f, value) * 200.0f;
    return unit == Pressure::Bar ? kpa / 100.0f : unit == Pressure::Psi ? kpa * 0.1450377f
                                                                        : kpa;
  }

  inline const char *pressureName(Pressure unit)
  {
    return unit == Pressure::Bar ? "bar" : unit == Pressure::Psi ? "psi"
                                                                 : "kPa";
  }

  inline float velocity(float pixelsPerTick, Speed unit)
  {
    const float metersPerSecond = pixelsPerTick * CellMeters * SimulationHz;
    return unit == Speed::KilometersPerHour ? metersPerSecond * 3.6f : metersPerSecond;
  }

  inline const char *speedName(Speed unit)
  {
    return unit == Speed::KilometersPerHour ? "km/h" : "m/s";
  }

  inline float windSpeed(float modelWind, Speed unit)
  {
    return unit == Speed::KilometersPerHour ? modelWind * 18.0f : modelWind * 5.0f;
  }

  inline float humidityPercent(float value) { return std::clamp(value, 0.0f, 1.0f) * 100.0f; }
  inline float nutrientsPpm(float value) { return std::clamp(value, 0.0f, 1.0f) * 1800.0f; }
  inline float rainfallMmPerHour(float value) { return std::max(0.0f, value) * 45.0f; }

  inline std::string clock24(float hour)
  {
    const int minutes = ((static_cast<int>(std::floor(hour * 60.0f + 0.5f)) % 1440) + 1440) % 1440;
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes / 60, minutes % 60);
    return buffer;
  }
}
