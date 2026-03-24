#include <pch.h>

//-----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>

#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/Console.h>

#include <Xinput.h>
#pragma comment(lib, "Xinput9_1_0.lib")

//-----------------------------------------------------------------------------
// Purpose: インプットシステムの初期化を行います
//-----------------------------------------------------------------------------
/**
 * @brief 入力システムを初期化する
 */
void InputSystem::Init() {
	RAWINPUTDEVICE rid[2];

	// キーボードデバイスを登録
	rid[0].usUsagePage = 0x01;
	rid[0].usUsage     = 0x06;
	rid[0].dwFlags     = 0;
	rid[0].hwndTarget  = nullptr;

	// マウスデバイスを登録
	rid[1].usUsagePage = 0x01;
	rid[1].usUsage     = 0x02;
	rid[1].dwFlags     = 0;
	rid[1].hwndTarget  = nullptr;

	if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
		MessageBox(
			nullptr, L"Failed to register raw input devices", L"Error",
			MB_OK
		);
		Console::Print(
			"Failed to register raw input devices\n",
			kConTextColorError, Channel::InputSystem
		);
	}

	ConCommand::RegisterCommand(
		"bind",
		[](const std::vector<std::string>& args) {
			if (args.size() < 2) {
				Console::Print(
					"Usage: bind <key> <command>\n",
					kConTextColorWarning, Channel::InputSystem
				);
				return;
			}
			std::string key     = args[0];
			std::string command = args[1];
			BindKey(key, command);
		},
		"Bind a key to a command."
	);

	ConCommand::RegisterCommand(
		"unbind",
		[](const std::vector<std::string>& args) {
			if (args.size() < 1) {
				Console::Print(
					"Usage: unbind <key>\n", kConTextColorWarning,
					Channel::InputSystem
				);
				return;
			}
			std::string key = args[0];
			UnbindKey(key);
		},
		"Unbind a key."
	);

	ConCommand::RegisterCommand(
		"unbindall",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			UnbindAll();
		},
		"Unbind all keys."
	);

	ConCommand::RegisterCommand(
		"togglelockcursor",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			mMouseLock = !mMouseLock;
		},
		"Toggle lock cursor."
	);

	mMouseLock = false;

	// タイマー初期化
	mLastUpdateTime = std::chrono::steady_clock::now();
}

/**
 * @brief 入力状態を更新する
 */
