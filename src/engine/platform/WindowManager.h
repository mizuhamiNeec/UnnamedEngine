#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "IPlatformEvents.h"
#include "Window.h"

#include "engine/EngineConfig.h"

namespace Unnamed {
	class WindowManager final {
	public:
		WindowManager();
		~WindowManager();

		/// @brief 初期化処理
		/// @param mainWindowConfig メインウィンドウの設定
		/// @return 成功したらtrue、失敗したらfalse
		bool Init(const EngineConfig::Window& mainWindowConfig);

		/// @brief 終了処理
		void Shutdown();

		/// @brief プラットフォームイベントを処理します。
		void ProcessMessage();

		/// @brief 終了すべきかどうかを返す
		/// @return 終了すべきならtrue、そうでなければfalse
		[[nodiscard]] bool ShouldQuit() const;

		/// @brief メインウィンドウのIDを取得する
		/// @return メインウィンドウのID
		[[nodiscard]] WindowId GetMainWindowId() const;

		/// @brief 新しいウィンドウを作成する
		/// @param desc ウィンドウの説明
		/// @return 作成したウィンドウのID。失敗した場合はstd::nulloptを返す
		std::optional<WindowId> CreateNewWindow(const WindowDesc& desc);

		/// @brief ウィンドウを破棄すします。
		/// @param id 破棄するウィンドウのID
		void DestroyWindow(WindowId id);

		/// @brief IDからウィンドウを探します。
		/// @return 見つかったウィンドウへのポインタ。見つからなかった場合はnullptrを返す
		[[nodiscard]]
		Window* FindWindowById(WindowId id);

		/// @brief IDからウィンドウを探します。 (const版)
		[[nodiscard]] const Window* FindWindowById(WindowId id) const;

		/// @brief すべてのウィンドウIDを取得します。
		[[nodiscard]] std::vector<WindowId> GetAllWindowIds() const;

		/// @brief プラットフォームイベントのコールバックを登録します。
		/// @param events 登録するイベントコールバックのインターフェースへのポインタ
		void RegisterPlatformEvents(IPlatformEvents* events);

	private:
		/// @brief ウィンドウクラスが登録されていることを保証します。登録されていない場合は登録します。
		/// @return 登録に成功したらtrue、失敗したらfalse
		bool EnsureWindowClassRegistered();

		/// @brief ネイティブウィンドウを作成します。
		/// @param desc ウィンドウの説明
		/// @return 作成したウィンドウのHWND。失敗した場合はstd::nulloptを返す
		std::optional<HWND> CreateNativeWindow(const WindowDesc& desc);

		/// @brief ウィンドウプロシージャの静的コールバック関数
		/// @param hwnd ウィンドウハンドル
		/// @param msg メッセージID
		/// @param wParam メッセージのWPARAM
		/// @param lParam メッセージのLPARAM
		/// @return メッセージ処理の結果を返す
		static LRESULT CALLBACK StaticWndProc(
			HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
		);

		/// @brief ウィンドウのダークモード設定を変更する
		/// @param hwnd ウィンドウハンドル
		/// @param isSystemDarkTheme ダークテーマを使用するかどうか
		static void SetUseImmersiveDarkMode(HWND hwnd, bool isSystemDarkTheme);

		WindowId mMainWindowId = {};
		uint32_t mNextWindowId = 1;
		std::unordered_map<uint32_t, std::unique_ptr<Window>> mWindows;

		static IPlatformEvents* mPlatformEvents;

		bool mInitialized           = false;
		bool mWindowClassRegistered = false;
	};
}
