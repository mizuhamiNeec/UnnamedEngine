#pragma once

#include <cstdint>
#include <memory>
#include <source_location>
#include <type_traits>
#include <unordered_map>

#include <core/containers/RingBuffer.h>

#include <engine/unnamed/subsystem/console/ConsoleFileLogSink.h>
#include <engine/unnamed/subsystem/console/ConsoleUI.h>
#include <engine/unnamed/subsystem/console/concommand/base/ConCommandBase.h>
#include <engine/unnamed/subsystem/console/interface/IConsole.h>
#include <engine/unnamed/subsystem/interface/ISubsystem.h>

namespace Unnamed {
	constexpr uint32_t kConsoleBufferSize = 8192; // ログバッファのサイズ

	class ConCommandBase;
	class ConCommand;
	template <typename T>
	class ConVar;

	enum class CONSOLE_SYSTEM_STATE : uint8_t {
		UNINITIALIZED,
		RUNNING,
		SHUTTING_DOWN,
		DESTROYED,
	};

	/// @brief コンソールログテキスト構造体
	struct ConsoleLogText {
		LogLevel             level;     // ログレベル
		std::string          channel;   // ログチャンネル
		std::string          message;   // ログメッセージ
		DateTime             timeStamp; // ログのタイムスタンプ
		std::source_location location;  // ソースコードの位置情報
	};

	/// @brief コンソールコマンド実行フラグ
	enum class EXEC_FLAG : uint8_t {
		NONE         = 0,      // フラグなし
		SILENT       = 1 << 0, // 実行ログを出力しない
		FROM_ENGINE  = 1 << 1, // エンジン内部からの実行(起動時の自動実行など)
		FROM_USER    = 1 << 2, // ユーザー入力からの実行(キーバインドやUIからの実行など)
		FROM_CONSOLE = 1 << 3, // コンソールからの実行(上記FROM_USERと重複するが、明示的に区別したい場合に使用)
	};

	/// @brief EXEC_FLAGのOR演算子オーバーロード
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 演算結果
	EXEC_FLAG operator|=(EXEC_FLAG& lhs, const EXEC_FLAG& rhs);

	/// @brief EXEC_FLAGのOR演算子オーバーロード
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 演算結果
	EXEC_FLAG operator|(EXEC_FLAG lhs, const EXEC_FLAG& rhs);

	/// @brief EXEC_FLAGのAND演算子オーバーロード
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 演算結果
	bool operator&(EXEC_FLAG lhs, EXEC_FLAG rhs);

	/// @brief コンソールシステムクラス
	/// @details Source風多機能コンソールシステム。文字列によるコマンド実行や変数管理を行います。
	class ConsoleSystem final : public ISubsystem, public IConsole {
	public:
		/// @brief デストラクタ
		~ConsoleSystem() override;

		// ---ISubsystem---

		/// @brief コンソールシステムの初期化
		/// @return 初期化成功ならtrue
		bool Init() override;

		/// @brief 更新
		/// @note ImGuiのコンテキスト内で呼んでください。
		void Update(float deltaTime) override;

		/// @brief シャットダウン
		void Shutdown() override;

		/// @brief サブシステム名を取得します
		/// @return サブシステム名
		[[nodiscard]] const std::string_view GetName() const override;

		// ---IConsole---

		/// @brief ログバッファを取得します
		[[nodiscard]]
		RingBuffer<ConsoleLogText, kConsoleBufferSize>& GetLogBuffer();

		/// @brief コンソールにログを出力します
		/// @details Logマクロから呼び出されます。というかLogマクロ専用です。
		/// @param level ログレベル
		/// @param channel チャンネル名
		/// @param message メッセージ
		/// @param location ソースコードの位置情報
		void Print(
			LogLevel         level, std::string_view       channel,
			std::string_view message, std::source_location location
		) override;

		/// @brief コンソールコマンドを登録します
		/// @param conCommand 登録するコマンドへのポインタ
		void RegisterConCommand(ConCommandBase* conCommand);
		void UnregisterConCommand(const ConCommandBase* conCommand) noexcept;

		/// @brief コンソール変数を登録します
		/// @param conVar 登録する変数へのポインタ
		void RegisterConVar(ConCommandBase* conVar);
		void UnregisterConVar(const ConCommandBase* conVar) noexcept;

