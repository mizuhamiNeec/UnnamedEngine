#pragma once
#include <typeindex>
#include <unordered_map>

/// @class ServiceLocator
/// @brief サービスロケーター
/// @details サービスロケーターは、アプリケーション全体で共有されるサービスのインスタンスを管理します。
class ServiceLocator {
public:
	template <typename T>
	static void Register(T* instance) {
		Map()[typeid(T)] = instance;
	}

	template <typename T>
	static T* Get() {
		const auto it = Map().find(typeid(T));
		return (it != Map().end()) ? static_cast<T*>(it->second) : nullptr;
	}

private:
	static std::unordered_map<std::type_index, void*>& Map() {
		static std::unordered_map<std::type_index, void*> instance;
		return instance;
	}
};
