#include "MemUtil.h"

uint64_t MemUtil::AlignUp(const uint64_t value, const uint64_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}