void InputSystem::Update() {
	// デルタタイム計算
	auto  now = std::chrono::steady_clock::now();
	float dt = std::chrono::duration<float>(now - mLastUpdateTime).count();
	mLastUpdateTime = now;

	// トリガー/リリース状態のクリア
	mTriggeredCommands.clear();
	mReleasedCommands.clear();
	for (auto& [isTriggered, isPressed] : mCommandStates | std::views::values) {
		isPressed = false; // isPressedは毎フレームリセット
	}

	// マウスの移動量をリセット
	mMouseDelta = Vec2::zero;

	// コントローラー入力の更新
	for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		if (XInputGetState(i, &state) == ERROR_SUCCESS) {
			mGamepadStates[i].isConnected = true;
			WORD oldButtons               = mGamepadStates[i].buttons;
			WORD newButtons               = state.Gamepad.wButtons;
			mGamepadStates[i].buttons     = newButtons;

			// デッドゾーン処理と正規化
			auto ApplyDeadzone =
				[](SHORT value, SHORT deadzone) -> float {
				if (std::abs(value) < deadzone) { return 0.0f; }
				return (value > 0 ? value - deadzone : value + deadzone) /
				       (32767.0f - deadzone);
			};

			mGamepadStates[i].leftStickX = ApplyDeadzone(
				state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
			);
			mGamepadStates[i].leftStickY = ApplyDeadzone(
				state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
			);
			mGamepadStates[i].rightStickX = ApplyDeadzone(
				state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
			);
			mGamepadStates[i].rightStickY = ApplyDeadzone(
				state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
			);

			mGamepadStates[i].leftTrigger =
				state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ?
					state.Gamepad.bLeftTrigger / 255.0f :
					0.0f;
			mGamepadStates[i].rightTrigger =
				state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ?
					state.Gamepad.bRightTrigger / 255.0f :
					0.0f;

			// ボタン入力の更新
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_DPAD_UP, "gp_dpad_up"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_DPAD_DOWN, "gp_dpad_down"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_DPAD_LEFT, "gp_dpad_left"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_DPAD_RIGHT,
				"gp_dpad_right"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_START, "gp_start"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_BACK, "gp_back"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_LEFT_THUMB,
				"gp_left_thumb"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_RIGHT_THUMB,
				"gp_right_thumb"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
				"gp_left_shoulder"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
				"gp_right_shoulder"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_A, "gp_a"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_B, "gp_b"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_X, "gp_x"
			);
			UpdateGamepadButtonState(
				oldButtons, newButtons, XINPUT_GAMEPAD_Y, "gp_y"
			);
		} else { mGamepadStates[i].isConnected = false; }
	}

	// 振動（同時再生・加算方式）
	for (int i = 0; i < XUSER_MAX_COUNT; ++i) {
		float totalLeft  = 0.0f;
		float totalRight = 0.0f;

		// 有効な振動エフェクトを走査して加算
		for (auto it = mActiveVibrations[i].begin();
		     it != mActiveVibrations[i].end();) {
			
			// 時間経過 (duration < 0 は無限再生扱いにする場合はここを調整するが
			// 今回は SetVibration で FLT_MAX をセットする運用とする)
			it->duration -= dt;

			if (it->duration <= 0.0f) {
				it = mActiveVibrations[i].erase(it);
			} else {
				totalLeft  += it->leftMotor;
				totalRight += it->rightMotor;
				++it;
			}
		}

		// 値をクランプして適用
		XINPUT_VIBRATION vibration;
		ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));
		vibration.wLeftMotorSpeed = static_cast<WORD>(
			std::clamp(totalLeft, 0.0f, 1.0f) * 65535
		);
		vibration.wRightMotorSpeed = static_cast<WORD>(
			std::clamp(totalRight, 0.0f, 1.0f) * 65535
		);
		XInputSetState(i, &vibration);
	}
}

/**
 * @brief 生の入力データを処理する
 * @param lParam Win32メッセージのlParam
 */
