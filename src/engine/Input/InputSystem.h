#pragma once
#include <pch.h>
#include <string>
#include <unordered_map>

#include <runtime/core/math/Math.h>

/**
 * @brief 入力システムクラス
 * @details キーボードとマウスの入力を管理し、コマンドバインディング機能を提供します
 */
class InputSystem {
public:
	/**
	 * @brief 入力システムを初期化する
	 */
	static void Init();
	
	/**
	 * @brief 入力状態を更新する
	 */
	static void Update();

	/**
	 * @brief 生の入力データを処理する
	 * @param lParam Win32メッセージのlParam
	 */
	static void ProcessInput(long lParam);

	/**
	 * @brief マウスの移動量を取得する
	 * @return マウスの移動量（ピクセル）
	 */
	static Vec2 GetMouseDelta();

	/**
	 * @brief コマンドがトリガーされたかを判定する
	 * @param command コマンド名
	 * @return トリガーされた場合true
	 */
	static bool IsTriggered(const std::string& command);
	
	/**
	 * @brief コマンドが押されているかを判定する
	 * @param command コマンド名
	 * @return 押されている場合true
	 */
	static bool IsPressed(const std::string& command);
	
	/**
	 * @brief コマンドが離されたかを判定する
	 * @param command コマンド名
	 * @return 離された場合true
	 */
	static bool IsReleased(const std::string& command);

	/**
	 * @brief キーにコマンドをバインドする
	 * @param key キー名
	 * @param command コマンド名
	 */
	static void BindKey(const std::string& key, const std::string& command);
	
	/**
	 * @brief キーのバインドを解除する
	 * @param key キー名
	 */
	static void UnbindKey(const std::string& key);
	
	/**
	 * @brief すべてのバインドを解除する
	 */
	static void UnbindAll();
	
	/**
	 * @brief コマンドを実行する
	 * @param command コマンド名
	 * @param isDown キーが押された場合true、離された場合false
	 */
	static void ExecuteCommand(const std::string& command, bool isDown);

	/**
	 * @brief すべてのキー状態をリセットする
	 */
	static void ResetAllKeys();

	/**
	 * @brief マウスカーソルのロック状態を確認する
	 */
	static void CheckMouseCursorLock();

private:
	static void UpdateMouseButtonState(USHORT             buttonFlags,
	                                   const std::string& buttonName,
	                                   USHORT             buttonDownFlag,
	                                   USHORT             buttonUpFlag);

	static std::string GetKeyName(UINT virtualKey);

	static Vec2 mOuseDelta;

	struct CommandState {
		bool isTriggered = false; // トリガーされた
		bool isPressed   = false; // 押されている
	};

	static std::unordered_map<std::string, CommandState> mCommandStates;
	// コマンドの状態
	static std::unordered_map<std::string, std::string> mKeyBindings;
	// キーとコマンドのバインド
	static std::unordered_map<std::string, bool> mTriggeredCommands;
	static std::unordered_map<std::string, bool> mPressedCommands;
	static std::unordered_map<std::string, bool> mReleasedCommands;
	static bool                                  mMouseLock;    // マウスのロック
	static bool                                  mCursorHidden; // カーソルが非表示か
};
