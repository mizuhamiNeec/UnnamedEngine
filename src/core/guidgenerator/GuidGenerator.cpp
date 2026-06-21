#include "GuidGenerator.h"

#include <functional>
#include <limits>
#include <random>
#include <thread>

namespace Unnamed {
	constexpr uint64_t kGuidRandom64MsbMask = 0x8000'0000'0000'0000ULL;

	namespace {
		uint64_t MakeThreadSeed() {
			std::random_device rd;
			uint64_t           seed = 0;
			seed ^= static_cast<uint64_t>(rd()) << 32;
			seed ^= static_cast<uint64_t>(rd());
			seed ^= std::hash<std::thread::id>{}(std::this_thread::get_id());
			return seed;
		}

		uint64_t AllocRandom64ThreadSafe() {
			thread_local std::mt19937_64 rng(MakeThreadSeed());
			thread_local std::uniform_int_distribution<uint64_t> dist(
				0, (std::numeric_limits<uint64_t>::max)()
			);
			return dist(rng) | kGuidRandom64MsbMask;
		}
	}

	GuidGenerator::GuidGenerator(const MODE m) : mMode(m) {
	}

	uint64_t GuidGenerator::Alloc() {
		switch (mMode) {
			case MODE::SEQUENTIAL: {
				return mCounter.fetch_add(
					       1, std::memory_order_relaxed
				       ) + 1;
			}
			case MODE::RANDOM64: return AllocRandom64ThreadSafe();

			default: {
				return mCounter.fetch_add(
					       1, std::memory_order_relaxed
				       ) + 1;
			}
		}
	}
}
