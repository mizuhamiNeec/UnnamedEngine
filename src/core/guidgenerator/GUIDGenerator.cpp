#include "GuidGenerator.h"

#include <array>

GuidGenerator::GuidGenerator(const MODE m) : mMode(m) {
	if (mMode == MODE::SEQUENTIAL) {
		std::random_device rd;
		std::array         seed{rd(), rd()};
		mRng.seed(*reinterpret_cast<uint64_t*>(seed.data()));
	}
}

uint64_t GuidGenerator::Alloc() {
	switch (mMode) {
	case MODE::SEQUENTIAL:
		return ++mCounter;
	case MODE::RANDOM64:
		return mDist64(mRng) | 0x8000'0000'0000'0000ULL;
	default:
		return ++mCounter;
	}
}