void InputSystem::ProcessInput(const long lParam) {
	if (mReplayInputActive) { return; }

	UINT dwSize = 0;
	GetRawInputData(
		reinterpret_cast<HRAWINPUT>(static_cast<LPARAM>(lParam)),
		RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER)
	);

	auto lpb = std::make_unique<BYTE[]>(dwSize);
	if (GetRawInputData(
		    reinterpret_cast<HRAWINPUT>(static_cast<LPARAM>(lParam)), RID_INPUT,
		    lpb.get(), &dwSize, sizeof(RAWINPUTHEADER)
	    ) != dwSize) { return; }

	const auto* raw = reinterpret_cast<RAWINPUT*>(lpb.get());

	// キーボード入力
	if (raw->header.dwType == RIM_TYPEKEYBOARD) {
		const auto vKey      = raw->data.keyboard.VKey;
		const bool isKeyDown = !(raw->data.keyboard.Flags & RI_KEY_BREAK);

		// 仮想キーを文字列に変換
		std::string keyName = GetKeyName(vKey);
		keyName             = Unnamed::StrUtil::ToLowerCase(keyName);

		if (!keyName.empty() && mKeyBindings.contains(keyName)) {
			std::string cmd = mKeyBindings[keyName];
			if (cmd[0] == '+') {
				std::string baseCmd = cmd.substr(1);

				if (isKeyDown) {
					// すでに押下中でない場合のみ triggered に追加
					if (!mPressedCommands[baseCmd]) {
						mTriggeredCommands[baseCmd] = true;
						mPressedCommands[baseCmd]   = true;
					}
				} else {
					// キーリリース時の処理
					mPressedCommands[baseCmd]  = false;
					mReleasedCommands[baseCmd] = true;
				}
			} else {
				// プレフィックスなしコマンドは押した瞬間のみ実行
				if (isKeyDown) {
					mTriggeredCommands[cmd] = true;
					Console::SubmitCommand(cmd);
				}
			}
		}
	}
	// マウス入力
	else if (raw->header.dwType == RIM_TYPEMOUSE) {
		// マウスの移動量を更新
		mMouseDelta.x += static_cast<float>(raw->data.mouse.lLastX);
		mMouseDelta.y += static_cast<float>(raw->data.mouse.lLastY);

		// マウスの位置を更新
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		//ScreenToClient(OldWindowManager::GetMainWindow()->GetWindowHandle(), &cursorPos);
		mMousePosition.x = static_cast<float>(cursorPos.x);
		mMousePosition.y = static_cast<float>(cursorPos.y);

		// マウスボタンの状態を更新
		// 左クリック
		UpdateMouseButtonState(
			raw->data.mouse.usButtonFlags,
			"mouse1",
			RI_MOUSE_LEFT_BUTTON_DOWN,
			RI_MOUSE_LEFT_BUTTON_UP
		);
		// 右クリック
		UpdateMouseButtonState(
			raw->data.mouse.usButtonFlags,
			"mouse2",
			RI_MOUSE_RIGHT_BUTTON_DOWN,
			RI_MOUSE_RIGHT_BUTTON_UP
		);
		// ホイールクリック
		UpdateMouseButtonState(
			raw->data.mouse.usButtonFlags,
			"mouse3",
			RI_MOUSE_MIDDLE_BUTTON_DOWN,
			RI_MOUSE_MIDDLE_BUTTON_UP
		);
		// サイドボタン
		UpdateMouseButtonState(
			raw->data.mouse.usButtonFlags,
			"mouse4",
			RI_MOUSE_BUTTON_4_DOWN,
			RI_MOUSE_BUTTON_4_UP
		);
		// サイドボタン
		UpdateMouseButtonState(
			raw->data.mouse.usButtonFlags,
			"mouse5",
			RI_MOUSE_BUTTON_5_DOWN,
			RI_MOUSE_BUTTON_5_UP
		);

		// マウスホイール
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
			short wheelDelta = static_cast<short>(raw->data.mouse.usButtonData);
			if (wheelDelta > 0) {
				ExecuteCommand(mKeyBindings["mousewheelup"], true);
				ExecuteCommand(mKeyBindings["mousewheelup"], false);
			} else if (wheelDelta < 0) {
				ExecuteCommand(mKeyBindings["mousewheeldown"], true);
				ExecuteCommand(mKeyBindings["mousewheeldown"], false);
			}
		}
	}
}

/**
 * @brief マウスの移動量を取得する
 * @return マウスの移動量（ピクセル）
 */
Vec2 InputSystem::GetMouseDelta() {
	if (mReplayInputActive) { return mReplayInputState.mouseDelta; }
	return mMouseDelta;
}

/// @brief マウスの位置を取得する
/// @return マウスの位置（ピクセル）
Vec2 InputSystem::GetMousePosition() { return mMousePosition; }

/**
 * @brief コマンドがトリガーされたかを判定する
 * @param command コマンド名
 * @return トリガーされた場合true
 */
bool InputSystem::IsTriggered(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	if (mReplayInputActive) {
		if (const auto it = mReplayInputState.triggered.find(baseCommand);
			it != mReplayInputState.triggered.end()) {
			return it->second;
		}
		return false;
	}
	return mTriggeredCommands[baseCommand];
}

