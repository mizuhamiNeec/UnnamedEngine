#pragma once
#include <mutex>
#include <random>

struct Vec3;

//-----------------------------------------------------------------------------
// 乱数生成器
//-----------------------------------------------------------------------------
class Random final {
public:
	static int RandomInt(const int& min = 0, const int& max = 1);
	static float RandomFloat(const float& min = 0.0f, const float& max = 1.0f);
	static Vec3 RandomVec3(const Vec3& min, const Vec3& max);

private:
	static std::mt19937_64 randomEngine_;
	static std::mutex mtx_; // スレッドセーフ
};
