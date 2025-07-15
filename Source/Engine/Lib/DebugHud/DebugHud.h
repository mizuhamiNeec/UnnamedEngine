#pragma once
#include <Lib/Math/Vector/Vec4.h>

constexpr Vec4 kDebugHudTextColor = { 0.71f, 0.71f, 0.71f, 1.0f };
constexpr Vec4 kDebugHudOutlineColor = { 0.0f, 0.0f, 0.0f, 0.75f };

class DebugHud {
public:
	static void Update();

	static void ShowFrameRate();
	static void ShowPlayerInfo();
};