/**
 * @brief コマンドが押されているかを判定する
 * @param command コマンド名
 * @return 押されている場合true
 */
bool InputSystem::IsPressed(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	if (mReplayInputActive) {
		if (const auto it = mReplayInputState.pressed.find(baseCommand);
			it != mReplayInputState.pressed.end()) {
			return it->second;
		}
		return false;
	}
	return mPressedCommands[baseCommand];
}

/**
 * @brief コマンドが離されたかを判定する
 * @param command コマンド名
 * @return 離された場合true
 */
bool InputSystem::IsReleased(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	if (mReplayInputActive) {
		if (const auto it = mReplayInputState.released.find(baseCommand);
			it != mReplayInputState.released.end()) {
			return it->second;
		}
		return false;
	}
	return mReleasedCommands[baseCommand];
}

/**
 * @brief キーにコマンドをバインドする
 * @param key キー名
 * @param command コマンド名
 */
void InputSystem::BindKey(const std::string& key, const std::string& command) {
	mKeyBindings[key] = command;
}

/**
 * @brief キーのバインドを解除する
 * @param key キー名
 */
void InputSystem::UnbindKey(const std::string& key) { mKeyBindings.erase(key); }

/**
 * @brief すべてのバインドを解除する
 */
void InputSystem::UnbindAll() { mKeyBindings.clear(); }

/**
 * @brief コマンドを実行する
 * @param command コマンド名
 * @param isDown キーが押された場合true、離された場合false
 */
void InputSystem::ExecuteCommand(const std::string& command, bool isDown) {
	// +/- プレフィックスの解析
	std::string baseCommand   = command;
	bool        isPlusCommand = false;

	if (!command.empty()) {
		if (command[0] == '+') {
			isPlusCommand = true;
			baseCommand   = command.substr(1);
		} else if (command[0] == '-') {
			isPlusCommand = false;
			baseCommand   = command.substr(1);
		}
	}

	// コマンドの状態更新
	if (isPlusCommand) {
		// +プレフィックス付きコマンドは長押し状態を管理
		if (isDown) {
			mPressedCommands[baseCommand]   = true;
			mTriggeredCommands[baseCommand] = true;
		} else {
			mPressedCommands[baseCommand]  = false;
			mReleasedCommands[baseCommand] = true;
		}
	} else {
		// プレフィックスなしコマンドは押した瞬間のみ実行
		if (isDown) {
			mTriggeredCommands[baseCommand] = true;
			// コンソールコマンドも実行
			Console::SubmitCommand(baseCommand);
		}
	}
}

/**
 * @brief すべてのキー状態をリセットする
 */
void InputSystem::ResetAllKeys() {
	mPressedCommands.clear();
	mTriggeredCommands.clear();
	mReleasedCommands.clear();
	mCommandStates.clear();
	mMouseDelta   = Vec2::zero;
	mMouseLock    = false;
	mCursorHidden = false;
}

/**
 * @brief マウスカーソルのロック状態を確認する
 */
void InputSystem::CheckMouseCursorLock(HWND hwnd, int32_t x, int32_t y) {
	if (mMouseLock) {
		// カーソルを引数の位置に移動
		const POINT centerCursorPos = {
			static_cast<LONG>(x),
			static_cast<LONG>(y)
		};

		if (hwnd == GetForegroundWindow()) {
			RECT rect;
			rect.left   = centerCursorPos.x;
			rect.top    = centerCursorPos.y;
			rect.right  = centerCursorPos.x + 1;
			rect.bottom = centerCursorPos.y + 1;
			ClientToScreen(hwnd, reinterpret_cast<LPPOINT>(&rect));
			ClientToScreen(hwnd, reinterpret_cast<LPPOINT>(&rect) + 1);
			ClipCursor(&rect);
		}

		// カーソルを非表示にする
		if (!mCursorHidden) {
			int count  = ShowCursor(FALSE);
			int safety = 0;
			while (count >= 0 && safety < 100) {
				count = ShowCursor(FALSE);
				safety++;
			}
			mCursorHidden = true;
		}
	} else {
		ClipCursor(nullptr); // カーソルのクリッピングを解除

		// カーソルを表示する
		if (mCursorHidden) {
			int count  = ShowCursor(TRUE);
			int safety = 0;
			while (count < 0 && safety < 100) {
				count = ShowCursor(TRUE);
				safety++;
			}
			mCursorHidden = false;
		}
	}
}

