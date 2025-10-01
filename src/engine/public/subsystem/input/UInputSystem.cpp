#include <pch.h>

#include <ranges>

#include <engine/public/subsystem/input/KeyNameTable.h>
#include <engine/public/subsystem/input/UInputSystem.h>
#include <engine/public/subsystem/input/device/keyboard/KeyboardDevice.h>
#include <engine/public/subsystem/input/device/mouse/MouseDevice.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "InputSystem";

	UInputSystem::~UInputSystem() {
	}

	bool UInputSystem::Init() {
		ServiceLocator::Register<UInputSystem>(this);
		return true;
	}

	void UInputSystem::Update(float) {
		for (const auto& inputDevice : mDevices) {
			inputDevice->Update();
		}

		//-------------------------------------------------------------------------
		// 軸入力は毎フレームリセットする
		//-------------------------------------------------------------------------
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
						false, false, false);
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

		// 一時マップから最終アクション状態を更新
		for (const auto& [action, states] : tempActionStates) {
			mActionStates[action] = {
				.bIsPressed = std::get<0>(states),
				.bIsHeld = std::get<1>(states),
				.bIsReleased = std::get<2>(states)
			};
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

	const std::string_view UInputSystem::GetName() const {
		return "InputSystem";
	}

	//-------------------------------------------------------------------------
	// Purpose: Win32 メッセージを受け取ったときに呼び出されます
	//-------------------------------------------------------------------------
	bool UInputSystem::OnWin32Message(
		[[maybe_unused]] HWND hWnd,
		UINT                  msg,
		WPARAM                wParam,
		LPARAM                lParam
	) {
		switch (msg) {
		case WM_ACTIVATE:
			// ウィンドウが非アクティブになったら入力をリセット
			if (LOWORD(wParam) == WA_INACTIVE) {
				ResetInputStates();
			}
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

		default:
			break;
		}

		return false;
	}

	void UInputSystem::RegisterDevice(
		const std::shared_ptr<BaseInputDevice>& device
	) {
		mDevices.emplace_back(device);
	}

	void UInputSystem::BindAction(
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
			"BindAction: {}, key = {}",
			action, KeyNameTable::ToString(key)
		);
	}

	void UInputSystem::BindAxis1D(
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
			"BindAxis1D: {}, key = {}, scale = {}",
			axis, KeyNameTable::ToString(key), scale
		);
	}

	void UInputSystem::BindAxis2D(const std::string& axis, const InputKey& key,
	                              const INPUT_AXIS&  axisType,
	                              const float&       scale) {
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
			"BindAxis2D: {}, key = {}, axis = {}, scale = {}",
			axis, KeyNameTable::ToString(key),
			static_cast<int>(axisType), scale
		);
	}

	//-----------------------------------------------------------------------------
	// Purpose: コンソールから送信されたアクションを処理します
	//-----------------------------------------------------------------------------
	void UInputSystem::HandleConsoleAction(
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

	//-----------------------------------------------------------------------------
	// Purpose: 指定したアクションが押された瞬間に true を返します
	//-----------------------------------------------------------------------------
	bool UInputSystem::IsPressed(const std::string& action) const {
		const auto it = mActionStates.find(action);
		if (it == mActionStates.end()) {
			Warning(
				kChannel,
				"Action '{}' is not found.",
				action
			);
			return false;
		}
		return it != mActionStates.end() && it->second.bIsPressed;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 指定したアクションが押されてから離されるまでの間に true を返します
	//-----------------------------------------------------------------------------
	bool UInputSystem::IsHeld(const std::string& action) const {
		const auto it = mActionStates.find(action);
		if (it == mActionStates.end()) {
			Warning(
				kChannel,
				"Action '{}' is not found.",
				action
			);
			return false;
		}
		return it != mActionStates.end() && it->second.bIsHeld;
	}

	bool UInputSystem::IsReleased(const std::string& action) const {
		const auto it = mActionStates.find(action);
		if (it == mActionStates.end()) {
			Warning(
				kChannel,
				"Action '{}' is not found.",
				action
			);
			return false;
		}
		return it != mActionStates.end() && it->second.bIsReleased;
	}

	float UInputSystem::Axis1D(const std::string& axis) const {
		const auto it = mAxisStates1D.find(axis);
		if (it == mAxisStates1D.end()) {
			Warning(
				kChannel,
				"Axis '{}' is not found.",
				axis
			);
			return 0.0f;
		}
		return it != mAxisStates1D.end() ? it->second.value : 0.0f;
	}

	Vec2 UInputSystem::Axis2D(const std::string& axis) const {
		const auto it = mAxisStates2D.find(axis);
		if (it == mAxisStates2D.end()) {
			Warning(
				kChannel,
				"Axis '{}' is not found.",
				axis
			);
			return Vec2::zero;
		}
		return it != mAxisStates2D.end() ? it->second.value : Vec2::zero;
	}

	void UInputSystem::ResetInputStates() {
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
	}

	void UInputSystem::OnRawInput(const RAWINPUT& rawInput) {
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

	bool UInputSystem::GetHardwareKeyState(const InputKey& key) const {
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

	float UInputSystem::GetAnalogValue(const InputKey& key) const {
		for (const auto& device : mDevices) {
			if (device->GetDeviceType() == key.device) {
				return device->GetAnalogValue(key);
			}
		}
		return 0.0f;
	}

	std::string UInputSystem::GetKeyName(const UINT& virtualKey) {
		char name[256];
		if (
			GetKeyNameTextA(
				static_cast<LONG>(
					MapVirtualKeyA(
						virtualKey,
						MAPVK_VK_TO_VSC)
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
}
