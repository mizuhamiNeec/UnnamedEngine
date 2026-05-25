#pragma once

#include <memory>

namespace Unnamed {
	class World;
	struct WorldServices;

	/// @brief PIE 向けにゲームワールドを生成する抽象ファクトリです。
	class IGameWorldFactory {
	public:
		virtual ~IGameWorldFactory() = default;

		/// @brief Standalone 実行用のワールドを生成します。
		/// @param services ワールドへ注入するサービス群
		/// @return 生成されたワールド
		[[nodiscard]] virtual std::unique_ptr<World> CreateRuntimeWorld(
			const WorldServices& services
		) = 0;

		/// @brief Play-In-Editor 用のワールドを生成します。
		/// @param services ワールドへ注入するサービス群
		/// @return 生成されたワールド
		[[nodiscard]] virtual std::unique_ptr<World> CreatePlayWorld(
			const WorldServices& services
		) = 0;
	};
}
