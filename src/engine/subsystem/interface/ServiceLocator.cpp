#include "ServiceLocator.h"

/// @brief サービスロケーターマップのシングルトンインスタンスを取得します
/// @return サービスロケーターマップの参照
ServiceLocatorMap& ServiceLocatorMap::Get() {
	static ServiceLocatorMap instance;
	return instance;
}

/// @brief サービスロケーターマップを取得します
/// @return サービスロケーターマップの参照
std::unordered_map<std::type_index, void*>& ServiceLocatorMap::GetMap() {
	return mMap;
}

/// @brief サービスロケーターマップが有効かどうかを取得します
/// @return 有効ならtrue
bool ServiceLocatorMap::IsValid() const {
	return !mDestroyed;
}

/// @brief デストラクタ
ServiceLocatorMap::~ServiceLocatorMap() {
	mDestroyed = true;
}
