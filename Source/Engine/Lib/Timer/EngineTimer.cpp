#include "EngineTimer.h"

double EngineTimer::deltaTime_ = 0;
double EngineTimer::totalTime_ = 0;

void EngineTimer::SetTimeScale(const float& scale) {
	ConVarManager::GetConVar("host_timescale")->SetValueFromString(std::to_string(scale));
}

float EngineTimer::GetDeltaTime() {
	return static_cast<float>(deltaTime_);
}

float EngineTimer::GetScaledDeltaTime() {
	return static_cast<float>(deltaTime_) * GetTimeScale();
}

float EngineTimer::GetTotalTime() {
	return static_cast<float>(totalTime_);
}

float EngineTimer::GetTimeScale() {
	return std::stof(ConVarManager::GetConVar("host_timescale")->GetValueAsString());
}
