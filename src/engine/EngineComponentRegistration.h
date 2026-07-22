#pragma once

namespace Unnamed {
	class ComponentRegistry;

	/// @brief エンジン標準コンポーネントの登録を保証します。
	/// @param componentRegistry 登録先コンポーネントレジストリ
	void RegisterDefaultEngineComponents(ComponentRegistry& componentRegistry);
}
