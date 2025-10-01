#include <pch.h>

//-----------------------------------------------------------------------------

#include <format>
#include <ranges>

#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/Console.h>
#include <engine/Window/WindowManager.h>

//-----------------------------------------------------------------------------
// Purpose: インプットシステムの初期化を行います
//-----------------------------------------------------------------------------
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
		MessageBox(nullptr, L"Failed to register raw input devices", L"Error",
		           MB_OK);
		Console::Print("Failed to register raw input devices\n",
		               kConTextColorError, Channel::InputSystem);
	}

	ConCommand::RegisterCommand(
		"bind",
		[](const std::vector<std::string>& args) {
			if (args.size() < 2) {
				Console::Print("Usage: bind <key> <command>\n",
				               kConTextColorWarning, Channel::InputSystem);
				return;
			}
			std::string key     = args[0];
			std::string command = args[1];
			InputSystem::BindKey(key, command);
		},
		"Bind a key to a command."
	);

	ConCommand::RegisterCommand(
		"unbind",
		[](const std::vector<std::string>& args) {
			if (args.size() < 1) {
				Console::Print("Usage: unbind <key>\n", kConTextColorWarning,
				               Channel::InputSystem);
				return;
			}
			std::string key = args[0];
			InputSystem::UnbindKey(key);
		},
		"Unbind a key."
	);

	ConCommand::RegisterCommand(
		"unbindall",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			InputSystem::UnbindAll();
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
}

void InputSystem::Update() {
	CheckMouseCursorLock();

	// トリガー/リリース状態のクリア
	mTriggeredCommands.clear();
	mReleasedCommands.clear();
	for (auto& [isTriggered, isPressed] : mCommandStates | std::views::values) {
		isPressed = false; // isPressedは毎フレームリセット
	}

	// マウスの移動量をリセット
	mOuseDelta = Vec2::zero;
}

void InputSystem::ProcessInput(const long lParam) {
	UINT dwSize = 0;
	GetRawInputData(reinterpret_cast<HRAWINPUT>(static_cast<LPARAM>(lParam)),
	                RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

	auto lpb = std::make_unique<BYTE[]>(dwSize);
	if (GetRawInputData(
		reinterpret_cast<HRAWINPUT>(static_cast<LPARAM>(lParam)), RID_INPUT,
		lpb.get(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
		return;
	}

	auto* raw = reinterpret_cast<RAWINPUT*>(lpb.get());

	// キーボード入力
	if (raw->header.dwType == RIM_TYPEKEYBOARD) {
		const auto vKey      = raw->data.keyboard.VKey;
		const bool isKeyDown = !(raw->data.keyboard.Flags & RI_KEY_BREAK);

		// 仮想キーを文字列に変換
		std::string keyName = GetKeyName(vKey);
		keyName             = StrUtil::ToLowerCase(keyName);

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
		mOuseDelta.x += static_cast<float>(raw->data.mouse.lLastX);
		mOuseDelta.y += static_cast<float>(raw->data.mouse.lLastY);

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

Vec2 InputSystem::GetMouseDelta() {
	Vec2 delta = mOuseDelta;
	return delta;
}

bool InputSystem::IsTriggered(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	return mTriggeredCommands[baseCommand];
}

bool InputSystem::IsPressed(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	return mPressedCommands[baseCommand];
}

bool InputSystem::IsReleased(const std::string& command) {
	// +プレフィックスの処理
	std::string baseCommand = command;
	if (!command.empty() && command[0] == '+') {
		baseCommand = command.substr(1);
	}
	return mReleasedCommands[baseCommand];
}

void InputSystem::BindKey(const std::string& key, const std::string& command) {
	mKeyBindings[key] = command;
}

void InputSystem::UnbindKey(const std::string& key) {
	mKeyBindings.erase(key);
}

void InputSystem::UnbindAll() {
	mKeyBindings.clear();
}

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

void InputSystem::ResetAllKeys() {
	mPressedCommands.clear();
	mTriggeredCommands.clear();
	mReleasedCommands.clear();
	mCommandStates.clear();
	mOuseDelta    = Vec2::zero;
	mMouseLock    = false;
	mCursorHidden = false;
}

//-----------------------------------------------------------------------------
// Purpose: マウスカーソルの表示/非表示を切り替えます。カーソルの固定も行います。
//-----------------------------------------------------------------------------
void InputSystem::CheckMouseCursorLock() {
	static int cursorCount = 0; // カーソル表示カウンタを追跡

	if (mMouseLock) {
		// カーソルをウィンドウの中央にリセット
		const POINT centerCursorPos = {
			static_cast<LONG>(OldWindowManager::GetMainWindow()->
				GetClientWidth() /
				2),
			static_cast<LONG>(OldWindowManager::GetMainWindow()->
				GetClientHeight() /
				2)
		};

		if (OldWindowManager::GetMainWindow()->GetWindowHandle() ==
			GetForegroundWindow()) {
			RECT rect;
			rect.left   = centerCursorPos.x;
			rect.top    = centerCursorPos.y;
			rect.right  = centerCursorPos.x + 1;
			rect.bottom = centerCursorPos.y + 1;
			ClientToScreen(OldWindowManager::GetMainWindow()->GetWindowHandle(),
			               reinterpret_cast<LPPOINT>(&rect));
			ClientToScreen(OldWindowManager::GetMainWindow()->GetWindowHandle(),
			               reinterpret_cast<LPPOINT>(&rect) + 1);
			ClipCursor(&rect);
		}

		// カーソルを非表示にする
		while (cursorCount >= 0) {
			cursorCount = ShowCursor(FALSE);
		}
		mCursorHidden = true;
	} else {
		ClipCursor(nullptr); // カーソルのクリッピングを解除

		// カーソルを表示する
		while (cursorCount < 0) {
			cursorCount = ShowCursor(TRUE);
		}
		mCursorHidden = false;
	}
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

std::string InputSystem::GetKeyName(const UINT virtualKey) {
	char name[256];
	if (GetKeyNameTextA(MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC) << 16, name,
	                    sizeof(name))) {
		return std::string(name);
	}
	Console::Print(std::format("キーの名前を取得できませんでした: {}\n", virtualKey),
	               kConTextColorError, Channel::InputSystem);
	return "Unknown";
}

Vec2 InputSystem::mOuseDelta = Vec2::zero;

std::unordered_map<std::string, std::string> InputSystem::mKeyBindings;
std::unordered_map<std::string, InputSystem::CommandState>
InputSystem::mCommandStates;
std::unordered_map<std::string, bool> InputSystem::mTriggeredCommands;
std::unordered_map<std::string, bool> InputSystem::mPressedCommands;
std::unordered_map<std::string, bool> InputSystem::mReleasedCommands;
bool                                  InputSystem::mMouseLock    = false;
bool                                  InputSystem::mCursorHidden = false;
