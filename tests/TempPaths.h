#pragma once

#include <filesystem>

inline std::filesystem::path testTempFile(const char *name)
{
  return std::filesystem::temp_directory_path() / name;
}
