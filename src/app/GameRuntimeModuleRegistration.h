#pragma once

namespace Unnamed {
	class GameModuleRegistry;

	/// @brief 静的リンク済み GameModule をレジストリへ登録します。
	void RegisterBuiltInGameModules(GameModuleRegistry& registry);
}