void InputSystem::SetReplayInputState(const ReplayInputState& state) {
	mReplayInputState  = state;
	mReplayInputActive = true;
}

void InputSystem::ClearReplayInputState() {
	mReplayInputState  = ReplayInputState{};
	mReplayInputActive = false;
}

/**
 * @brief コントローラーの振動を追加する（同時再生・加算）
 * @param padIndex コントローラーインデックス (0-3)
 * @param leftMotor 左モーターの強度 (0.0 - 1.0)
 * @param rightMotor 右モーターの強度 (0.0 - 1.0)
 * @param duration 振動時間 (秒)
 */
void InputSystem::AddVibration(
	int padIndex, float leftMotor, float rightMotor, float duration
) {
	if (padIndex < 0 || padIndex >= XUSER_MAX_COUNT) { return; }
	if (!mGamepadStates[padIndex].isConnected) { return; }

	VibrationEffect effect;
	effect.leftMotor  = leftMotor;
	effect.rightMotor = rightMotor;
	effect.duration   = duration;
	mActiveVibrations[padIndex].push_back(effect);
}

/**
 * @brief コントローラーの振動を設定する (即時反映・他を停止)
 * @param padIndex コントローラーインデックス (0-3)
 * @param leftMotor 左モーターの強度 (0.0 - 1.0)
 * @param rightMotor 右モーターの強度 (0.0 - 1.0)
 */
void InputSystem::SetVibration(
	int padIndex, float leftMotor, float rightMotor
) {
	if (padIndex < 0 || padIndex >= XUSER_MAX_COUNT) { return; }
	if (!mGamepadStates[padIndex].isConnected) { return; }

	// リストをクリアして強制設定（無限時間のエフェクトとして追加）
	mActiveVibrations[padIndex].clear();

	if (leftMotor > 0.0f || rightMotor > 0.0f) {
		VibrationEffect effect;
		effect.leftMotor  = leftMotor;
		effect.rightMotor = rightMotor;
		effect.duration   = FLT_MAX; // 無限
		mActiveVibrations[padIndex].push_back(effect);
	}
	// 0,0 の場合はクリアされた時点で停止扱いになるので追加不要
}

Vec2 InputSystem::GetLeftStick(int padIndex) {
	if (mReplayInputActive) { return mReplayInputState.leftStick; }
	if (padIndex < 0 || padIndex >= XUSER_MAX_COUNT) { return Vec2::zero; }
	return Vec2(
		mGamepadStates[padIndex].leftStickX, mGamepadStates[padIndex].leftStickY
	);
}

Vec2 InputSystem::GetRightStick(int padIndex) {
	if (mReplayInputActive) { return mReplayInputState.rightStick; }
	if (padIndex < 0 || padIndex >= XUSER_MAX_COUNT) { return Vec2::zero; }
	return Vec2(
		mGamepadStates[padIndex].rightStickX,
		mGamepadStates[padIndex].rightStickY
	);
}

float InputSystem::GetLeftTrigger(int padIndex) {
	if (mReplayInputActive) { return mReplayInputState.leftTrigger; }
	if (padIndex < 0 || padIndex >= XUSER_MAX_COUNT) { return 0.0f; }
	return mGamepadStates[padIndex].leftTrigger;
}

