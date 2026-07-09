#include <pch.h>

#include "engine/game/GameModuleRegistry.h"

#include <algorithm>
#include <ranges>

#include "core/string/StrUtil.h"
#include "engine/game/IGameModule.h"

namespace Unnamed {
	bool GameModuleRegistry::RegisterFactory(
		const std::string_view moduleName,
		const CreateFunction   createFunction
	) {
		if (moduleName.empty() || createFunction == nullptr) {
			return false;
		}

		const std::string normalized = StrUtil::ToLowerCase(moduleName);
		mFactories[normalized]       = createFunction;
		mDisplayNames[normalized]    = std::string(moduleName);
		return true;
	}

	std::unique_ptr<IGameModule> GameModuleRegistry::Create(
		const std::string_view moduleName
	) const {
		const std::string normalized = StrUtil::ToLowerCase(moduleName);
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
		const std::string normalized = StrUtil::ToLowerCase(moduleName);
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
}
