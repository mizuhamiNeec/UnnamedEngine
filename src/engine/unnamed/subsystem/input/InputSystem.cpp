#include <algorithm>

#include <pch.h>
#include <ranges>

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/ConCommand.h>
#include <engine/unnamed/subsystem/input/InputSystem.h>
#include <engine/unnamed/subsystem/input/KeyNameTable.h>
#include <engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h>
#include <engine/unnamed/subsystem/input/device/keyboard/KeyboardDevice.h>
#include <engine/unnamed/subsystem/input/device/mouse/MouseDevice.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

#include <Dbt.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "Input";

	namespace {
		constexpr int MouseButtonIndexFromVirtualKey(const uint32_t vk) {
			switch (vk) {
				case VM_1: return 0;
				case VM_2: return 1;
				case VM_3: return 2;
				case VM_4: return 3;
				case VM_5: return 4;
				default: return -1;
			}
		}

		/// @brief 指定されたウィンドウが実際にアクティブかどうかを判定します
		/// @param hWnd ウィンドウハンドル
		/// @return アクティブであればtrue
		bool IsWindowActuallyActive(const HWND hWnd) {
			if (!hWnd) {
				return false;
			}
			const HWND foreground = GetForegroundWindow();
			return foreground == hWnd;
		}
	}

	InputSystem::~InputSystem() = default;

	/// @brief 初期化
	bool InputSystem::Init() {
		ServiceLocator::Register<InputSystem>(this);

		// 起動直後に何も触っていなくてもカーソルが消えるのを防ぐため、
		// 入力システム側のデフォルトは「表示・ロック解除」に寄せておく。
		mMouseCursorLockRequested    = false;
		mMouseCursorLockOverride     = false;
		mMouseCursorLockOverrideMode = false;
		mMouseCursorVisible          = true;

		mBindCommand = std::make_unique<ConCommand>(
			"bind",
			[&](const std::vector<std::string>& args) {
				// bind <key> <command...>
				if (args.size() < 2) {
					return false;
				}

				const auto key = KeyNameTable::FromString(args[0]);
				if (!key.has_value()) {
					return false;
				}

				// コマンド部分を結合
				std::string combined;
				for (size_t i = 1; i < args.size(); ++i) {
					combined += args[i];
					if (i + 1 < args.size()) {
						combined += " ";
					}
				}

				BindCommand(
					{
						.device = key->device,
						.code   = key->code
					},
					combined
				);
				return true;
			},
			"Bind a key to a command. Usage: bind <key> <command>"
		);

		mUnbindCommand = std::make_unique<ConCommand>(
			"unbind",
			[&](const std::vector<std::string>& args) {
				// unbind <key>
				if (args.empty()) {
					return false;
				}
				const auto key = KeyNameTable::FromString(args[0]);
				if (!key.has_value()) {
					return false;
				}
				UnbindCommand({.device = key->device, .code = key->code});
				return true;
			},
			"Unbind a key from a command. Usage: unbind <key>"
		);

		mUnbindAllCommand = std::make_unique<ConCommand>(
			"unbindall",
			[&](const std::vector<std::string>&) {
				UnbindAllCommands();
				return true;
			},
			"Unbind all keys."
		);

		auto ToggleCursorLockHandler = [&](const std::vector<std::string>&) {
			const bool bNewState = !IsMouseCursorLocked();
			if (bNewState == mMouseCursorLockRequested) {
				mMouseCursorLockOverride = false;
			} else {
				mMouseCursorLockOverride     = true;
				mMouseCursorLockOverrideMode = bNewState;
			}
			// ロック状態に合わせて表示も同期する。
			SetMouseCursorVisible(!bNewState);
			if (!bNewState) {
				ClearMouseCursorLockAnchor();
			}
			DevMsg(
				kChannel,
				"Cursor lock state changed: {}",
				bNewState ? "Locked" : "Unlocked"
			);
			return true;
		};

		mToggleLockCursorCommand = std::make_unique<ConCommand>(
			"togglelockcursor",
			ToggleCursorLockHandler,
			"Toggle the cursor lock state."
		);

		mToggleCursorLockCommand = std::make_unique<ConCommand>(
			"togglecursorlock",
			ToggleCursorLockHandler,
			"Toggle the cursor lock state."
		);

		return true;
	}

	/// @brief 更新
	void InputSystem::Update(float) {
		// このフレームのアクションと軸を同じデバイス状態から組み立てる
		for (const auto& inputDevice : mDevices) {
			inputDevice->Update();
		}

		mMouseButtonPressed.fill(false);
		mMouseButtonReleased.fill(false);

		HWND primaryMouseWindow = nullptr;
		for (const auto& device : mDevices) {
			if (device->GetDeviceType() != InputDeviceType::MOUSE) {
				continue;
			}

			const auto mouseDevice = std::static_pointer_cast<MouseDevice>(
				device
			);
			primaryMouseWindow = mouseDevice->GetWindowHandle();

			constexpr std::array<uint32_t, 5> mouseButtons = {
				VM_1,
				VM_2,
				VM_3,
				VM_4,
				VM_5,
			};

			for (const uint32_t vk : mouseButtons) {
				const int index = MouseButtonIndexFromVirtualKey(vk);
				if (index < 0) {
					continue;
				}
				const InputKey key{
					.device = InputDeviceType::MOUSE,
					.code   = vk
				};
				const bool down             = mouseDevice->GetKeyState(key);
				mMouseButtonPressed[index]  = !mMouseButtonDown[index] && down;
				mMouseButtonReleased[index] = mMouseButtonDown[index] && !down;
				mMouseButtonDown[index]     = down;
			}
			break;
		}

		if (primaryMouseWindow != nullptr) {
			POINT cursorPos = {};
			if (GetCursorPos(&cursorPos) != 0) {
				POINT clientPos = cursorPos;
				ScreenToClient(primaryMouseWindow, &clientPos);
				mMouseClientPosition = Vec2(
					static_cast<float>(clientPos.x),
					static_cast<float>(clientPos.y)
				);
			}

			RECT clientRect = {};
			if (GetClientRect(primaryMouseWindow, &clientRect) != 0) {
				mMouseClientViewportSize = Vec2(
					static_cast<float>(clientRect.right - clientRect.left),
					static_cast<float>(clientRect.bottom - clientRect.top)
				);
			}
		}

		// アクション状態をリセット
		for (auto& state : mActionStates | std::views::values) {
			state.bIsPressed  = false;
			state.bIsReleased = false;
		}

		// コマンドバインドの処理
		ProcessCommandBinds();

		//---------------------------------------------------------------------
		// 軸入力は毎フレームリセットする
		//---------------------------------------------------------------------
		for (auto& [value] : mAxisStates1D | std::views::values) {
			value = 0.0f;
		}

		for (auto& [value] : mAxisStates2D | std::views::values) {
			value = Vec2::zero;
		}

		// アクション状態を一時的に保存するマップ
		std::unordered_map<std::string, std::tuple<bool, bool, bool>>
			tempActionStates;

		// <axis名, <正の値, 負の値>>
		std::unordered_map<std::string, std::pair<float, float>> tempAxis1D;
		// <axis名, <正のVec2, 負のVec2>>
		std::unordered_map<std::string, std::pair<Vec2, Vec2>> tempAxis2D;

		// すべての入力を処理して一時マップに値を蓄積
		for (const auto& [key, type, target, scale, scale2D, axis] :
		     mBindings) {
			const bool  bIsDown     = GetHardwareKeyState(key);
			const float analogValue = GetAnalogValue(key);

			const bool bWasDown    = mPreviousKeyStates[key];
			mCurrentKeyStates[key] = bIsDown;

			if (type == BINDING_TYPE::ACTION) {
				// このキーの状態を計算
				bool bIsPressed  = !bWasDown && bIsDown; // 新規に押された
				bool bIsReleased = bWasDown && !bIsDown; // 離された
				bool bIsHeld     = bIsDown;

				// 一時マップに存在しなければ初期化
				if (!tempActionStates.contains(target)) {
					tempActionStates[target] = std::make_tuple(
						false, false, false
					);
				}

				// 状態を論理ORで更新
				auto& [existing_pressed, existing_held, existing_released] =
					tempActionStates[target];
				existing_pressed  = existing_pressed || bIsPressed;
				existing_held     = existing_held || bIsHeld;
				existing_released = existing_released || bIsReleased;
			}

			if (type == BINDING_TYPE::AXIS_1D) {
				float scaledValue          = analogValue * scale;
				auto& [positive, negative] = tempAxis1D[target];

				if (scaledValue > 0) {
					positive = std::max(positive, scaledValue);
				} else if (scaledValue < 0) {
					negative = std::min(negative, scaledValue);
				}
			}

			if (type == BINDING_TYPE::AXIS_2D) {
				auto& [positive, negative] = tempAxis2D[target];

				if (axis == INPUT_AXIS::X) {
					float scaledValue = analogValue * scale2D.x;
					if (scaledValue > 0) {
						positive.x = std::max(positive.x, scaledValue);
					} else if (scaledValue < 0) {
						negative.x = std::min(negative.x, scaledValue);
					}
				} else {
					float scaledValue = analogValue * scale2D.y;
					if (scaledValue > 0) {
						positive.y = std::max(positive.y, scaledValue);
					} else if (scaledValue < 0) {
						negative.y = std::min(negative.y, scaledValue);
					}
				}
			}
		}

		// 一時マップから最終アクション状態を更新（デバイス入力分）
		for (const auto& [action, states] : tempActionStates) {
			mActionStates[action].bIsPressed =
				mActionStates[action].bIsPressed || std::get<0>(states);
			mActionStates[action].bIsHeld =
				mActionStates[action].bIsHeld || std::get<1>(states);
			mActionStates[action].bIsReleased =
				mActionStates[action].bIsReleased || std::get<2>(states);
		}

		// 一時マップから最終的な値を計算
		for (const auto& [axisName, values] : tempAxis1D) {
			auto& [value] = mAxisStates1D[axisName];
			value         = values.first + values.second; // 正の値と負の値を合計
		}

		for (const auto& [axisName, values] : tempAxis2D) {
			auto& [value] = mAxisStates2D[axisName];
			value.x       = values.first.x + values.second.x;
			value.y       = values.first.y + values.second.y;
		}

		// 前フレームの状態を保存
		mPreviousKeyStates = mCurrentKeyStates;

		//マウスのデルタをリセット
		for (const auto& device : mDevices) {
			if (device->GetDeviceType() == InputDeviceType::MOUSE) {
				const auto mouse = std::static_pointer_cast<
					MouseDevice>(device);
				mouse->ResetDelta();
			}
		}
	}

	void InputSystem::Shutdown() {
		mBindCommand.reset();
		mUnbindCommand.reset();
		mUnbindAllCommand.reset();
		mToggleLockCursorCommand.reset();
		mToggleCursorLockCommand.reset();
	}

	/// @brief サブシステム名の取得
	const std::string_view InputSystem::GetName() const {
		return "InputSystem";
	}

	/// @brief Win32メッセージの処理
	/// @param hWnd ウィンドウハンドル
	/// @param msg メッセージID
	/// @param wParam メッセージのWPARAM
	/// @param lParam メッセージのLPARAM
	/// @return メッセージが処理された場合はtrue、そうでない場合はfalse
	bool InputSystem::OnWin32Message(
		[[maybe_unused]] HWND hWnd,
		const UINT            msg,
		const WPARAM          wParam,
		const LPARAM          lParam
	) {
		switch (msg) {
			case WM_ACTIVATE:
				// ウィンドウが非アクティブになったら入力をリセット
				if (LOWORD(wParam) == WA_INACTIVE) {
					ResetInputStates();
					// 非アクティブ時はカーソルクリップを解除
					ClipCursor(nullptr);
				}
				break;

			case WM_KILLFOCUS:
				// フォーカス喪失も解除（Alt+Tab、Winキー時）
				ResetInputStates();
				ClipCursor(nullptr);
				break;

			case WM_INPUT: {
				// RAW Inputを受け取る
				UINT size = 0;
				GetRawInputData(
					reinterpret_cast<HRAWINPUT>(lParam), // NOLINT(performance-no-int-to-ptr)
					RID_INPUT,
					nullptr,
					&size,
					sizeof(RAWINPUTHEADER)
				);

				std::vector<uint8_t> buffer(size);

				if (
					GetRawInputData(
						reinterpret_cast<HRAWINPUT>(lParam), // NOLINT(performance-no-int-to-ptr)
						RID_INPUT,
						buffer.data(),
						&size,
						sizeof(RAWINPUTHEADER)
					) == size
				) {
					const RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.
						data());
					OnRawInput(*raw);
				}
				break;
			}

			case WM_DEVICECHANGE: {
				if (
					wParam == DBT_DEVICEARRIVAL ||
					wParam == DBT_DEVICEREMOVECOMPLETE ||
					wParam == DBT_DEVNODES_CHANGED
				) {
					for (const auto& device : mDevices) {
						if (device->GetDeviceType() !=
						    InputDeviceType::GAMEPAD) {
							continue;
						}

						std::static_pointer_cast<GamepadDevice>(device)
							->RequestDirectInputRefresh();
					}
				}
				break;
			}

			default: break;
		}

		return false;
	}

	void InputSystem::CheckMouseCursorLockAndVisibility(
		const HWND hWnd
	) const {
		static int  cursorCount        = 0;
		static bool sLastCursorVisible = true;

		const bool bActive        = IsWindowActuallyActive(hWnd);
		const bool bCursorLocked  = ResolveMouseCursorLockState();
		const bool bCursorVisible = ResolveMouseCursorVisibleState();

		// 非アクティブ中は OS 側の挙動や他 UI による ShowCursor 操作と競合しやすいので、
		// カーソルは必ず「表示」に寄せる。
		if (!bActive && !bCursorVisible) {
			// const 関数のため、表示状態そのものは変えずに OS 側だけ可視へ収束させる
			sLastCursorVisible          = true;
			constexpr int maxIterations = 32;
			int           it            = 0;
			while (cursorCount < 0 && it++ < maxIterations) {
				cursorCount = ShowCursor(TRUE);
			}
		}

		// マウスカーソルのロックと表示状態の更新
		if (bCursorLocked) {
			// 非アクティブ時は必ずロック解除
			if (!bActive) {
				ClipCursor(nullptr);
			} else {
				const HWND anchorHwnd = mMouseLockAnchor.valid &&
				                        mMouseLockAnchor.hwnd != nullptr ?
					                        mMouseLockAnchor.hwnd :
					                        hWnd;
				POINT lockPos;
				if (mMouseLockAnchor.valid) {
					lockPos.x = static_cast<LONG>(mMouseLockAnchor.clientPos.x);
					lockPos.y = static_cast<LONG>(mMouseLockAnchor.clientPos.y);
				} else {
					RECT clientRect = {};
					GetClientRect(anchorHwnd, &clientRect);
					lockPos.x = (clientRect.right - clientRect.left) / 2;
					lockPos.y = (clientRect.bottom - clientRect.top) / 2;
				}
				ClientToScreen(anchorHwnd, &lockPos);

				// ウィンドウがアクティブな場合のみロック
				if (bActive) {
					RECT rect;
					rect.left   = lockPos.x;
					rect.top    = lockPos.y;
					rect.right  = lockPos.x + 1;
					rect.bottom = lockPos.y + 1;
					ClipCursor(&rect); // ClipCursorのほうがガタガタしない
					SetCursorPos(lockPos.x, lockPos.y);
				}
			}
		} else {
			// カーソルロック解除
			ClipCursor(nullptr);
		}

		if (bCursorVisible != sLastCursorVisible) {
			sLastCursorVisible = bCursorVisible;

			// ShowCursor の戻り値: 0 以上なら表示、0 未満なら非表示
			constexpr int maxIterations = 32;
			int           it            = 0;

			if (bCursorVisible) {
				while (cursorCount < 0 && it++ < maxIterations) {
					cursorCount = ShowCursor(TRUE);
				}
			} else {
				while (cursorCount >= 0 && it++ < maxIterations) {
					cursorCount = ShowCursor(FALSE);
				}
			}

			if (it >= maxIterations) {
				Warning(
					kChannel,
					"ShowCursor のカウントが収束しませんでした (targetVisible={}, lastCount={}).",
					bCursorVisible,
					cursorCount
				);
			}
		}
	}

	/// @brief 入力デバイスを登録します
	/// @param device 登録する入力デバイスの共有ポインタ
	void InputSystem::RegisterDevice(
		const std::shared_ptr<BaseInputDevice>& device
	) {
		mDevices.emplace_back(device);
	}

	void InputSystem::PlayGamepadRumble(
		const float low,
		const float high,
		const float durationSec
	) const {
		for (const auto& device : mDevices) {
			if (device->GetDeviceType() != InputDeviceType::GAMEPAD) {
				continue;
			}

			std::static_pointer_cast<GamepadDevice>(device)->EnqueueRumble(
				low, high, durationSec
			);
			return;
		}
	}

	void InputSystem::StopGamepadRumble() const {
		for (const auto& device : mDevices) {
			if (device->GetDeviceType() != InputDeviceType::GAMEPAD) {
				continue;
			}

			std::static_pointer_cast<GamepadDevice>(device)->StopRumble();
			return;
		}
	}

	void InputSystem::BindCommand(
		const InputKey& key, const std::string& command
	) {
		auto& commands = mCommandBinds[key];
		if (!std::ranges::contains(commands, command)) {
			commands.emplace_back(command);
		}
	}

	void InputSystem::UnbindCommand(const InputKey& key) {
		mCommandBinds.erase(key);
		DevMsg(
			kChannel,
			"コマンドのバインドを解除しました: key = {}",
			KeyNameTable::ToString(key)
		);
	}

	void InputSystem::UnbindAllCommands() {
		mCommandBinds.clear();
		DevMsg(kChannel, "すべてのコマンドバインドを解除しました。");
	}

	std::string InputSystem::GetBoundCommand(const InputKey& key) const {
		const auto it = mCommandBinds.find(key);
		if (it == mCommandBinds.end() || it->second.empty()) {
			return {};
		}

		std::string combined;
		for (size_t i = 0; i < it->second.size(); ++i) {
			combined += it->second[i];
			if (i + 1 < it->second.size()) {
				combined += "; ";
			}
		}
		return combined;
	}

	/// @brief アクションをキーにバインドします
	/// @param action アクション名
	/// @param key 入力キー
	void InputSystem::BindAction(
		const std::string& action,
		const InputKey&    key
	) {
		InputBinding binding;
		binding.key    = key;
		binding.type   = BINDING_TYPE::ACTION;
		binding.target = action;
		mBindings.emplace_back(binding);
		DevMsg(
			kChannel,
			"アクションをバインドしました: {}, key = {}",
			action, KeyNameTable::ToString(key)
		);
	}

	/// @brief 1D軸をキーにバインドします
	/// @param axis 軸名
	/// @param key 入力キー
	/// @param scale スケール
	void InputSystem::BindAxis1D(
		const std::string& axis,
		const InputKey&    key,
		float              scale
	) {
		InputBinding binding;
		binding.key    = key;
		binding.type   = BINDING_TYPE::AXIS_1D;
		binding.target = axis;
		binding.scale  = scale;
		mBindings.emplace_back(binding);
		DevMsg(
			kChannel,
			"1D軸をバインドしました: {}, key = {}, scale = {}",
			axis, KeyNameTable::ToString(key), scale
		);
	}

	/// @brief 2D軸をキーにバインドします
	/// @param axis 軸名
	/// @param key 入力キー
	/// @param axisType 軸タイプ（X軸またはY軸）
	/// @param scale スケール
	void InputSystem::BindAxis2D(
		const std::string& axis,
		const InputKey&    key,
		const INPUT_AXIS&  axisType,
		const float&       scale
	) {
		InputBinding binding;
		binding.key    = key;
		binding.type   = BINDING_TYPE::AXIS_2D;
		binding.target = axis;
		if (axisType == INPUT_AXIS::X) {
			binding.scale2D.x = scale;
			binding.scale2D.y = 0.0f; // Y軸は無視される
		} else {
			binding.scale2D.x = 0.0f; // X軸は無視される
			binding.scale2D.y = scale;
		}
		binding.axis = axisType;
		mBindings.emplace_back(binding);
		DevMsg(
			kChannel,
			"2D軸をバインドしました: {}, key = {}, axis = {}, scale = {}",
			axis, KeyNameTable::ToString(key),
			static_cast<int>(axisType), scale
		);
	}

	void InputSystem::Unbind(const std::string& action) {
		std::erase_if(
			mBindings,
			[&action](const InputBinding& binding) {
				return binding.type ==
				       BINDING_TYPE::ACTION &&
				       binding.target == action;
			}
		);
	}

	/// @brief コンソールアクションの処理
	/// @param action アクション名
	/// @param pressed 押下状態
	void InputSystem::HandleConsoleAction(
		const std::string& action,
		const bool&        pressed
	) {
		if (pressed) {
			mActionStates[action].bIsPressed = true;
			mActionStates[action].bIsHeld    = true;
		} else {
			mActionStates[action].bIsReleased = true;
			mActionStates[action].bIsHeld     = false;
		}
	}

	/// @brief 指定したアクションが押された瞬間に true を返します
	/// @param action アクション名
	/// @return 押された瞬間に true
	bool InputSystem::IsPressed(const std::string& action) const {
		const auto it = mActionStates.find(action);
		if (it == mActionStates.end()) {
			return false;
		}
		return it->second.bIsPressed;
	}

	/// @brief 指定したアクションが押されている間 true を返します
	/// @param action アクション名
	/// @return 押されている間 true
	bool InputSystem::IsHeld(const std::string& action) const {
		const auto it = mActionStates.find(action);
		if (it == mActionStates.end()) {
			return false;
		}
		return it->second.bIsHeld;
	}

	/// @brief 指定したアクションが離された瞬間に true を返します
	/// @param action アクション名
	/// @return 離された瞬間に true
	bool InputSystem::IsReleased(const std::string& action) const {
		const auto it = mActionStates.find(action);
		if (it == mActionStates.end()) {
			return false;
		}
		return it->second.bIsReleased;
	}

	/// @brief 指定した1D軸の値を取得します
	/// @param axis 軸名
	/// @return 軸の値
	float InputSystem::Axis1D(const std::string& axis) const {
		const auto it = mAxisStates1D.find(axis);
		if (it == mAxisStates1D.end()) {
			return 0.0f;
		}
		return it != mAxisStates1D.end() ? it->second.value : 0.0f;
	}

	/// @brief 指定した2D軸の値を取得します
	/// @param axis 軸名
	/// @return 軸の値
	Vec2 InputSystem::Axis2D(const std::string& axis) const {
		const auto it = mAxisStates2D.find(axis);
		if (it == mAxisStates2D.end()) {
			return Vec2::zero;
		}
		return it != mAxisStates2D.end() ? it->second.value : Vec2::zero;
	}

	bool InputSystem::IsMouseCursorLocked() const {
		return ResolveMouseCursorLockState();
	}

	void InputSystem::SetMouseCursorLocked(const bool& locked) {
		mMouseCursorLockRequested = locked;
		if (!ResolveMouseCursorLockState()) {
			ClearMouseCursorLockAnchor();
		}
	}

	bool InputSystem::IsMouseCursorVisible() const {
		return ResolveMouseCursorVisibleState();
	}

	void InputSystem::SetMouseCursorVisible(const bool& visible) {
		mMouseCursorVisible = visible;
	}

	void InputSystem::SetMouseCursorLockClientPosition(
		const HWND hwnd, const Vec2& clientPos
	) {
		mMouseLockAnchor.hwnd      = hwnd;
		mMouseLockAnchor.clientPos = clientPos;
		mMouseLockAnchor.valid     = hwnd != nullptr;
	}

	void InputSystem::ClearMouseCursorLockAnchor() {
		mMouseLockAnchor = {};
	}

	void InputSystem::SetMouseClientViewportRect(
		const Vec2& origin,
		const Vec2& size
	) {
		SetMouseClientViewportRect(origin, size, size);
	}

	void InputSystem::SetMouseClientViewportRect(
		const Vec2& origin,
		const Vec2& size,
		const Vec2& virtualSize
	) {
		if (size.x <= 0.0f || size.y <= 0.0f) {
			ClearMouseClientViewportRectOverride();
			return;
		}
		if (virtualSize.x <= 0.0f || virtualSize.y <= 0.0f) {
			ClearMouseClientViewportRectOverride();
			return;
		}

		mMouseViewportRectOverride = true;
		mMouseViewportOrigin       = origin;
		mMouseViewportSizeOverride = Vec2(
			std::max(1.0f, size.x),
			std::max(1.0f, size.y)
		);
		mMouseViewportVirtualSize = Vec2(
			std::max(1.0f, virtualSize.x),
			std::max(1.0f, virtualSize.y)
		);
	}

	void InputSystem::ClearMouseClientViewportRectOverride() {
		mMouseViewportRectOverride = false;
		mMouseViewportOrigin       = Vec2::zero;
		mMouseViewportSizeOverride = Vec2::zero;
		mMouseViewportVirtualSize  = Vec2::zero;
	}

	Vec2 InputSystem::GetMouseClientPosition() const {
		if (!mMouseViewportRectOverride) {
			return mMouseClientPosition;
		}

		auto localPos = Vec2(
			mMouseClientPosition.x - mMouseViewportOrigin.x,
			mMouseClientPosition.y - mMouseViewportOrigin.y
		);
		if (
			mMouseViewportSizeOverride.x > 0.0f &&
			mMouseViewportSizeOverride.y > 0.0f &&
			mMouseViewportVirtualSize.x > 0.0f &&
			mMouseViewportVirtualSize.y > 0.0f
		) {
			localPos.x *=
				mMouseViewportVirtualSize.x / mMouseViewportSizeOverride.x;
			localPos.y *=
				mMouseViewportVirtualSize.y / mMouseViewportSizeOverride.y;
		}
		return localPos;
	}

	Vec2 InputSystem::GetMouseClientViewportSize() const {
		if (
			mMouseViewportRectOverride &&
			mMouseViewportVirtualSize.x > 0.0f &&
			mMouseViewportVirtualSize.y > 0.0f
		) {
			return mMouseViewportVirtualSize;
		}
		return mMouseClientViewportSize;
	}

	bool InputSystem::IsMouseButtonDown(const uint32_t mouseVirtualKey) const {
		const int index = MouseButtonIndexFromVirtualKey(mouseVirtualKey);
		return index >= 0 ? mMouseButtonDown[index] : false;
	}

	bool InputSystem::WasMouseButtonPressed(
		const uint32_t mouseVirtualKey
	) const {
		const int index = MouseButtonIndexFromVirtualKey(mouseVirtualKey);
		return index >= 0 ? mMouseButtonPressed[index] : false;
	}

	bool InputSystem::WasMouseButtonReleased(
		const uint32_t mouseVirtualKey
	) const {
		const int index = MouseButtonIndexFromVirtualKey(mouseVirtualKey);
		return index >= 0 ? mMouseButtonReleased[index] : false;
	}

	bool InputSystem::ResolveMouseCursorLockState() const {
		return mMouseCursorLockOverride ?
			       mMouseCursorLockOverrideMode :
			       mMouseCursorLockRequested;
	}

	bool InputSystem::ResolveMouseCursorVisibleState() const {
		if (mMouseCursorLockOverride && !mMouseCursorLockOverrideMode) {
			return true;
		}
		return mMouseCursorVisible;
	}

	/// @brief 入力状態をリセットします
	void InputSystem::ResetInputStates() {
		DevMsg(
			kChannel,
			"ウィンドウが非アクティブになったため、入力状態をリセットします。"
		);

		for (const auto& [key, bIsPressed] : mCurrentKeyStates) {
			if (bIsPressed) {
				// 現在押下中のキーは前フレームの状態に移行
				mPreviousKeyStates[key] = true;
				// 現在の状態はリセット
				mCurrentKeyStates[key] = false;
			}
		}

		// アクションの状態をリセット
		for (
			auto& [bIsPressed, bIsHeld, bIsReleased] :
			mActionStates | std::views::values
		) {
			if (bIsHeld) {
				bIsReleased = true;
			}

			bIsPressed = false;
			bIsHeld    = false;
		}

		// アナログ値のリセット
		for (auto& [value] : mAxisStates1D | std::views::values) {
			value = 0.0f;
		}
		for (auto& [value] : mAxisStates2D | std::views::values) {
			value = Vec2::zero;
		}

		// デバイス状態のリセット
		for (auto& device : mDevices) {
			device.get()->ResetStates();
		}

		mMouseLockAnchor.valid   = false;
		mMouseClientPosition     = Vec2::zero;
		mMouseClientViewportSize = Vec2::zero;
		mMouseButtonDown.fill(false);
		mMouseButtonPressed.fill(false);
		mMouseButtonReleased.fill(false);
	}

	/// @brief RAW Inputの処理
	/// @param rawInput RAW Inputデータ
	void InputSystem::OnRawInput(const RAWINPUT& rawInput) const {
		for (auto& device : mDevices) {
			if (device->GetDeviceType() == InputDeviceType::KEYBOARD) {
				std::static_pointer_cast<KeyboardDevice>(device)->
					HandleRawInput(rawInput);
			}

			if (device->GetDeviceType() == InputDeviceType::MOUSE) {
				std::static_pointer_cast<MouseDevice>(device)->
					HandleRawInput(rawInput);
			}
		}
	}

	/// @brief ハードウェアキーの状態を取得します
	/// @param key 入力キー
	/// @return 押下中ならtrue
	bool InputSystem::GetHardwareKeyState(const InputKey& key) const {
		for (const auto& device : mDevices) {
			if (device->GetDeviceType() == key.device) {
				return device->GetKeyState(key);
			}
		}
		Warning(
			kChannel,
			"Device '{}' not found for key '{}'.",
			static_cast<int>(key.device), KeyNameTable::ToString(key)
		);
		return false;
	}

	/// @brief アナログ値を取得します
	/// @param key 入力キー
	/// @return アナログ値
	float InputSystem::GetAnalogValue(const InputKey& key) const {
		for (const auto& device : mDevices) {
			if (device->GetDeviceType() == key.device) {
				return device->GetAnalogValue(key);
			}
		}
		return 0.0f;
	}

	/// @brief 仮想キーコードからキー名を取得します
	/// @param virtualKey 仮想キーコード
	/// @return キー名
	std::string InputSystem::GetKeyName(const UINT& virtualKey) {
		char name[256];
		if (
			GetKeyNameTextA(
				static_cast<LONG>(
					MapVirtualKeyA(
						virtualKey,
						MAPVK_VK_TO_VSC
					)
				) << 16,
				name,
				sizeof(name)
			)
		) {
			return name;
		}
		Warning(
			kChannel,
			"キーの名前を取得できませんでした: {}",
			virtualKey
		);
		return "Unknown";
	}

	namespace {
		/// @brief 押下コマンドから離脱コマンドを生成します
		/// @param cmd 押下コマンド文字列
		/// @return 離脱コマンド文字列（押下コマンドでない場合は空文字列）
		std::string MakeReleaseCommandFromPress(const std::string& cmd) {
			const std::string trimmed = StrUtil::TrimSpaces(cmd);
			if (trimmed.empty() || trimmed[0] != '+') {
				return {};
			}

			const size_t firstSpace = trimmed.find(' ');
			const size_t len        =
				firstSpace == std::string::npos ?
					trimmed.size() :
					firstSpace;

			std::string out;
			out.reserve(len);
			out.push_back('-');
			out.append(trimmed, 1, len - 1);
			return out;
		}
	}

	void InputSystem::ProcessCommandBinds() {
		auto* console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			return;
		}

		for (const auto& [key, commands] : mCommandBinds) {
			const bool bIsDown  = GetHardwareKeyState(key);
			const bool bWasDown = mPreviousKeyStates[key];

			mCurrentKeyStates[key] = bIsDown;

			const bool bPressed  = !bWasDown && bIsDown;
			const bool bReleased = bWasDown && !bIsDown;

			if (bPressed) {
				for (const auto& cmd : commands) {
					console->ExecuteCommand(
						cmd, EXEC_FLAG::FROM_USER | EXEC_FLAG::SILENT
					);
				}
			}

			if (bReleased) {
				for (const auto& cmd : commands) {
					const std::string releaseCmd =
						MakeReleaseCommandFromPress(cmd);
					if (!releaseCmd.empty()) {
						console->ExecuteCommand(
							releaseCmd,
							EXEC_FLAG::FROM_USER | EXEC_FLAG::SILENT
						);
					}
				}
			}
		}
	}
}
