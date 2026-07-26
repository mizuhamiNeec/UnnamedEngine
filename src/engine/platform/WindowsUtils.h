#pragma once

#include <Windows.h>

#include <string>

/// @brief Windowsユーティリティクラス
class WindowsUtils {
public:
	static std::string GetWindowsUserName();
	static std::string GetWindowsComputerName();
	static std::string GetWindowsVersion();
	static std::string GetCPUName();
	static std::string GetGPUName();
	static std::string GetRamMax();
	static std::string GetRamUsage();
	static std::string GetHresultMessage(HRESULT hr);
	static bool        RegistryGetDWord(
		void*          hKeyParent, const char* key,
		const char*    name,
		unsigned long* pData
	);
	static bool IsAppDarkTheme();
	static bool IsSystemDarkTheme();

	/// @brief ProcessResultは、子processのexit code、標準出力・標準error、timeoutとWin32起動errorを返します
	struct ProcessResult {
		unsigned long exitCode = 0;
		std::string   stdoutText;
		std::string   stderrText;
		bool          timedOut   = false;
		unsigned long win32Error = 0; // CreateProcessW/pipe 失敗時の GetLastError
	};

	/// @brief プロセスを実行し、出力をキャプチャします
	/// @param application 実行ファイルパス
	/// @param arguments 引数
	/// @param workingDirectory 作業ディレクトリ（空文字列ならカレントディレクトリ）
	/// @param timeoutMs タイムアウト時間（ミリ秒、0ならタイムアウトなし）
	/// @return 実行結果
	static ProcessResult RunProcessCapture(
		const std::wstring& application,
		const std::wstring& arguments,
		const std::wstring& workingDirectory = L"",
		unsigned long       timeoutMs        = 0
	);

	/// @brief cmd.exe コマンドを実行します
	/// @param cmd コマンドライン
	/// @param workingDirectory 作業ディレクトリ（空文字列ならカレントディレクトリ）
	/// @param timeoutMs タイムアウト時間（ミリ秒、0ならタイムアウトなし）
	/// @return 実行結果
	static ProcessResult RunCmdCapture(
		const std::wstring& cmd,
		const std::wstring& workingDirectory = L"",
		unsigned long       timeoutMs        = 0
	);

	/// @brief PowerShell スクリプトファイルを実行します
	/// @param scriptPath スクリプトファイルパス
	/// @param arguments 引数
	/// @param workingDirectory 作業ディレクトリ（空文字列ならカレントディレクトリ）
	/// @param timeoutMs タイムアウト時間（ミリ秒、0ならタイムアウトなし）
	/// @return 実行結果
	static ProcessResult RunPowerShellFileCapture(
		const std::wstring& scriptPath,
		const std::wstring& arguments        = L"",
		const std::wstring& workingDirectory = L"",
		unsigned long       timeoutMs        = 0
	);

	/// @brief ProcessHandleは、子プロセス、主スレッド、標準出力パイプのWin32ハンドルをCloseProcessHandleまで所有・管理します
	struct ProcessHandle {
		void* hProcess  = nullptr;
		void* hThread   = nullptr;
		void* hReadPipe = nullptr;
	};

	/// @brief プロセスを起動し、出力キャプチャ用のパイプをセットアップします
	/// @param application 実行ファイルパス
	/// @param arguments 引数
	/// @param outHandle プロセスハンドルの出力先
	/// @param workingDirectory 作業ディレクトリ（空文字列ならカレントディレクトリ）
	/// @return 成功したら true を返す
	static bool StartProcessCapture(
		const std::wstring& application,
		const std::wstring& arguments,
		ProcessHandle&      outHandle,
		const std::wstring& workingDirectory = L""
	);

	/// @brief プロセスの出力をポーリングし、取得できたチャンクを outChunk に格納します
	/// @param handle プロセスハンドル
	/// @param outChunk 取得したチャンクの出力先
	/// @return 取得できたら true を返す
	static bool PollProcessOutput(
		const ProcessHandle& handle, std::string& outChunk
	);

	/// @brief プロセスの終了コードを取得します
	/// @param handle プロセスハンドル
	/// @param exitCode 終了コードの出力先
	/// @return 終了コードが取得できたら true を返す
	static bool TryGetProcessExitCode(
		const ProcessHandle& handle, unsigned long& exitCode
	);

	/// @brief プロセスハンドルを閉じます
	/// @param handle プロセスハンドル
	static void CloseProcessHandle(ProcessHandle& handle);

	/// @brief CP932 文字列を UTF-8 文字列に変換します
	/// @param s CP932 文字列
	/// @return UTF-8 文字列
	static std::string ConvertCP932ToUtf8(const std::string& s);
};
