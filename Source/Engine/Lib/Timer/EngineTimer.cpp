#include "EngineTimer.h"

float EngineTimer::GetDeltaTime() const {
	return static_cast<float>(deltaTime_);
}

float EngineTimer::GetScaledDeltaTime() const {
	return static_cast<float>(deltaTime_) * std::stof(ConVarManager::GetConVar("host_timescale")->GetValueAsString());
}

float EngineTimer::GetTotalTime() const {
	return static_cast<float>(totalTime_);
}
