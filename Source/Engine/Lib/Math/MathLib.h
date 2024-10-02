#pragma once
#include <numbers>

namespace Math {
	// ----------------------------------------------------------------------------
	// 定数
	// ----------------------------------------------------------------------------

	// π
	constexpr float pi = std::numbers::pi_v<float>;

	// Convert degree to radian
	constexpr float deg2Rad = pi / 180.0f;

	// Convert radian to degree
	constexpr float rad2Deg = 180.0f / pi;
}