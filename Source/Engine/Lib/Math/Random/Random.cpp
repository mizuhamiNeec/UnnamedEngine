#include "Random.h"

//-----------------------------------------------------------------------------
// 乱数生成器のインスタンスを取得します
//-----------------------------------------------------------------------------
Random& Random::GetInstance() {
	static Random instance;
	return instance;
}

//-----------------------------------------------------------------------------
// 指定された範囲の乱数を生成します
//-----------------------------------------------------------------------------
float Random::RandomFloat(const float min, const float max) {
	std::lock_guard lock(mtx);
	std::uniform_real_distribution<float> distribution(min, max);
	return distribution(rnd);
}

//-----------------------------------------------------------------------------
// 指定された範囲の乱数を生成します
//-----------------------------------------------------------------------------
int Random::RandomInt(const int min, const int max) {
	std::lock_guard lock(mtx);
	std::uniform_int_distribution<int> distribution(min, max);
	return distribution(rnd);
}

Random::Random() : rnd(std::random_device{}()) {}
