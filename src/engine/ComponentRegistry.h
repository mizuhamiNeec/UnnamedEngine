#pragma once
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	/// @brief コンポーネントレジストリ
	/// @details
	class ComponentRegistry {
	public:
		using CreateFn = std::unique_ptr<BaseComponent>(*)();

		/// @brief Entryは、コンポーネント生成関数、表示名、型識別子を登録済みcomponentごとに保持します
		struct Entry {
			CreateFn        create = nullptr;
			std::string     displayName;
			std::type_index typeIndex   = std::type_index(typeid(void));
			bool            hasTypeInfo = false;
		};

		/// @brief RegisteredComponentInfoは、Editorのcomponent追加UIへ公開する安定名と表示名を保持します
		struct RegisteredComponentInfo {
			std::string stableName;
			std::string displayName;
		};

		ComponentRegistry()  = default;
		~ComponentRegistry() = default;

		ComponentRegistry(const ComponentRegistry&)            = delete;
		ComponentRegistry& operator=(const ComponentRegistry&) = delete;

		bool Register(
			std::string_view stableName, CreateFn createFn,
			std::string_view displayName
		);
		bool Register(
			std::string_view stableName, CreateFn         createFn,
			std::string_view displayName, std::type_index typeIndex
		);

		template <typename T>
		bool RegisterTyped(
			std::string_view stableName,
			std::string_view displayName
		) {
			static_assert(
				std::is_base_of_v<BaseComponent, T>,
				"T must derive from BaseComponent."
			);
			return Register(
				stableName,
				[]() -> std::unique_ptr<BaseComponent> {
					return std::make_unique<T>();
				},
				displayName,
				std::type_index(typeid(T))
			);
		}

		[[nodiscard]] std::unique_ptr<BaseComponent> Create(
			std::string_view stableName
		) const;

		[[nodiscard]] bool IsRegistered(std::string_view stableName) const;

		[[nodiscard]] const Entry* Find(std::string_view stableName) const;
		[[nodiscard]] std::vector<RegisteredComponentInfo>
		ListRegisteredComponents() const;

		void Clear();

		static ComponentRegistry& Get();

	private:
		bool RegisterInternal(
			std::string_view stableName, CreateFn         createFn,
			std::string_view displayName, std::type_index typeIndex,
			bool             hasTypeInfo
		);

		std::unordered_map<std::string, Entry>           mEntries;
		std::unordered_map<std::type_index, std::string> mStableNamesByType;
	};

	namespace Detail {
		/// @brief AutoComponentRegisterは、static初期化時にcomponent factoryをComponentRegistryへ登録します
		struct AutoComponentRegister final {
			AutoComponentRegister(
				std::string_view            stableName,
				std::string_view            displayName,
				ComponentRegistry::CreateFn createFn
			);
			AutoComponentRegister(
				std::string_view            stableName,
				std::string_view            displayName,
				ComponentRegistry::CreateFn createFn,
				std::type_index             typeIndex
			);
		};
	}
}

// コンポーネント登録マクロ
#define REGISTER_COMPONENT(T)								\
namespace {													\
std::unique_ptr<Unnamed::BaseComponent> Create_##T() {		\
return std::make_unique<T>();								\
}															\
const Unnamed::Detail::AutoComponentRegister gAutoReg_##T(	\
T{}.GetStableName(), T{}.GetComponentName(), &Create_##T, std::type_index(typeid(T)));	\
}

// 拡張コンポーネント登録マクロ
#define REGISTER_COMPONENT_EX(T, stableNameLiteral, displayNameLiteral) \
namespace {																\
std::unique_ptr<Unnamed::BaseComponent> Create_##T() {					\
return std::make_unique<T>();											\
}																		\
const Unnamed::Detail::AutoComponentRegister gAutoReg_##T(				\
(stableNameLiteral), (displayNameLiteral), &Create_##T, std::type_index(typeid(T)));				\
}
