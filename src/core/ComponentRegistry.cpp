#include "ComponentRegistry.h"

#include <algorithm>

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	bool ComponentRegistry::Register(
		const std::string_view stableName, const CreateFn createFn,
		const std::string_view displayName
	) {
		return RegisterInternal(
			stableName,
			createFn,
			displayName,
			std::type_index(typeid(void)),
			false
		);
	}

	bool ComponentRegistry::Register(
		const std::string_view stableName, const CreateFn createFn,
		const std::string_view displayName,
		const std::type_index  typeIndex
	) {
		return RegisterInternal(
			stableName, createFn, displayName, typeIndex, true
		);
	}

	std::unique_ptr<BaseComponent> ComponentRegistry::Create(
		const std::string_view stableName
	) const {
		const Entry* e = Find(stableName);
		if (!e || !e->create) {
			return nullptr;
		}

		return e->create();
	}

	bool ComponentRegistry::IsRegistered(
		const std::string_view stableName
	) const {
		return Find(stableName) != nullptr;
	}

	const ComponentRegistry::Entry* ComponentRegistry::Find(
		const std::string_view stableName
	) const {
		if (stableName.empty()) {
			return nullptr;
		}

		const auto it = mEntries.find(std::string(stableName));
		if (it == mEntries.end()) {
			return nullptr;
		}
		return &it->second;
	}

	std::vector<ComponentRegistry::RegisteredComponentInfo>
	ComponentRegistry::ListRegisteredComponents() const {
		std::vector<RegisteredComponentInfo> result;
		result.reserve(mEntries.size());

		for (const auto& [stableName, entry] : mEntries) {
			RegisteredComponentInfo info;
			info.stableName  = stableName;
			info.displayName = entry.displayName;
			result.emplace_back(std::move(info));
		}

		std::ranges::sort(
			result,
			[](
			const RegisteredComponentInfo& lhs,
			const RegisteredComponentInfo& rhs
		) {
				return lhs.stableName < rhs.stableName;
			}
		);
		return result;
	}

	void ComponentRegistry::Clear() {
		mEntries.clear();
		mStableNamesByType.clear();
	}

	ComponentRegistry& ComponentRegistry::Get() {
		static ComponentRegistry sRegistry;
		return sRegistry;
	}

	bool ComponentRegistry::RegisterInternal(
		const std::string_view stableName, const CreateFn createFn,
		const std::string_view displayName,
		const std::type_index  typeIndex, const bool hasTypeInfo
	) {
		if (stableName.empty() || createFn == nullptr) {
			return false;
		}

		std::string key(stableName);
		const auto  it = mEntries.find(key);
		if (it != mEntries.end()) {
			const Entry& existing              = it->second;
			const bool   sameTypedRegistration =
				hasTypeInfo && existing.hasTypeInfo &&
				existing.typeIndex == typeIndex;
			if (sameTypedRegistration) {
				if (
					!displayName.empty() &&
					!existing.displayName.empty() &&
					existing.displayName != displayName
				) {
					Warning(
						"ComponentRegistry",
						"Component '{}' was already registered with display name '{}'; duplicate registration requested display name '{}'. Keeping existing value.",
						stableName,
						existing.displayName,
						displayName
					);
				}
				return true;
			}

			const bool sameLegacyFactory =
				!hasTypeInfo && !existing.hasTypeInfo &&
				existing.create == createFn;
			if (sameLegacyFactory) {
				if (
					!displayName.empty() &&
					!existing.displayName.empty() &&
					existing.displayName != displayName
				) {
					Warning(
						"ComponentRegistry",
						"Component '{}' was already registered with display name '{}'; duplicate legacy registration requested display name '{}'. Keeping existing value.",
						stableName,
						existing.displayName,
						displayName
					);
				}
				return true;
			}

			if (existing.hasTypeInfo && hasTypeInfo) {
				Error(
					"ComponentRegistry",
					"Conflicting component registration for stable name '{}': existing type='{}' new type='{}'.",
					stableName,
					existing.typeIndex.name(),
					typeIndex.name()
				);
			} else {
				Error(
					"ComponentRegistry",
					"Conflicting component registration for stable name '{}': type information is missing on one side and safety cannot be guaranteed.",
					stableName
				);
			}
			return false;
		}

		if (hasTypeInfo) {
			const auto typeIt = mStableNamesByType.find(typeIndex);
			if (typeIt != mStableNamesByType.end() && typeIt->second != key) {
				Error(
					"ComponentRegistry",
					"Component type '{}' is already registered as stable name '{}' and cannot be re-registered as '{}'.",
					typeIndex.name(),
					typeIt->second,
					stableName
				);
				return false;
			}
		}

		Entry e;
		e.create      = createFn;
		e.displayName = std::string(displayName);
		e.typeIndex   = typeIndex;
		e.hasTypeInfo = hasTypeInfo;

		mEntries.emplace(std::move(key), std::move(e));
		if (hasTypeInfo) {
			mStableNamesByType[typeIndex] = std::string(stableName);
		}
		return true;
	}

	namespace Detail {
		AutoComponentRegister::AutoComponentRegister(
			const std::string_view            stableName,
			const std::string_view            displayName,
			const ComponentRegistry::CreateFn createFn
		) {
			ComponentRegistry::Get().Register(
				stableName, createFn, displayName
			);
		}

		AutoComponentRegister::AutoComponentRegister(
			const std::string_view            stableName,
			const std::string_view            displayName,
			const ComponentRegistry::CreateFn createFn,
			const std::type_index             typeIndex
		) {
			ComponentRegistry::Get().Register(
				stableName, createFn, displayName, typeIndex
			);
		}
	}
}
