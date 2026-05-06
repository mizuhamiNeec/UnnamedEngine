#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "engine/game/IGameModule.h"

namespace Unnamed {
	/// @brief GameModule 生成関数の型です。
	/// @details 将来の DLL/plugin 境界でもこの関数ポインタで登録できます。
	using GameModuleCreateFunction = std::unique_ptr<IGameModule> (*)();

	/// @brief 既定のゲームプロファイル（Paths/alias）を登録します。
	/// @details 既知の game_profile.json を読み込み、Paths/alias を登録します。
	void RegisterDefaultGameModuleProfiles();

	/// @brief game_profile.json 探索に使う repo root を明示指定します。
	/// @details `--repo-root` など App 側引数解決結果を渡します。
	void SetGameModuleManifestRepoRootOverride(const std::filesystem::path& repoRootPath);

	/// @brief game_profile.json 探索に使う projects root を明示指定します。
	/// @details `--projects-root` など App 側引数解決結果を渡します。
	void SetGameModuleManifestProjectsRootOverride(const std::filesystem::path& projectsRootPath);

	/// @brief 明示指定された game_profile.json パスを優先して読み込みます。
	/// @details `--project` など App 側引数解決結果を渡します。
	void SetGameModuleManifestPathOverride(const std::filesystem::path& manifestPath);

	/// @brief 登録済みプロファイルが単一の場合、そのゲーム名を返します。
	/// @details 単一に確定できない場合は空文字を返します。
	[[nodiscard]] std::string ResolveSingleRegisteredGameName();

	/// @brief 指定ゲームの manifest を明示経路へ固定し、以降の解決を `--project` 相当に統一します。
	/// @details 既に読み込み済みの場合は profile 再読込を行い、リンク済み runtime 登録は維持します。
	[[nodiscard]] bool PinGameModuleManifestToGame(std::string_view gameName);

	/// @brief 指定ゲーム名へ生成関数を登録します。
	/// @param gameName 登録名（例: Parkour）
	/// @param createFunction モジュール生成関数
	/// @retval true 登録成功
	/// @retval false gameName または createFunction が不正
	[[nodiscard]] bool RegisterGameModule(
		std::string_view gameName,
		GameModuleCreateFunction createFunction
	);

	/// @brief ゲーム名エイリアスを登録します。
	/// @param aliasName エイリアス名（例: parkourgame）
	/// @param targetGameName 登録済みの正規ゲーム名（例: parkour）
	/// @retval true 登録成功
	/// @retval false 対象ゲーム未登録または引数不正
	[[nodiscard]] bool RegisterGameModuleAlias(
		std::string_view aliasName,
		std::string_view targetGameName
	);

	/// @brief 指定ゲーム名に対応する GameModule を生成します。
	/// @details 未対応のゲーム名は nullptr を返します。
	[[nodiscard]] std::unique_ptr<IGameModule> CreateGameModule(
		std::string_view gameName
	);

	/// @brief 指定ゲーム名に対応するルート情報を解決します。
	/// @details 未対応のゲーム名は空の GameModulePaths を返します。
	[[nodiscard]] GameModulePaths ResolveGameModulePaths(
		std::string_view gameName
	);

	/// @brief 起動導線向け検証の挙動オプションです。
	struct StartupValidationOptions {
		/// @brief 未登録コンポーネントを検出した場合に失敗扱いとするかどうかです。
		bool failOnUnknownComponentTypes = true;
		/// @brief 詳細ログを出力するかどうかです。
		bool emitDetailedLogs = true;
	};

	/// @brief GameModule の manifest と startup scene とコンポーネント登録を検証します。
	[[nodiscard]] bool ValidateGameModuleStartupProfile(
		IGameModule&                      gameModule,
		const StartupValidationOptions& options = {}
	);

	/// @brief ゲーム名を解決して startup 検証を実行します。
	[[nodiscard]] bool ValidateGameModuleStartupProfile(
		std::string_view                  gameName,
		const StartupValidationOptions& options = {}
	);
}