		/// @brief コンソールにコマンドを送信します。
		/// @param command コマンド文字列
		/// @param flag フラグ
		void ExecuteCommand(
			const std::string& command,
			EXEC_FLAG          flag = EXEC_FLAG::FROM_ENGINE
		);

		/// @brief 登録されている全てのコンソール変数を取得します
		[[nodiscard]]
		std::unordered_map<std::string, ConCommandBase*> GetConVars();

		/// @brief 名前からコンソール変数を取得します
		/// @param name 変数名
		/// @return 変数へのポインタ（存在しない場合はnullptr）
		ConCommandBase* GetConVar(std::string_view name);

		/// @brief 名前からコンソールコマンドを取得します
		/// @param name コマンド名
		/// @return コマンドへのポインタ（存在しない場合はnullptr）
		ConCommandBase* GetConCommand(std::string_view name);

		/// @brief ConVarの現在値を文字列で取得します
		/// @param name 変数名
		/// @return 値文字列（存在しない/ConVarでない場合は空文字）
		[[nodiscard]] std::string GetConVarValueString(
			std::string_view name
		) const;

		/// @brief ConCommandBaseからCVAR_TYPEを取得します
		/// @param var 変数へのポインタ
		/// @return 変数の型
		[[nodiscard]]
		static enum class CVAR_TYPE GetConVarType(ConCommandBase* var);

		/// @brief 指定した型のコンソール変数を名前から取得します
		/// @tparam TVar 取得する変数の型
		/// @param name 変数名
		/// @return 変数へのポインタ（無効な場合はnullptr）
		template <class TVar>
		[[nodiscard]] TVar* GetConVarAs(const std::string_view name) {
			static_assert(
				std::is_base_of_v<ConCommandBase, TVar>,
				"TVar must be derived from ConCommandBase"
			);

			auto* base = GetConVar(name);
			if (!base) {
				return nullptr;
			}

			return dynamic_cast<TVar*>(base);
		}

		/// @brief 指定した型のコンソール変数の値を取得します。存在しない/型不一致の場合はフォールバック値を返します。
		/// @tparam T 取得する変数の型
		/// @param name 変数名
		/// @param fallback フォールバック値
		/// @return 変数の値、またはフォールバック値
		template <typename T>
		[[nodiscard]] T GetConVarValueOr(
			std::string_view name,
			const T&               fallback
		);

		/// @brief 入力テキストに基づいて曖昧検索でコンソール変数を検索します
		/// @param input 検索キーワード
		/// @param maxResults 最大結果数
		/// @return マッチしたconvar名のベクター（スコア順でソート、高スコアから低スコア）
		[[nodiscard]]
		std::vector<std::string> FindSimilarConVars(
			std::string_view input, size_t maxResults = 10
		);

		/// @brief 入力テキストに基づいて曖昧検索でコンソールコマンドを検索します
		/// @param input 検索キーワード
		/// @param maxResults 最大結果数
		/// @return マッチしたconcommand名のベクター（スコア順でソート、高スコアから低スコア）
		[[nodiscard]]
		std::vector<std::string> FindSimilarConCommands(
			std::string_view input, size_t maxResults = 10
		);

	private:
		/// @brief 一般コマンドを登録します
		void RegisterCommonCommands();

		/// @brief コマンド引数から変数に値をセットします。
		/// @param var 対象の変数
		/// @param args コマンド引数のベクター
		void SetVarFromArgs(
			ConCommandBase* var, const std::vector<std::string>& args
		);

		void DetachRegisteredConsoleObjects() noexcept;

		// ログのリングバッファ
		RingBuffer<ConsoleLogText, kConsoleBufferSize> mLogBuffer;

		// ファイル出力
		ConsoleFileLogSink mFileLogSink;
		bool               mEnableFileLog = true;

		// 登録されているコマンドと変数のマップ
		std::unordered_map<std::string, ConCommandBase*> mConCommands;
		std::unordered_map<std::string, ConCommandBase*> mConVars;
		std::unique_ptr<ConCommand>                      mHelpCommand;
		std::unique_ptr<ConCommand>                      mClearCommand;
		CONSOLE_SYSTEM_STATE                               mState =
			CONSOLE_SYSTEM_STATE::UNINITIALIZED;

#ifdef UNNAMED_WITH_EDITOR // デバッグ時(ImGui有効化時)にはコンソールUIを有効化
		std::unique_ptr<ConsoleUI> mConsoleUI;
#endif
	};
}
