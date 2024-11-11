#include "EngineTimer.h"

float EngineTimer::GetDeltaTime() const {
	return static_cast<float>(deltaTime_);
}

float EngineTimer::GetTotalTime() const {
	return static_cast<float>(totalTime_);
}
