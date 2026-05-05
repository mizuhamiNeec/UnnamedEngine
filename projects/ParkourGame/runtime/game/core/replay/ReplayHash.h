#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace Unnamed::ReplayHash {
	constexpr uint64_t kFnv1aOffsetBasis = 14695981039346656037ull;
	constexpr uint64_t kFnv1aPrime       = 1099511628211ull;

	inline uint64_t Begin() {
		return kFnv1aOffsetBasis;
	}

	inline void AppendBytes(
		uint64_t& hash, const void* data, const size_t byteCount
	) {
		if (data == nullptr || byteCount == 0) {
			return;
		}
		const auto* bytes = static_cast<const uint8_t*>(data);
		for (size_t i = 0; i < byteCount; ++i) {
			hash ^= bytes[i];
			hash *= kFnv1aPrime;
		}
	}

	inline void AppendString(uint64_t& hash, const std::string_view text) {
		AppendBytes(hash, text.data(), text.size());
	}

	template <typename T>
	inline void AppendPod(uint64_t& hash, const T& value) {
		static_assert(std::is_trivially_copyable_v<T>);
		AppendBytes(hash, &value, sizeof(T));
	}

	template <typename T>
	inline void AppendFloating(uint64_t& hash, const T value) {
		static_assert(std::is_floating_point_v<T>);
		using Bits =
			std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t, uint64_t>;
		Bits bits = 0;
		std::memcpy(&bits, &value, sizeof(T));
		AppendPod(hash, bits);
	}
}
