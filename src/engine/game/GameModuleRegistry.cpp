#include "GameModuleRegistry.h"

#include <algorithm>
#include <cctype>
#include <ranges>

namespace Unnamed {
	bool GameModuleRegistry::RegisterFactory(
		const std::string_view moduleName,
		const CreateFunction   createFunction
	) {
		if (moduleName.empty() || createFunction == nullptr) {
			return false;
		}

		const std::string normalized = NormalizeModuleName(moduleName);
		mFactories[normalized] = createFunction;
		mDisplayNames[normalized] = std::string(moduleName);
		return true;
	}

	std::unique_ptr<IGameModule> GameModuleRegistry::Create(
		const std::string_view moduleName
	) const {
		const std::string normalized = NormalizeModuleName(moduleName);
		if (normalized.empty()) {
			return nullptr;
		}

		const auto factoryIt = mFactories.find(normalized);
		if (factoryIt == mFactories.end() || factoryIt->second == nullptr) {
			return nullptr;
		}
		return factoryIt->second();
	}

	bool GameModuleRegistry::Contains(const std::string_view moduleName) const {
		const std::string normalized = NormalizeModuleName(moduleName);
		if (normalized.empty()) {
			return false;
		}
		return mFactories.contains(normalized);
	}

	std::vector<std::string> GameModuleRegistry::ListRegisteredNames() const {
		std::vector<std::string> names = {};
		names.reserve(mDisplayNames.size());
		for (const auto& [normalizedName, displayName] : mDisplayNames) {
			(void)normalizedName;
			names.push_back(displayName);
		}
		std::ranges::sort(names);
		return names;
	}

	std::string GameModuleRegistry::NormalizeModuleName(
		const std::string_view moduleName
	) {
		std::string normalized(moduleName);
		std::transform(
			normalized.begin(),
			normalized.end(),
			normalized.begin(),
			[](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
		);
		return normalized;
	}
}
