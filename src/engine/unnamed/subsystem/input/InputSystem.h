#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include <engine/IWin32MsgListener.h>
#include <engine/unnamed/subsystem/input/device/base/BaseInputDevice.h>
#include <engine/unnamed/subsystem/interface/ISubsystem.h>

/// @brief 入力キー構造体
template <>
struct std::hash<Unnamed::InputKey> {
	size_t operator()(const Unnamed::InputKey& key) const noexcept {
		return hash<uint32_t>()(static_cast<uint32_t>(key.device)) ^ hash<
			       uint32_t>()(key.code << 1);
	}
};

namespace Unnamed {
	class ConCommand;

	/// @brief 入力アクション状態構造体
	struct InputActionState {
		bool bIsPressed  = false;
		bool bIsHeld     = false;
		bool bIsReleased = false;
	};

	/// @brief 1D入力軸状態構造体
	struct InputAxisState1D {
		float value = 0.0f;
	};

	enum class INPUT_AXIS : uint8_t {
		X, Y
	};

	/// @brief 2D入力軸状態構造体
	struct InputAxisState2D {
		Vec2 value = Vec2::zero;
	};

	enum class BINDING_TYPE : uint8_t {
		ACTION,
		AXIS_1D,
		AXIS_2D
	};

	/// @brief 入力バインディング構造体
	struct InputBinding {
		InputKey     key;
		BINDING_TYPE type;
		std::string  target;

		float      scale   = 1.0f;
		Vec2       scale2D = Vec2::one;
		INPUT_AXIS axis    = INPUT_AXIS::X;
	};

	/// @brief 入力システムクラス
	class InputSystem final : public ISubsystem, public IWin32MsgListener {
	public:
		~InputSystem() override;

		// ISubsystem
		bool Init() override;
		void Update(float) override;
		void Shutdown() override;

		[[nodiscard]] const std::string_view GetName() const override;

		// IWin32MsgListener
		bool OnWin32Message(HWND, UINT, WPARAM, LPARAM) override;

		//---------------------------------------------------------------------

		/// @brief マウスカーソルのロックと表示状態をチェックします
		/// @param hWnd ウィンドウハンドル
		void CheckMouseCursorLockAndVisibility(HWND hWnd) const;

		/// @brief 入力デバイスを登録します
		/// @param device 登録する入力デバイスの共有ポインタ
		void RegisterDevice(const std::shared_ptr<BaseInputDevice>& device);

		/// @brief ゲームパッドランブルを追加再生します
		/// @param low 低周波モータ強度 [0..1]
		/// @param high 高周波モータ強度 [0..1]
		/// @param durationSec 再生時間（秒）
		void PlayGamepadRumble(float low, float high, float durationSec) const;

		/// @brief すべてのゲームパッドランブルを停止します
		void StopGamepadRumble() const;

		/// @brief コマンドをキーにバインドします
		/// @param key バインドするキー
		/// @param command バインドするコマンド文字列
		void BindCommand(const InputKey& key, const std::string& command);

		/// @brief キーからバインドされているコマンドを解除します
		/// @param key バインドを解除するキー
		void UnbindCommand(const InputKey& key);

		/// @brief すべてのコマンドバインドを解除します
		void UnbindAllCommands();

		/// @brief キーにバインドされているコマンドを取得します
		/// @param key キー
		/// @return バインドされているコマンド文字列（存在しない場合は空文字列）
		[[nodiscard]] std::string GetBoundCommand(const InputKey& key) const;

		/// @brief アクションまたは軸にキーをバインドします
		/// @param action バインドするアクションまたは軸の名前
		/// @param key バインドするキー
		void BindAction(const std::string& action, const InputKey& key);

		/// @brief 1D軸にキーをバインドします
		/// @param axis バインドする軸の名前
		/// @param key バインドするキー
		/// @param scale スケール値（デフォルトは1.0f）
		void BindAxis1D(
			const std::string& axis,
			const InputKey&    key,
			float              scale = 1.0f
		);

		/// @brief 2D軸にキーをバインドします
		/// @param axis バインドする軸の名前
		/// @param key バインドするキー
		/// @param axisType 軸のタイプ（XまたはY）
		/// @param scale スケール値（デフォルトは1.0f）
		void BindAxis2D(
			const std::string& axis,
			const InputKey&    key,
			const INPUT_AXIS&  axisType,
			const float&       scale = 1.0f
		);

		/// @brief アクションまたは軸からすべてのバインドを解除します
		/// @param action バインドを解除するアクションまたは軸の名前
		void Unbind(const std::string& action);

		/// @brief コンソールからのアクション入力を処理します
		/// @param action アクション名
		/// @param bPressed 押下状態（trueなら押下、falseなら解放）
		void HandleConsoleAction(
			const std::string& action, const bool& bPressed
		);

		/// @brief 指定したアクションの状態を取得します
		/// @param action 取得するアクションの名前
		/// @return 押下状態、保持状態、解放状態
		[[nodiscard]] bool IsPressed(const std::string& action) const;

		/// @brief 指定したアクションの状態を取得します
		/// @param action 取得するアクションの名前
		/// @return 押下状態、保持状態、解放状態
		[[nodiscard]] bool IsHeld(const std::string& action) const;

		/// @brief 指定したアクションの状態を取得します
		/// @param action 取得するアクションの名前
		/// @return 押下状態、保持状態、解放状態
		[[nodiscard]] bool IsReleased(const std::string& action) const;

