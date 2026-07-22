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

#include "core/filesystem/Path.h"

namespace Unnamed {
	/// @brief App 起動引数から抽出した共通オプションです。
	struct LaunchDesc {
		/// @brief `--game` で指定されたゲーム名です。
		std::optional<std::string> gameName = std::nullopt;
		/// @brief `--project` で指定された game_profile.json のパスです。
		std::optional<Path> projectManifestPath = std::nullopt;
		/// @brief `--repo-root` で指定された repo root です。
		std::optional<Path> repoRootOverride = std::nullopt;
		/// @brief `--projects-root` で指定された projects ルートです。
		std::optional<Path> projectsRootOverride = std::nullopt;
		/// @brief `--help` / `-h` が指定されたかどうかです。
		bool showHelp = false;
		/// @brief 起動前検証のみ実行して終了するかどうかです。
		bool validateStartupOnly = false;
		/// @brief 音声出力を無効化して起動するかどうかです。
		bool disableAudio = false;
		/// @brief 起動引数診断（警告/エラー）です。
		std::vector<std::string> diagnostics = {};
	};

	/// @brief UTF-16文字列をUTF-8へ変換します。
	[[nodiscard]] inline std::string ConvertWideToUtf8(
		const std::wstring_view wide
	) {
		if (wide.empty()) {
			return {};
		}

		const int wideLength = static_cast<int>(wide.size());
		const int utf8Length = WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			wide.data(),
			wideLength,
			nullptr,
			0,
			nullptr,
			nullptr
		);
		if (utf8Length <= 0) {
			return {};
		}

