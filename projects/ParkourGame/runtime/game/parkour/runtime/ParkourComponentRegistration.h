#pragma once

namespace Unnamed {
	class ComponentRegistry;

	/// @brief Parkour/共通 gameplay のコンポーネントを明示登録します。
	/// @details 静的初期化順やリンク順に依存せず、Editor でも確実に登録できるようにします。
	void RegisterParkourGameComponents(ComponentRegistry& componentRegistry);
}
