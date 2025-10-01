#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <engine/subsystem/input/device/base/BaseInputDevice.h>

namespace Unnamed {
	struct KeyHash {
		size_t operator()(const InputKey& key) const noexcept {
			return static_cast<size_t>(key.device) << 24 ^ key.code;
		}
	};

	class KeyNameTable {
	public:
		static std::optional<InputKey> FromString(std::string_view);
		static std::string_view        ToString(const InputKey& key);

		static const std::unordered_map<std::string, InputKey>&
		NameToKey();
		static const std::unordered_map<
			InputKey, std::string_view, KeyHash>&
		KeyToName();

	private:
		static std::string Normalize(std::string_view str);

		static const std::unordered_map<std::string, InputKey> sNameToKey;
		static const std::unordered_map<InputKey, std::string_view, KeyHash>
		sKeyToName;
	};
}
