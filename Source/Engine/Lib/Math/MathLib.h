#pragma once
#include <numbers>

// ----------------------------------------------------------------------------
// 定数
// ----------------------------------------------------------------------------

// π
inline constexpr float pi = std::numbers::pi_v<float>;

// Convert degree to radian
inline constexpr float deg2Rad = pi / 180.0f;

// Convert radian to degree
inline constexpr float rad2Deg = 180.0f / pi;