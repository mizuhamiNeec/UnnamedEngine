#pragma once
#include <numbers>

inline constexpr float deg2Rad = static_cast<float>(std::numbers::pi) / 180.0f;
inline constexpr float Rad2Deg = 180.0f / static_cast<float>(std::numbers::pi);