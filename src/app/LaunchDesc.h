#pragma once

#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <Windows.h>
#include <shellapi.h>

namespace Unnamed {
	/// @brief App 起動引数から抽出した共通オプションです。
	struct LaunchDesc {
		/// @brief `--game` で指定されたゲーム名です。
		std::optional<std::string> gameName = std::nullopt;
		/// @brief `--project` で指定された game_profile.json のパスです。
		std::optional<std::filesystem::path> projectManifestPath = std::nullopt;
		/// @brief `--repo-root` で指定された repo root です。
		std::optional<std::filesystem::path> repoRootOverride = std::nullopt;
		/// @brief `--projects-root` で指定された projects ルートです。
		std::optional<std::filesystem::path> projectsRootOverride = std::nullopt;
		/// @brief `--help` / `-h` が指定されたかどうかです。
		bool showHelp = false;
		/// @brief 起動前検証のみ実行して終了するかどうかです。
		bool validateStartupOnly = false;
		/// @brief 起動引数診断（警告/エラー）です。
		std::vector<std::string> diagnostics = {};
	};

	/// @brief ワイド文字列を UTF-8 文字列へ変換します。
	[[nodiscard]] inline std::string ConvertWideToUtf8(
		const std::wstring_view text
	) {
		if (text.empty()) {
			return {};
		}

		const int requiredSize = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			text.data(),
			static_cast<int>(text.size()),
			nullptr,
			0,
			nullptr,
			nullptr
		);
		if (requiredSize <= 0) {
			return {};
		}

