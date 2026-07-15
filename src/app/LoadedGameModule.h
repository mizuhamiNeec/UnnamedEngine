#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "engine/game/IDemoService.h"

namespace Unnamed {
	class Engine;
	class GameModuleRegistry;
	class IGameModule;
	class IGameWorldFactory;
	struct GameRuntimeContext;

	/// @brief App/Launcher 層で GameModule の所有と lifecycle を管理します。
	class LoadedGameModule final {
	public:
		/// @brief 登録済み名から GameModule を生成してロードコンテキストを構築します。
		[[nodiscard]] static std::unique_ptr<LoadedGameModule> Create(
			const GameModuleRegistry& registry,
			std::string_view          requestedModuleName
		);

		~LoadedGameModule();

		/// @brief EngineRuntime が参照するゲームコンテキストを ServiceLocator へ登録します。
		void RegisterRuntimeContextService();
		/// @brief 登録済みゲームコンテキストを ServiceLocator から解除します。
		void UnregisterRuntimeContextService();

		/// @brief Engine 初期化完了後の登録・ロード処理を実行します。
		[[nodiscard]] bool RegisterAndLoad(Engine& engine);
		/// @brief Engine シャットダウン前のアンロード処理を実行します。
		void Unload(Engine& engine);

		/// @brief Engine へ渡すワールドファクトリを返します。
		[[nodiscard]] IGameWorldFactory& GetWorldFactory() const;
		/// @brief Engine へ渡す Demo サービスを生成します。
		[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() const;
		/// @brief Engine へ渡すランタイムコンテキストを返します。
		[[nodiscard]] GameRuntimeContext& GetRuntimeContext() const;
		/// @brief ログ向けに GameModule 名を返します。
		[[nodiscard]] std::string GetGameModuleName() const;
		/// @brief ログ向けに要求 runtime module 名を返します。
		[[nodiscard]] const std::string& GetRequestedModuleName() const;

	private:
		LoadedGameModule(
			std::string                  requestedModuleName,
			std::unique_ptr<IGameModule> gameModule
		);

		std::string                         mRequestedModuleName      = {};
		std::unique_ptr<IGameModule>        mGameModule               = {};
		std::unique_ptr<GameRuntimeContext> mRuntimeContext           = {};
		bool                                mLoaded                   = false;
		bool                                mRuntimeContextRegistered = false;
	};
}
