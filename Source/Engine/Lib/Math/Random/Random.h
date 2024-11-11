#pragma once
#include <mutex>
#include <random>

//-----------------------------------------------------------------------------
// 乱数生成器
//-----------------------------------------------------------------------------
class Random final {
public:
	static Random* GetInstance();

	float RandomFloat(float min = 0.0f, float max = 1.0f);
	int RandomInt(int min = 0, int max = 1);

private:
	std::random_device seedGenerator_;
	std::mt19937_64 randomEngine_;
	std::mutex mtx_;

private:
	static Random* instance_;

	Random() = default;
	~Random() = default;
	Random(Random&) = delete;
	Random& operator=(Random&) = delete;
};