		std::string output(static_cast<size_t>(requiredSize), '\0');
		const int   writtenSize = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			text.data(),
			static_cast<int>(text.size()),
			output.data(),
			requiredSize,
			nullptr,
			nullptr
		);
		if (writtenSize != requiredSize) {
			return {};
		}
		return output;
	}

	/// @brief 起動前ログを標準エラーとデバッガ出力へ送ります。
	[[nodiscard]] inline std::string BuildLaunchMessage(
		const std::string_view appName,
		const std::string_view message
	) {
		std::string line = "[";
		line.append(appName);
		line.append("] ");
		line.append(message);
		return line;
	}

	/// @brief 起動前ログを出力します。
	inline void EmitPreLaunchLog(
		const std::string_view appName,
		const std::string_view message
	) {
		const std::string line = BuildLaunchMessage(appName, message);
		std::fputs((line + "\n").c_str(), stderr);
		::OutputDebugStringA((line + "\n").c_str());
	}

	/// @brief 起動オプションのヘルプを表示します。
	inline void PrintLaunchHelp(const std::string_view executableName) {
		std::string helpText;
		helpText += "Usage:\n";
		helpText += "  ";
		helpText += executableName;
		helpText += " [options]\n\n";
		helpText += "Options:\n";
		helpText += "  --help, -h               ヘルプを表示して終了。\n";
		helpText += "  --game=<name>            ゲームプロファイルを名前またはエイリアスで選択。\n";
		helpText += "  --game <name>            ゲームプロファイルを名前またはエイリアスで選択。\n";
		helpText += "  --project=<path>         game_profile.json を明示指定して起動対象を解決。\n";
		helpText += "  --project <path>         game_profile.json を明示指定して起動対象を解決。\n";
		helpText += "  --repo-root=<path>       明示的にリポジトリルートを指定してマニフェスト検索。\n";
		helpText += "  --repo-root <path>       明示的にリポジトリルートを指定してマニフェスト検索。\n";
		helpText += "  --projects-root=<path>   明示的に projects ルートを指定してマニフェスト検索。\n";
		helpText += "  --projects-root <path>   明示的に projects ルートを指定してマニフェスト検索。\n";
		helpText += "  --validate-startup-only  起動前検証のみ実行して終了。\n\n";
		helpText += "Environment:\n";
		helpText += "  UNNAMED_PROJECTS_ROOT=<path> projects ルートを直接指定してマニフェスト検索。\n";
		helpText += "  UNNAMED_REPO_ROOT=<path> リポジトリルートを指定してマニフェスト検索。\n\n";
		helpText += "マニフェスト検索の優先順位:\n";
		helpText += "  1) --project\n";
		helpText += "  2) --projects-root\n";
		helpText += "  3) UNNAMED_PROJECTS_ROOT\n";
		helpText += "  4) --repo-root\n";
		helpText += "  5) UNNAMED_REPO_ROOT\n";
		helpText += "  6) Upward search from current working directory\n";
		helpText += "  7) Upward search from executable directory\n\n";
		helpText += "Example:\n";
		helpText +=
			"  UnnamedEditorApp.exe --project=S:/Repositories/TD4_01/projects/TeamGame/config/game_profile.json\n";

		std::fputs(helpText.c_str(), stdout);
		::OutputDebugStringA(helpText.c_str());
	}

	/// @brief 解析した引数診断を表示します。
	inline void EmitLaunchOptionDiagnostics(
		const std::string_view  appName,
		const LaunchDesc& options
	) {
		for (const std::string& diagnostic : options.diagnostics) {
			EmitPreLaunchLog(appName, diagnostic);
		}
	}

	/// @brief 現在プロセスのコマンドラインを共通ルールで解析します。
	/// @details `--game[= ]` と `--project[= ]` と `--projects-root[= ]` と `--repo-root[= ]` と `--help/-h` に対応します。
	[[nodiscard]] inline LaunchDesc
	ParseAppLaunchOptionsFromCommandLine() {
		LaunchDesc options          = {};
		const auto       appendDiagnostic = [&](const std::string_view text) {
			options.diagnostics.emplace_back(text);
		};
		const auto isOptionToken = [](const std::wstring_view token) {
			return !token.empty() && token[0] == L'-';
		};
		const auto isEmptyOrWhitespace = [](const std::wstring_view text) {
			for (wchar_t ch : text) {
				if (!iswspace(static_cast<unsigned>(ch))) {
					return false;
				}
			}
			return true;
		};

		int     argc = 0;
		LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
		if (argv == nullptr) {
			appendDiagnostic(
				"コマンドライン引数の解析に失敗しました (CommandLineToArgvW が null を返しました)"
			);
			return options;
		}

		for (int i = 1; i < argc; ++i) {
			const std::wstring_view arg(argv[i]);

			if (arg == L"--help" || arg == L"-h") {
				options.showHelp = true;
				continue;
			}

			if (arg.rfind(L"--game=", 0) == 0) {
				const std::wstring_view gameText = arg.substr(7);
				if (gameText.empty() || isEmptyOrWhitespace(gameText)) {
					appendDiagnostic(
						"empty value for --game=...; オプションが無視されました"
					);
					continue;
				}

				const std::string gameName = ConvertWideToUtf8(gameText);
				if (!gameName.empty()) {
					options.gameName = gameName;
				} else {
					appendDiagnostic(
						"--game の値を UTF-8 に変換できませんでした; オプションが無視されました"
					);
				}
				continue;
			}

			if (arg == L"--game") {
				if (i + 1 >= argc || isOptionToken(argv[i + 1])) {
					appendDiagnostic(
						"--game の後に値がありませんでした; 期待される形式: --game <name>"
					);
					continue;
				}
				const std::string gameName = ConvertWideToUtf8(argv[i + 1]);
				if (!gameName.empty()) {
					options.gameName = gameName;
				} else {
					appendDiagnostic(
						"--game の値を UTF-8 に変換できませんでした; オプションが無視されました"
					);
				}
				++i;
				continue;
			}

			if (arg.rfind(L"--repo-root=", 0) == 0) {
				const std::wstring_view pathText = arg.substr(12);
				if (pathText.empty() || isEmptyOrWhitespace(pathText)) {
					appendDiagnostic(
						"--repo-root の値が空か空白のみでした; オプションが無視されました"
					);
					continue;
				}

				options.repoRootOverride = std::filesystem::path(
					std::wstring(pathText)
				);
				continue;
			}

			if (arg.rfind(L"--projects-root=", 0) == 0) {
				const std::wstring_view pathText = arg.substr(16);
				if (pathText.empty() || isEmptyOrWhitespace(pathText)) {
					appendDiagnostic(
						"--projects-root の値が空か空白のみでした; オプションが無視されました"
					);
					continue;
				}

				options.projectsRootOverride = std::filesystem::path(
					std::wstring(pathText)
				);
				continue;
			}

			if (arg.rfind(L"--project=", 0) == 0) {
				const std::wstring_view pathText = arg.substr(10);
				if (pathText.empty() || isEmptyOrWhitespace(pathText)) {
					appendDiagnostic(
						"--project の値が空か空白のみでした; オプションが無視されました"
					);
					continue;
				}

				options.projectManifestPath = std::filesystem::path(
					std::wstring(pathText)
				);
				continue;
			}

			if (arg == L"--project") {
				if (i + 1 >= argc || isOptionToken(argv[i + 1])) {
					appendDiagnostic(
						"--project の後に値がありませんでした; 期待される形式: --project <path>"
					);
					continue;
				}
				options.projectManifestPath = std::filesystem::path(argv[i + 1]);
				++i;
				continue;
			}

			if (arg == L"--repo-root") {
				if (i + 1 >= argc || isOptionToken(argv[i + 1])) {
					appendDiagnostic(
						"--repo-root の後に値がありませんでした; 期待される形式: --repo-root <path>"
					);
					continue;
				}
				options.repoRootOverride = std::filesystem::path(argv[i + 1]);
				++i;
				continue;
			}

			if (arg == L"--projects-root") {
				if (i + 1 >= argc || isOptionToken(argv[i + 1])) {
					appendDiagnostic(
						"--projects-root の後に値がありませんでした; 期待される形式: --projects-root <path>"
					);
					continue;
				}
				options.projectsRootOverride = std::filesystem::path(argv[i + 1]);
				++i;
				continue;
			}

			if (arg.rfind(L"--", 0) == 0) {
				if (arg == L"--validate-startup-only") {
					options.validateStartupOnly = true;
					continue;
				}
				appendDiagnostic(
					"無効なオプション '" + ConvertWideToUtf8(arg) +
					"'; オプションが無視されました"
				);
			}
		}

		if (options.repoRootOverride.has_value()) {
			std::error_code ec;
			const std::filesystem::path& repoRoot = *options.repoRootOverride;
			const bool exists = std::filesystem::exists(repoRoot, ec);
			if (ec || !exists) {
				appendDiagnostic(
					"--repo-root パスは存在しないかアクセス不能です:" +
					repoRoot.generic_string() +
					"' (manifest search will continue with fallback candidates)"
				);
			}
		}

		if (options.projectManifestPath.has_value()) {
			std::error_code ec;
			const std::filesystem::path& manifestPath = *options.projectManifestPath;
			const bool exists = std::filesystem::exists(manifestPath, ec);
			if (ec || !exists) {
				appendDiagnostic(
					"--project パスは存在しないかアクセス不能です:'" +
					manifestPath.generic_string() +
					"' (explicit manifest load will fail if this remains unresolved)"
				);
			}
		}
		if (options.projectsRootOverride.has_value()) {
			std::error_code ec;
			const std::filesystem::path& projectsRoot = *options.projectsRootOverride;
			const bool exists = std::filesystem::exists(projectsRoot, ec);
			if (ec || !exists) {
				appendDiagnostic(
					"--projects-root パスは存在しないかアクセス不能です:'" +
					projectsRoot.generic_string() +
					"' (manifest search will continue with fallback candidates)"
				);
			}
		}

		::LocalFree(argv);
		return options;
	}
}
