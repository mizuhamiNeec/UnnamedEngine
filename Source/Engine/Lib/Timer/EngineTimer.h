#pragma once
#include <chrono>
#include <thread>

#include "../Lib/Console/ConVarManager.h"

class EngineTimer {
public:
	EngineTimer()
		: startTime_(Clock::now()),
		  lastFrameTime_(Clock::now()),
		  frameDuration_(1.0 / kMaxFps) {}

	void StartFrame() {
		const TimePoint currentTime = Clock::now();
		const std::chrono::duration<double> elapsed = currentTime - lastFrameTime_;
		deltaTime_ = elapsed.count();
		totalTime_ += deltaTime_;
		lastFrameTime_ = currentTime;

#ifdef _DEBUG
		ImGui::Begin("EngineTimer");
		ImGui::Text("%.2f FPS", 1.0 / deltaTime_);
		ImGui::Text("%.2f ms", deltaTime_ * 1000.0);

		const int totalMilliseconds = static_cast<int>(totalTime_ * 100.0); // 0.01秒単位
		const int hours = (totalMilliseconds / (100 * 60 * 60)) % 24;
		const int minutes = (totalMilliseconds / (100 * 60)) % 60;
		const int secs = (totalMilliseconds / 100) % 60;
		const int centiseconds = totalMilliseconds % 100;

		ImGui::Text("Total Time: %02d:%02d:%02d.%02d", hours, minutes, secs, centiseconds);
		ImGui::End();
#endif
	}

	void EndFrame() {
		const auto fpsMaxConVar = ConVarManager::GetConVar("cl_fpsmax");
		const double fpsLimit = std::stod(fpsMaxConVar->GetValueAsString());
		if (fpsLimit > 0) {
			const double frameLimitDuration = 1.0 / fpsLimit;

			// 現在時刻を取得
			const TimePoint currentTime = Clock::now();
			const std::chrono::duration<double> elapsed = currentTime - lastFrameTime_;

			// 次フレームまでの残り時間を計算
			double remainingTime = frameLimitDuration - elapsed.count();
			if (remainingTime > 0.0) {
				// 残り時間の大部分をスリープ
				const double sleepTime = remainingTime * 0.9; // 残り時間の90%をスリープ
				if (sleepTime > 0.0) {
					std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
				}

				// スリープ後の補正（忙待ちで微調整）
				TimePoint afterSleepTime = Clock::now();
				while (std::chrono::duration<double>(afterSleepTime - lastFrameTime_).count() < frameLimitDuration) {
					afterSleepTime = Clock::now();
				}
			}
		}
	}

	static float ScaledDelta() {
		const auto timeScaleConVar = ConVarManager::GetConVar("host_timescale");
		const double timeScale = std::stod(timeScaleConVar->GetValueAsString());
		return static_cast<float>(deltaTime_ * timeScale);
	}

	// Setter
	static void SetTimeScale(const float& scale);

	// Getter
	[[nodiscard]] static float GetDeltaTime();
	[[nodiscard]] static float GetScaledDeltaTime();
	[[nodiscard]] static float GetTotalTime();
	[[nodiscard]] static float GetTimeScale();

private:
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;

	TimePoint startTime_;
	TimePoint lastFrameTime_;
	static double deltaTime_; // 前回のフレームから経過した時間
	static double totalTime_; // エンジンの起動から経過した時間
	double frameDuration_; // フレーム持続時間
};
