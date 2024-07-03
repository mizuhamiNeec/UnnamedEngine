#pragma once
#include <mutex>
#include <random>

//-----------------------------------------------------------------------------
// 乱数生成器
//-----------------------------------------------------------------------------
class Random final {
public:
	static Random& GetInstance();

	float RandomFloat(float min = 0.0f, float max = 1.0f);
	int RandomInt(int min = 0, int max = 1);

private:
	Random();

	Random(const Random&) = delete;
	Random& operator=(const Random&) = delete;

	std::mt19937_64 rnd;
	std::mutex mtx;
};
