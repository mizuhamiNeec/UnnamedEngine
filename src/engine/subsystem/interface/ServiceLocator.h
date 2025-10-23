#pragma once
#include <typeindex>
#include <unordered_map>

/// @brief サービスロケーターマップクラス
class ServiceLocatorMap {
public:
	static ServiceLocatorMap&                   Get();
	std::unordered_map<std::type_index, void*>& GetMap();
	[[nodiscard]] bool                          IsValid() const;
	~ServiceLocatorMap();

private:
	std::unordered_map<std::type_index, void*> mMap;
	bool                                       mDestroyed = false;
};

/// @class ServiceLocator
/// @brief サービスロケーター
/// @details サービスロケーターは、アプリケーション全体で共有されるサービスのインスタンスを管理します。
class ServiceLocator {
public:
	/// @brief サービスを登録します
	/// @tparam T サービスの型
	/// @param instance サービスのインスタンスへのポインタ
	template <typename T>
	static void Register(T* instance) {
		auto& mapInstance = ServiceLocatorMap::Get();
		if (mapInstance.IsValid()) {
			mapInstance.GetMap()[typeid(T)] = instance;
		}
	}

	/// @brief サービスを取得します
	/// @tparam T サービスの型
	/// @return サービスのインスタンスへのポインタ、存在しない場合はnullptr
	template <typename T>
	static T* Get() {
		auto& mapInstance = ServiceLocatorMap::Get();
		if (!mapInstance.IsValid()) {
			return nullptr;
		}

		const auto it = mapInstance.GetMap().find(typeid(T));
		return (it != mapInstance.GetMap().end()) ?
			       static_cast<T*>(it->second) :
			       nullptr;
	}
};
