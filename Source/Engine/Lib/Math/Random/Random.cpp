#include "Random.h"

Random* Random::instance_ = nullptr;

//-----------------------------------------------------------------------------
// 乱数生成器のインスタンスを取得します
//-----------------------------------------------------------------------------
Random* Random::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new Random;
	}
	return instance_;
}

//-----------------------------------------------------------------------------
// 指定された範囲の乱数を生成します
//-----------------------------------------------------------------------------
float Random::RandomFloat(const float min, const float max) {
	std::uniform_real_distribution<float> distribution(min, max);
	//std::lock_guard lock(mtx_); // 追加
	return distribution(randomEngine_);
}

//-----------------------------------------------------------------------------
// 指定された範囲の乱数を生成します
//-----------------------------------------------------------------------------
int Random::RandomInt(const int min, const int max) {
	std::uniform_int_distribution<int> distribution(min, max);
	//std::lock_guard lock(mtx_); // 追加
	return distribution(randomEngine_);
}
