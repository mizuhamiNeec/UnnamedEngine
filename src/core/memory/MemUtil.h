#pragma once
#include <cstdint>

class MemUtil {
public:
	static uint64_t AlignUp(uint64_t value, uint64_t alignment);
};
