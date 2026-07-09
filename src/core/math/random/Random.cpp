#include <core/math/random/Random.h>

#include <mutex>
#include <random>
#include <utility>

//-----------------------------------------------------------------------------
// 指定された範囲の乱数を生成します
//-----------------------------------------------------------------------------
int Random::IntRange(const int& min, const int& max) {
	if (min == max) {
		return min;
	}
	const int                          validMin = std::min(min, max);
	const int                          validMax = std::max(min, max);
	std::uniform_int_distribution<int> distribution(validMin, validMax);
	std::lock_guard                    lock(mtx_);
	return distribution(randomEngine_);
}

//-----------------------------------------------------------------------------
// 指定された範囲の乱数を生成します
//-----------------------------------------------------------------------------
float Random::FloatRange(const float& min, const float& max) {
	if (min == max) {
		return min;
	}
	const float                           validMin = std::min(min, max);
	const float                           validMax = std::max(min, max);
	std::uniform_real_distribution<float> distribution(validMin, validMax);
	std::lock_guard                       lock(mtx_);
	return distribution(randomEngine_);
}

//-----------------------------------------------------------------------------
// 指定された範囲の乱数を生成します
//-----------------------------------------------------------------------------
Vec3 Random::Vec3Range(const Vec3& min, const Vec3& max) {
	return {
		FloatRange(min.x, max.x),
		FloatRange(min.y, max.y),
		FloatRange(min.z, max.z)
	};
}

// 静的メンバ変数の定義
std::mt19937_64 Random::randomEngine_{std::random_device{}()}; // 初期化リストでシード値を設定

std::mutex Random::mtx_; // mutexも定義
