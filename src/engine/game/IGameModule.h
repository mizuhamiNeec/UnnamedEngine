#pragma once

#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <type_traits>

#include "engine/game/GameModulePaths.h"
#include "engine/game/IGameWorldFactory.h"

namespace Unnamed {
	class Engine;
	class World;
	class IDemoService;
	class ComponentRegistry;
	struct EngineServices;
	struct WorldServices;

	/// @brief ゲーム側から Engine へ機能注入するためのモジュール抽象です。
	class IGameModule : public IGameWorldFactory {
	public:
		~IGameModule() override = default;

		/// @brief モジュール識別名を返します。
		[[nodiscard]] virtual std::string GetName() const = 0;
		/// @brief Engine 初期化完了後に呼ばれるロードフックです。
		virtual void OnLoad(Engine& engine) = 0;
		/// @brief Engine シャットダウン開始時に呼ばれるアンロードフックです。
		virtual void OnUnload(Engine& engine) = 0;
		/// @brief ゲーム固有コンポーネントを登録します。
		virtual void RegisterComponents(Engine& engine) = 0;
		/// @brief ゲーム固有システムを登録します。
		virtual void RegisterSystems(Engine& engine) = 0;
		/// @brief ゲーム固有コンソールコマンドを登録します。
		virtual void RegisterConsoleCommands(Engine& engine) = 0;
		/// @brief ゲーム固有アセット型を登録します。
		virtual void RegisterAssetTypes(Engine& engine) = 0;

		/// @brief Engine 初期化後にゲームモジュールを初期化します。
		virtual void Initialize(EngineServices& services) = 0;
		/// @brief Standalone 実行用のランタイムワールドを生成します。
		[[nodiscard]] virtual std::unique_ptr<World> CreateRuntimeWorld(
			const WorldServices& services
		) = 0;
		/// @brief Demo サービス実装を生成します。
		[[nodiscard]] virtual std::unique_ptr<IDemoService> CreateDemoService() = 0;
		/// @brief ゲーム固有コンポーネントを登録します。
		virtual void RegisterGameComponents(ComponentRegistry& componentRegistry) = 0;
		/// @brief ゲーム名・ルート・既定シーン情報を返します。
		[[nodiscard]] virtual GameModulePaths GetGameModulePaths() const = 0;
		/// @brief 起動時のデフォルトシーンパスを返します。
		[[nodiscard]] virtual std::string GetDefaultStartupScenePath() const {
			return GetGameModulePaths().defaultStartupScene;
		}
		/// @brief UI ドキュメントのデフォルトパスを返します。
		/// @details Engine 側はこの値を利用し、ゲーム固有パスを直書きしません。
		[[nodiscard]] virtual std::string GetDefaultUiDocumentPath() const {
			return {};
		}
	};

	/// @brief Runtime DLL 境界の ABI バージョン定義です。
	enum class GameRuntimeAbiVersion : std::uint32_t {
		V1 = 1,
		Current = V1,
	};

	/// @brief Runtime DLL が公開する C ABI 関数テーブル（v1）です。
	/// @details P2-2 で `LoadLibrary/GetProcAddress` から取得し、静的登録経路と並行運用します。
	struct GameRuntimeApiV1 {
		/// @brief ABI 互換判定用バージョンです。
		std::uint32_t abiVersion =
			static_cast<std::uint32_t>(GameRuntimeAbiVersion::V1);
		/// @brief 構造体サイズです。将来拡張時の前方互換判定に使います。
		std::uint32_t structSize = static_cast<std::uint32_t>(sizeof(GameRuntimeApiV1));
		/// @brief 予約フラグ（現状は 0 固定）です。
		std::uint32_t reservedFlags = 0;
		/// @brief 予約領域（現状は 0 固定）です。
		std::uint32_t reserved = 0;
		/// @brief Runtime 名の取得関数です（ログ/診断向け）。
		const char* (*GetRuntimeName)() = nullptr;
		/// @brief GameModule インスタンス生成関数です。
		IGameModule* (*CreateGameModule)() = nullptr;
		/// @brief GameModule インスタンス破棄関数です。
		void (*DestroyGameModule)(IGameModule* module) = nullptr;
	};

	/// @brief Runtime DLL から取得する API 取得関数シグネチャです。
	using GetGameRuntimeApiV1Function = const GameRuntimeApiV1* (*)();