		/// @brief 指定した1D軸の値を取得します
		/// @param axis 取得する軸の名前
		/// @return 軸の値
		[[nodiscard]] float Axis1D(const std::string& axis) const;

		/// @brief 指定した2D軸の値を取得します
		/// @param axis 取得する軸の名前
		/// @return 軸の値
		[[nodiscard]] Vec2 Axis2D(const std::string& axis) const;

		/// @brief マウスカーソルがロックされているかどうかを取得します
		/// @return ロックされているならtrue
		[[nodiscard]] bool IsMouseCursorLocked() const;

		/// @brief マウスカーソルのロック状態を設定します
		/// @param locked ロックするならtrue、解除するならfalse
		void SetMouseCursorLocked(const bool& locked);

		/// @brief マウスカーソルが表示されているかどうかを取得します
		/// @return 表示されているならtrue
		[[nodiscard]] bool IsMouseCursorVisible() const;

		/// @brief マウスカーソルの表示状態を設定します
		/// @param visible 表示するならtrue、非表示にするならfalse
		void SetMouseCursorVisible(const bool& visible);

		/// @brief マウスカーソルのロック位置を設定します
		/// @param hwnd ロック位置の基準となるウィンドウ
		/// @param clientPos ロック位置のクライアント座標
		void SetMouseCursorLockClientPosition(HWND hwnd, const Vec2& clientPos);
		void ClearMouseCursorLockAnchor();
		void SetMouseClientViewportRect(const Vec2& origin, const Vec2& size);
		void SetMouseClientViewportRect(
			const Vec2& origin,
			const Vec2& size,
			const Vec2& virtualSize
		);
		void ClearMouseClientViewportRectOverride();

		[[nodiscard]] Vec2 GetMouseClientPosition() const;
		[[nodiscard]] Vec2 GetMouseClientViewportSize() const;
		[[nodiscard]] bool IsMouseButtonDown(uint32_t mouseVirtualKey) const;
		[[nodiscard]] bool WasMouseButtonPressed(
			uint32_t mouseVirtualKey
		) const;
		[[nodiscard]] bool WasMouseButtonReleased(
			uint32_t mouseVirtualKey
		) const;

	private:
		[[nodiscard]] bool ResolveMouseCursorLockState() const;
		[[nodiscard]] bool ResolveMouseCursorVisibleState() const;

		/// @brief 入力状態をリセットします
		void ResetInputStates();

		/// @brief RAWINPUTメッセージを処理します
		/// @param rawInput RAWINPUT構造体
		void OnRawInput(const RAWINPUT& rawInput) const;

		/// @brief ハードウェアキーの現在の状態を取得します
		/// @param key 取得するキー
		/// @return 押下されているならtrue
		[[nodiscard]] bool GetHardwareKeyState(const InputKey& key) const;

		/// @brief アナログ値を取得します
		/// @param key 取得するキー
		/// @return アナログ値
		[[nodiscard]] float GetAnalogValue(const InputKey& key) const;

		/// @brief 仮想キーコードからキー名を取得します
		/// @param virtualKey 仮想キーコード
		/// @return キー名の文字列
		static std::string GetKeyName(const UINT& virtualKey);

		/// @brief コマンドバインドを押下/解放に応じて実行します
		void ProcessCommandBinds();

		std::unordered_map<std::string, InputActionState> mActionStates;
		std::unordered_map<std::string, InputAxisState1D> mAxisStates1D;
		std::unordered_map<std::string, InputAxisState2D> mAxisStates2D;

		std::vector<InputBinding> mBindings;
		std::unordered_map<InputKey, bool> mCurrentKeyStates;
		std::unordered_map<InputKey, bool> mPreviousKeyStates;
		std::unordered_map<InputKey, float> mAnalogValues;
		std::unordered_map<InputKey, std::vector<std::string>> mCommandBinds;

		std::vector<std::shared_ptr<BaseInputDevice>> mDevices;

		struct MouseLockAnchor {
			HWND hwnd      = nullptr;
			Vec2 clientPos = Vec2::zero;
			bool valid     = false;
		};

		MouseLockAnchor mMouseLockAnchor             = {};
		bool            mMouseCursorLockRequested    = false;
		bool            mMouseCursorLockOverride     = false;
		bool            mMouseCursorLockOverrideMode = false;
		bool            mMouseCursorVisible          = false;

		Vec2                mMouseClientPosition       = Vec2::zero;
		Vec2                mMouseClientViewportSize   = Vec2::zero;
		bool                mMouseViewportRectOverride = false;
		Vec2                mMouseViewportOrigin       = Vec2::zero;
		Vec2                mMouseViewportSizeOverride = Vec2::zero;
		Vec2                mMouseViewportVirtualSize  = Vec2::zero;
		std::array<bool, 5> mMouseButtonDown           = {};
		std::array<bool, 5> mMouseButtonPressed        = {};
		std::array<bool, 5> mMouseButtonReleased       = {};

		std::unique_ptr<ConCommand> mBindCommand;
		std::unique_ptr<ConCommand> mUnbindCommand;
		std::unique_ptr<ConCommand> mUnbindAllCommand;
		std::unique_ptr<ConCommand> mToggleLockCursorCommand;
		std::unique_ptr<ConCommand> mToggleCursorLockCommand;
	};
}
