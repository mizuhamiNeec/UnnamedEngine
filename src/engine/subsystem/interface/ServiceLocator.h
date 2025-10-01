#pragma once
#include <typeindex>
#include <unordered_map>

class ServiceLocatorMap {
public:
	static ServiceLocatorMap& Get() {
		static ServiceLocatorMap instance;
		return instance;
	}

	std::unordered_map<std::type_index, void*>& GetMap() {
		return mMap;
	}

	[[nodiscard]] bool IsValid() const {
		return !mDestroyed;
	}

	~ServiceLocatorMap() {
		mDestroyed = true;
	}

private:
	std::unordered_map<std::type_index, void*> mMap;
	bool mDestroyed = false;
};

/// @class ServiceLocator
/// @brief サービスロケーター
/// @details サービスロケーターは、アプリケーション全体で共有されるサービスのインスタンスを管理します。
class ServiceLocator {
public:
	template <typename T>
	static void Register(T* instance) {
		auto& mapInstance = ServiceLocatorMap::Get();
		if (mapInstance.IsValid()) {
			mapInstance.GetMap()[typeid(T)] = instance;
		}
	}

	template <typename T>
	static T* Get() {
		auto& mapInstance = ServiceLocatorMap::Get();
		if (!mapInstance.IsValid()) {
			return nullptr;
		}

		const auto it = mapInstance.GetMap().find(typeid(T));
		return (it != mapInstance.GetMap().end()) ? static_cast<T*>(it->second) : nullptr;
	}
};