	/// @brief Runtime DLL で公開する API シンボル名です。
	inline constexpr const char* kGameRuntimeApiV1SymbolName =
		"UnnamedGetGameRuntimeApiV1";

#if defined(_WIN32)
	/// @brief Windows 向け Runtime API エクスポート指定子です。
#define UNNAMED_GAME_RUNTIME_API_EXPORT extern "C" __declspec(dllexport)
#else
	/// @brief 非 Windows 向け Runtime API エクスポート指定子です。
#define UNNAMED_GAME_RUNTIME_API_EXPORT extern "C"
#endif

	/// @brief Runtime API の最小妥当性を検査します。
	[[nodiscard]] inline bool IsValidGameRuntimeApiV1(
		const GameRuntimeApiV1* api
	) {
		if (api == nullptr) {
			return false;
		}
		if (api->abiVersion !=
		    static_cast<std::uint32_t>(GameRuntimeAbiVersion::V1)) {
			return false;
		}
		if (api->structSize < sizeof(GameRuntimeApiV1)) {
			return false;
		}
		return api->GetRuntimeName != nullptr &&
		       api->CreateGameModule != nullptr &&
		       api->DestroyGameModule != nullptr;
	}

	/// @brief Runtime API 向けに GameModule を生成します。
	template <typename TGameModule>
	[[nodiscard]] IGameModule* CreateRuntimeGameModuleInstance() {
		static_assert(
			std::is_base_of_v<IGameModule, TGameModule>,
			"TGameModule must derive from IGameModule."
		);
		return new (std::nothrow) TGameModule();
	}

	/// @brief Runtime API 向けに GameModule を破棄します。
	template <typename TGameModule>
	void DestroyRuntimeGameModuleInstance(IGameModule* module) {
		static_assert(
			std::is_base_of_v<IGameModule, TGameModule>,
			"TGameModule must derive from IGameModule."
		);
		delete static_cast<TGameModule*>(module);
	}

	template <typename TGameModule>
	[[nodiscard]] const char*& RuntimeNameStorageForGameModuleType() {
		static const char* kRuntimeName = "";
		return kRuntimeName;
	}

	template <typename TGameModule>
	[[nodiscard]] const char* GetRuntimeNameForGameModuleType() {
		return RuntimeNameStorageForGameModuleType<TGameModule>();
	}

	template <typename TGameModule>
	void SetRuntimeNameForGameModuleType(const char* runtimeName) {
		RuntimeNameStorageForGameModuleType<TGameModule>() =
			runtimeName == nullptr ? "" : runtimeName;
	}

	/// @brief 指定型の GameModule 用 Runtime API v1 を構築します。
	template <typename TGameModule>
	[[nodiscard]] const GameRuntimeApiV1* BuildGameRuntimeApiV1(
		const char* runtimeName
	) {
		static_assert(
			std::is_base_of_v<IGameModule, TGameModule>,
			"TGameModule must derive from IGameModule."
		);

		// 同一モジュール型については最初に渡された名前を返す運用とする。
		SetRuntimeNameForGameModuleType<TGameModule>(runtimeName);
		static const GameRuntimeApiV1 kApi = {
			.abiVersion = static_cast<std::uint32_t>(GameRuntimeAbiVersion::V1),
			.structSize = static_cast<std::uint32_t>(sizeof(GameRuntimeApiV1)),
			.reservedFlags = 0u,
			.reserved = 0u,
			.GetRuntimeName = &GetRuntimeNameForGameModuleType<TGameModule>,
			.CreateGameModule = &CreateRuntimeGameModuleInstance<TGameModule>,
			.DestroyGameModule = &DestroyRuntimeGameModuleInstance<TGameModule>,
		};
		return &kApi;
	}

#define UNNAMED_EXPORT_GAME_RUNTIME_API_V1(runtime_name_literal, game_module_type) \
	UNNAMED_GAME_RUNTIME_API_EXPORT const ::Unnamed::GameRuntimeApiV1*             \
	UnnamedGetGameRuntimeApiV1() {                                                  \
		return ::Unnamed::BuildGameRuntimeApiV1<game_module_type>(                  \
			runtime_name_literal                                                     \
		);                                                                          \
	}
}