float InputSystem::GetRightTrigger(int padIndex) {
	if (mReplayInputActive) { return mReplayInputState.rightTrigger; }
	if (padIndex < 0 || padIndex >= XUSER_MAX_COUNT) { return 0.0f; }
	return mGamepadStates[padIndex].rightTrigger;
}

void InputSystem::UpdateMouseButtonState(
	const USHORT buttonFlags, const std::string& buttonName,
	const USHORT buttonDownFlag, const USHORT    buttonUpFlag
) {
	if (buttonFlags & buttonDownFlag) {
		if (mKeyBindings.contains(buttonName)) {
			const std::string cmd = mKeyBindings[buttonName];
			if (cmd[0] == '+') {
				std::string baseCmd         = cmd.substr(1);
				mTriggeredCommands[baseCmd] = true;
				mPressedCommands[baseCmd]   = true; // 長押し状態を設定
			}
		}
	}

	if (buttonFlags & buttonUpFlag) {
		if (mKeyBindings.contains(buttonName)) {
			const std::string cmd = mKeyBindings[buttonName];
			if (cmd[0] == '+') {
				std::string baseCmd        = cmd.substr(1);
				mPressedCommands[baseCmd]  = false; // 長押し状態を解除
				mReleasedCommands[baseCmd] = true;
			}
		}
	}
}

void InputSystem::UpdateGamepadButtonState(
	WORD               oldButtons, WORD newButtons, WORD targetFlag,
	const std::string& buttonName
) {
	bool wasPressed = (oldButtons & targetFlag) != 0;
	bool isPressed  = (newButtons & targetFlag) != 0;

	// 状態変化がない場合は何もしない (キーボードの状態を保護)
	if (wasPressed == isPressed) { return; }

	if (mKeyBindings.contains(buttonName)) {
		const std::string cmd = mKeyBindings[buttonName];
		if (cmd[0] == '+') {
			std::string baseCmd = cmd.substr(1);
			if (isPressed) {
				mTriggeredCommands[baseCmd] = true;
				mPressedCommands[baseCmd]   = true;
			} else {
				// ここでは「最後に操作したデバイス」が優先される挙動になる
				// キーボードを押しながらコントローラーAを押して、離すと解除される
				mPressedCommands[baseCmd]  = false;
				mReleasedCommands[baseCmd] = true;
			}
		} else {
			// プレフィックスなしコマンド (押した瞬間のみ)
			if (isPressed) {
				mTriggeredCommands[cmd] = true;
				Console::SubmitCommand(cmd);
			}
		}
	}
}

std::string InputSystem::GetKeyName(const UINT virtualKey) {
	char name[256];
	if (GetKeyNameTextA(
		MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC) << 16, name,
		sizeof(name)
	)) { return std::string(name); }
	Console::Print(
		std::format("キーの名前を取得できませんでした: {}\n", virtualKey),
		kConTextColorError, Channel::InputSystem
	);
	return "Unknown";
}

Vec2 InputSystem::mMouseDelta    = Vec2::zero;
Vec2 InputSystem::mMousePosition = Vec2::zero;

std::unordered_map<std::string, InputSystem::CommandState>
InputSystem::mCommandStates;
std::unordered_map<std::string, std::string> InputSystem::mKeyBindings;
std::unordered_map<std::string, bool>        InputSystem::mTriggeredCommands;
std::unordered_map<std::string, bool>        InputSystem::mPressedCommands;
std::unordered_map<std::string, bool>        InputSystem::mReleasedCommands;
InputSystem::GamepadState                    InputSystem::mGamepadStates[4];
std::vector<InputSystem::VibrationEffect>    InputSystem::mActiveVibrations[4];
std::chrono::steady_clock::time_point        InputSystem::mLastUpdateTime;
bool                                         InputSystem::mMouseLock    = false;
bool                                         InputSystem::mCursorHidden = false;
bool InputSystem::mReplayInputActive = false;
InputSystem::ReplayInputState InputSystem::mReplayInputState = {};