		std::string utf8(static_cast<size_t>(utf8Length), '\0');
		const int   convertedLength = WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			wide.data(),
			wideLength,
			utf8.data(),
			utf8Length,
			nullptr,
			nullptr
		);
		if (convertedLength <= 0) {
			return {};
		}
		return utf8;
	}

	/// @brief UTF-8文字列をデバッガ出力へ送ります。
	inline void OutputDebugStringUtf8(const std::string_view text) {
		if (text.empty()) {
			return;
		}

		const int utf8Length = static_cast<int>(text.size());
		const int wideLength = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			text.data(),
			utf8Length,
			nullptr,
			0
		);
		if (wideLength <= 0) {
			return;
		}

		std::wstring wide(static_cast<size_t>(wideLength), L'\0');
		const int    convertedLength = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			text.data(),
			utf8Length,
			wide.data(),
			wideLength
		);
		if (convertedLength <= 0) {
			return;
		}
		OutputDebugStringW(wide.c_str());
	}

	/// @brief UTF-16引数からOSネイティブのfilesystem::pathを作ります。
	[[nodiscard]] inline Path MakeNativePath(
		const std::wstring_view path
	) {
		return Path::FromNative(std::filesystem::path(std::wstring(path)));
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
		OutputDebugStringUtf8(line + "\n");
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
		helpText +=
			"  --project=<path>         game_profile.json を明示指定して起動対象を解決。\n";
		helpText +=
			"  --project <path>         game_profile.json を明示指定して起動対象を解決。\n";
		helpText += "  --repo-root=<path>       明示的にリポジトリルートを指定してマニフェスト検索。\n";
		helpText += "  --repo-root <path>       明示的にリポジトリルートを指定してマニフェスト検索。\n";
		helpText +=
			"  --projects-root=<path>   明示的に projects ルートを指定してマニフェスト検索。\n";
		helpText +=
			"  --projects-root <path>   明示的に projects ルートを指定してマニフェスト検索。\n";
		helpText += "  --validate-startup-only  起動前検証のみ実行して終了。\n";
		helpText += "  --disable-audio          音声出力を無効化して起動。\n\n";
		helpText += "Environment:\n";
		helpText +=
			"  UNNAMED_PROJECTS_ROOT=<path> projects ルートを直接指定してマニフェスト検索。\n";
		helpText += "  UNNAMED_REPO_ROOT=<path> リポジトリルートを指定してマニフェスト検索。\n\n";
		helpText += "マニフェスト検索の優先順位:\n";
		helpText += "  1) --project\n";
		helpText += "  2) --projects-root\n";
		helpText += "  3) UNNAMED_PROJECTS_ROOT\n";
		helpText += "  4) --repo-root\n";
		helpText += "  5) UNNAMED_REPO_ROOT\n";
		helpText += "  6) Upward search from current working directory\n";
		helpText += "  7) Upward search from executable directory\n\n";

		std::fputs(helpText.c_str(), stdout);
		OutputDebugStringUtf8(helpText);
	}

	/// @brief 解析した引数診断を表示します。
	inline void EmitLaunchOptionDiagnostics(
		const std::string_view appName,
		const LaunchDesc&      options
	) {
		for (const std::string& diagnostic : options.diagnostics) {
			EmitPreLaunchLog(appName, diagnostic);
		}
	}

	/// @brief 現在プロセスのコマンドラインを共通ルールで解析します。
	/// @details `--game[= ]`、`--project[= ]`、`--projects-root[= ]`、`--repo-root[= ]`、`--validate-startup-only`、`--disable-audio`、`--help/-h` に対応します。
	[[nodiscard]] inline LaunchDesc ParseAppLaunchOptionsFromCommandLine() {
		LaunchDesc options          = {};
		const auto appendDiagnostic = [&](const std::string_view text) {
			options.diagnostics.emplace_back(text);
		};
		const auto isOptionToken = [](const std::wstring_view token) {
			return !token.empty() && token[0] == L'-';
		};
		const auto isEmptyOrWhitespace = [](const std::wstring_view text) {
			for (const wchar_t ch : text) {
				if (!iswspace(ch)) {
					return false;
				}
			}
			return true;
		};

		int     argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
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

				options.repoRootOverride = MakeNativePath(pathText);
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

				options.projectsRootOverride = MakeNativePath(pathText);
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

				options.projectManifestPath = MakeNativePath(pathText);
				continue;
			}

			if (arg == L"--project") {
				if (i + 1 >= argc || isOptionToken(argv[i + 1])) {
					appendDiagnostic(
						"--project の後に値がありませんでした; 期待される形式: --project <path>"
					);
					continue;
				}
				options.projectManifestPath = MakeNativePath(argv[i + 1]);
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
				options.repoRootOverride = MakeNativePath(argv[i + 1]);
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
				options.projectsRootOverride = MakeNativePath(argv[i + 1]);
				++i;
				continue;
			}

			if (arg.rfind(L"--", 0) == 0) {
				if (arg == L"--validate-startup-only") {
					options.validateStartupOnly = true;
					continue;
				}
				if (arg == L"--disable-audio") {
					options.disableAudio = true;
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
			const Path& repoRoot = *options.repoRootOverride;
			const bool exists = std::filesystem::exists(repoRoot.Native(), ec);
			if (ec || !exists) {
				appendDiagnostic(
					"--repo-root パスは存在しないかアクセス不能です:" +
					repoRoot.ToGenericUtf8() +
					"' (manifest search will continue with fallback candidates)"
				);
			}
		}

		if (options.projectManifestPath.has_value()) {
			std::error_code ec;
			const Path&     manifestPath = *options.projectManifestPath;
			const bool      exists       = std::filesystem::exists(
				manifestPath.Native(), ec
			);
			if (ec || !exists) {
				appendDiagnostic(
					"--project パスは存在しないかアクセス不能です:'" +
					manifestPath.ToGenericUtf8() +
					"' (explicit manifest load will fail if this remains unresolved)"
				);
			}
		}
		if (options.projectsRootOverride.has_value()) {
			std::error_code ec;
			const Path&     projectsRoot = *options.projectsRootOverride;
			const bool      exists       = std::filesystem::exists(
				projectsRoot.Native(), ec
			);
			if (ec || !exists) {
				appendDiagnostic(
					"--projects-root パスは存在しないかアクセス不能です:'" +
					projectsRoot.ToGenericUtf8() +
					"' (manifest search will continue with fallback candidates)"
				);
			}
		}

		LocalFree(argv);
		return options;
	}
}
