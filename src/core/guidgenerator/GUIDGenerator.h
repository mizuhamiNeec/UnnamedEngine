#pragma once

#include <atomic>
#include <cstdint>
#include <random>

class GuidGenerator {
public:
	enum class MODE { SEQUENTIAL, RANDOM64 };

	explicit GuidGenerator(MODE m = MODE::SEQUENTIAL);

	uint64_t Alloc();

private:
	std::atomic<uint64_t> mCounter = {0};

	MODE                                    mMode;
	std::mt19937_64                         mRng;
	std::uniform_int_distribution<uint64_t> mDist64;
};
