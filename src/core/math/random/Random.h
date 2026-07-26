#pragma once
#include <mutex>
#include <random>

struct Vec3;

//-----------------------------------------------------------------------------
// 乱数生成器
//-----------------------------------------------------------------------------
/// @brief エンジン共通の疑似乱数生成器と分布変換を提供します
class Random final {
public:
	static int   IntRange(const int& min = 0, const int& max = 1);
	static float FloatRange(const float& min = 0.0f, const float& max = 1.0f);
	static Vec3  Vec3Range(const Vec3& min, const Vec3& max);

private:
	static std::mt19937_64 randomEngine_;
	static std::mutex      mtx_; // スレッドセーフ
};
