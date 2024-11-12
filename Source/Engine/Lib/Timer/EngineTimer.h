#pragma once
#include <chrono>
#include <thread>

class EngineTimer {
public:
	EngineTimer() {
		reference_ = clock::now(); // 現在時間を記録する
}

	void Update() {
		// 現在の時間を取得
		auto currentTime = clock::now();

		// 前回のフレームからの経過時間を計算
		std::chrono::duration<double> diff = currentTime - reference_;

		// 秒単位でデルタタイムを更新
		deltaTime_ = diff.count();

		// トータル時間を更新
		totalTime_ += deltaTime_;

		// 次のフレームのために現在時間を保存
		reference_ = currentTime;
	}

	// Getter
	[[nodiscard]] float GetDeltaTime() const;
	[[nodiscard]] float GetTotalTime() const;

private:
	using clock = std::chrono::steady_clock;
	clock::time_point reference_;
	double deltaTime_ = 0.0f; // 前回のフレームから経過した時間
	double totalTime_ = 0.0f; // エンジンの起動から経過した時間
};
