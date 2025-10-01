#pragma once
#include <runtime/core/math/Math.h>

class GameTime;

constexpr Vec4 kDebugHudTextColor    = {0.71f, 0.71f, 0.71f, 1.0f};
constexpr Vec4 kDebugHudOutlineColor = {0.0f, 0.0f, 0.0f, 0.75f};

class DebugHud {
public:
	explicit DebugHud(GameTime* time) : mTime(time) {
	}

	static void Update(float deltaTime);

	static void ShowFrameRate(float deltaTime);
	static void ShowPlayerInfo();

private:
	GameTime* mTime = nullptr;
};
