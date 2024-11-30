#pragma once
#include <chrono>
#include <thread>

#include "../Lib/Console/ConVarManager.h"

class EngineTimer {
public:
	EngineTimer() {
		reference_ = clock::now(); // 現在時間を記録する
	}

	void Update() {
#ifdef _DEBUG
		ImGui::Begin("EngineTimer");
		ImGui::Text("%.2f FPS", 1.0f / deltaTime_);
		ImGui::Text("%.2f ms", deltaTime_ * 1000.0f);

		int totalMilliseconds = static_cast<int>(totalTime_ * 100.0f); // 0.01秒単位
		int hours = (totalMilliseconds / (100 * 60 * 60)) % 24;
		int minutes = (totalMilliseconds / (100 * 60)) % 60;
		int secs = (totalMilliseconds / 100) % 60;
		int centiseconds = totalMilliseconds % 100; // 小数点以下2桁

		ImGui::Text("totalTime : %02d:%02d:%02d.%02d", hours, minutes, secs, centiseconds);
		ImGui::End();
#endif

		// 現在時間を取得する
		const auto now = clock::now();
		// 前回記録からの経過時間を取得する
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

		// deltaTime_を計算する
		deltaTime_ = 1e-6 * static_cast<double>(elapsed.count()); // マイクロ秒から秒に変換

		// totalTime_を更新する
		totalTime_ += deltaTime_;


		const float maxFps = std::stof(ConVarManager::GetConVar("cl_fpsmax")->GetValueAsString());

		// フレームレートをもとにスリープ
		const std::chrono::microseconds kMinTime(static_cast<uint64_t>(1000000.0f / maxFps));
		if (elapsed < kMinTime) {
			std::this_thread::sleep_for(kMinTime - elapsed); // 残り時間だけスリープ
		}

		// 現在の時間を記録する
		reference_ = clock::now();
	}

	// Getter
	[[nodiscard]] float GetDeltaTime() const;
	[[nodiscard]] float GetScaledDeltaTime() const;
	[[nodiscard]] float GetTotalTime() const;

private:
	using clock = std::chrono::steady_clock;
	clock::time_point reference_;
	double deltaTime_ = 0.0f; // 前回のフレームから経過した時間
	double totalTime_ = 0.0f; // エンジンの起動から経過した時間
};
