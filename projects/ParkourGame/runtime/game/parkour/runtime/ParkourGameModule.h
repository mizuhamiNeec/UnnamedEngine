#pragma once

#include "engine/game/IGameModule.h"

namespace Unnamed {
	/// @brief Parkour ゲーム向けの GameModule 実装です。
	class ParkourGameModule final : public IGameModule {
	public:
		/// @brief モジュール識別名を返します。
		[[nodiscard]] std::string GetName() const override;
		/// @brief Engine 初期化完了後のロードフックです。
		void OnLoad(Engine& engine) override;
		/// @brief Engine シャットダウン開始時のアンロードフックです。
		void OnUnload(Engine& engine) override;
		/// @brief ゲーム固有コンポーネントを登録します。
		void RegisterComponents(Engine& engine) override;
		/// @brief ゲーム固有システムを登録します。
		void RegisterSystems(Engine& engine) override;
		/// @brief ゲーム固有コンソールコマンドを登録します。
		void RegisterConsoleCommands(Engine& engine) override;
		/// @brief ゲーム固有アセット型を登録します。
		void RegisterAssetTypes(Engine& engine) override;
		/// @brief モジュールを初期化します。
		void Initialize(EngineServices& services) override;
		/// @brief Standalone 向けランタイムワールドを生成します。
		[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
			const WorldServices& services
		) override;
		/// @brief PIE 向けワールドを生成します。
		[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
			const WorldServices& services
		) override;
		/// @brief Demo サービス実装を生成します。
		[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override;
		/// @brief ゲーム固有コンポーネントを登録します。
		void RegisterGameComponents(ComponentRegistry& componentRegistry) override;
		/// @brief ゲーム名・ルート・既定シーン情報を返します。
		[[nodiscard]] GameModulePaths GetGameModulePaths() const override;
		/// @brief UI ドキュメントのデフォルトパスを返します。
		[[nodiscard]] Path GetDefaultUiDocumentPath() const override;
	};

	/// @brief Parkour GameModule を生成します。
	[[nodiscard]] std::unique_ptr<IGameModule> CreateParkourGameModule();
}
