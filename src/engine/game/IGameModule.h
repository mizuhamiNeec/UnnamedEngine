#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "engine/game/IGameWorldFactory.h"

namespace Unnamed {
	class World;
	class IDemoService;
	class ComponentRegistry;
	struct EngineServices;
	struct WorldServices;

	/// @brief ゲームのルート情報と既定起動情報をまとめた構造体です。
	struct GameModulePaths {
		/// @brief ゲーム識別名です。
		std::string gameName;
		/// @brief ゲームルートディレクトリです。
		std::string gameRoot;
		/// @brief コンテンツルートディレクトリです。
		std::string contentRoot;
		/// @brief 設定ファイルルートディレクトリです。
		std::string configRoot;
		/// @brief 既定の起動シーン（contentRoot 相対）です。
		std::string defaultStartupScene;
		/// @brief 解決に成功した game_profile.json の実パスです。
		std::string resolvedManifestPath;
	};

	/// @brief ゲーム側から Engine へ機能注入するためのモジュール抽象です。
	class IGameModule : public IGameWorldFactory {
	public:
		~IGameModule() override = default;

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
}
