#include "WindowManager.h"

#include <dwmapi.h>
#include <ranges>

#include "IPlatformEvents.h"
#include "WindowsUtils.h"

#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

#pragma comment(lib, "winmm.lib")

#ifdef UNNAMED_WITH_EDITOR
/// @brief ImGuiのWndProcハンドラ
/// @param hWnd ウィンドウハンドル
/// @param msg メッセージ
/// @param wParam wParam
/// @param lParam lParam
/// @return 処理結果
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam
);
#endif

namespace Unnamed {
	static constexpr std::string_view kChannel = "WindowManager";

	namespace {
		auto gWindowClassName = L"UnnamedEngineWindowClass";
	}

	WindowManager::WindowManager()  = default;
	WindowManager::~WindowManager() = default;

	bool WindowManager::Init(
		const EngineConfig::Window& mainWindowConfig
	) {
		if (mInitialized) {
			Warning(kChannel, "WindowManager is already initialized.");
			return false;
		}

		WindowDesc mainDesc;
		mainDesc.title     = mainWindowConfig.title;
		mainDesc.width     = mainWindowConfig.width;
		mainDesc.height    = mainWindowConfig.height;
		mainDesc.mode      = mainWindowConfig.mode;
		mainDesc.resizable = mainWindowConfig.resizable;
		mainDesc.visible   = true;

		if (!EnsureWindowClassRegistered()) {
			Error(kChannel, "Failed to register window class.");
			return false;
		}

		WindowDesc nativeDesc = mainDesc;
		nativeDesc.mode       = WINDOW_MODE::WINDOWED;
		nativeDesc.visible    = mainDesc.visible &&
		                        mainDesc.mode == WINDOW_MODE::WINDOWED;

		const auto hwndOpt = CreateNativeWindow(nativeDesc);
		if (!hwndOpt.has_value()) {
			Error(kChannel, "Failed to create main window.");
			return false;
		}

		mMainWindowId = WindowId{mNextWindowId++};
		auto window   = std::make_unique<Window>(
			mMainWindowId, nativeDesc, hwndOpt.value()
		);
		window->SetMode(mainDesc.mode);
		if (mainDesc.visible && mainDesc.mode != WINDOW_MODE::WINDOWED) {
			ShowWindow(hwndOpt.value(), SW_SHOW);
			UpdateWindow(hwndOpt.value());
			SetFocus(hwndOpt.value());
		}

		mWindows.emplace(mMainWindowId.value, std::move(window));

		mInitialized = true;
		return true;
	}

	void WindowManager::Shutdown() {
		if (!mInitialized) {
			Error(kChannel, "WindowManager is not initialized.");
			return;
		}

		std::vector<HWND> hwnds;
		hwnds.reserve(mWindows.size());
		for (const auto& wnd : mWindows | std::views::values) {
			if (wnd && wnd->GetHwnd()) {
				hwnds.emplace_back(wnd->GetHwnd());
			}
		}

		for (const HWND hwnd : hwnds) {
			::DestroyWindow(hwnd);
		}

		mWindows.clear();

		if (mWindowClassRegistered) {
			UnregisterClassW(gWindowClassName, GetModuleHandleW(nullptr));
			mWindowClassRegistered = false;
		}

		mInitialized = false;
	}

	void WindowManager::ProcessMessage() {
		MSG msg{};
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				for (auto& wnd : mWindows | std::views::values) {
					if (wnd) {
						// 各ウィンドウのWM_DESTROYで勝手に消えるので特に何もしない
					}
				}
			}

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	bool WindowManager::ShouldQuit() const {
		// メインウィンドウが閉じられたら終了

		const auto it = mWindows.find(mMainWindowId.value);
		if (it == mWindows.end() || !it->second) {
			return true;
		}

		return it->second->ShouldClose();
	}

	WindowId WindowManager::GetMainWindowId() const {
		return mMainWindowId;
	}

	std::optional<WindowId> WindowManager::CreateNewWindow(
		const WindowDesc& desc
	) {
		if (!mInitialized) {
			return std::nullopt;
		}
		if (!EnsureWindowClassRegistered()) {
			return std::nullopt;
		}

		WindowDesc nativeDesc = desc;
		nativeDesc.mode       = WINDOW_MODE::WINDOWED;
		nativeDesc.visible    = desc.visible &&
		                        desc.mode == WINDOW_MODE::WINDOWED;

		const auto hwndOpt = CreateNativeWindow(nativeDesc);
		if (!hwndOpt.has_value()) {
			return std::nullopt;
		}

		const WindowId id{mNextWindowId++};
		auto window = std::make_unique<Window>(id, nativeDesc, hwndOpt.value());
		window->SetMode(desc.mode);
		if (desc.visible && desc.mode != WINDOW_MODE::WINDOWED) {
			ShowWindow(hwndOpt.value(), SW_SHOW);
			UpdateWindow(hwndOpt.value());
			SetFocus(hwndOpt.value());
		}
		mWindows.emplace(id.value, std::move(window));
		return id;
	}

	void WindowManager::DestroyWindow(const WindowId id) {
		const auto it = mWindows.find(id.value);
		if (it == mWindows.end() || !it->second) {
			return;
		}

		if (const HWND hwnd = it->second->GetHwnd()) {
			::DestroyWindow(hwnd);
		}

		mWindows.erase(it);
	}

	Window* WindowManager::FindWindowById(const WindowId id) {
		const auto it = mWindows.find(id.value);
		if (it == mWindows.end()) {
			return nullptr;
		}
		return it->second.get();
	}

	const Window* WindowManager::FindWindowById(const WindowId id) const {
		const auto it = mWindows.find(id.value);
		if (it == mWindows.end()) {
			return nullptr;
		}
		return it->second.get();
	}

	std::vector<WindowId> WindowManager::GetAllWindowIds() const {
		std::vector<WindowId> ids;
		ids.reserve(mWindows.size());
		for (const auto& id : mWindows | std::views::keys) {
			ids.emplace_back(WindowId{id});
		}
		return ids;
	}

	void WindowManager::RegisterPlatformEvents(
		IPlatformEvents* events
	) {
		mPlatformEvents = events;
	}

	bool WindowManager::EnsureWindowClassRegistered() {
		if (mWindowClassRegistered) {
			return true;
		}

		WNDCLASSEXW wc   = {};
		wc.cbSize        = sizeof(WNDCLASSEXW);
		wc.style         = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = &WindowManager::StaticWndProc;
		wc.hInstance     = GetModuleHandleW(nullptr);
		wc.lpszClassName = gWindowClassName;
		wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
		wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
		wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
		wc.hIconSm       = LoadIcon(wc.hInstance, IDI_APPLICATION);

		if (!RegisterClassExW(&wc)) {
			return false;
		}

		mWindowClassRegistered = true;
		return true;
	}

	std::optional<HWND> WindowManager::CreateNativeWindow(
		const WindowDesc& desc
	) {
		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!desc.resizable) {
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		}

		RECT rect{.left = 0, .top = 0, .right = desc.width, .bottom = desc.height};
		AdjustWindowRectEx(&rect, style, FALSE, 0);

		const std::wstring titleW = StrUtil::ToWString(desc.title);

		HWND hwnd = CreateWindowExW(
			0,
			gWindowClassName,
			titleW.c_str(),
			style,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			rect.right - rect.left,
			rect.bottom - rect.top,
			nullptr,
			nullptr,
			GetModuleHandleW(nullptr),
			this
		);

		if (!hwnd) {
			Error(
				kChannel, "Failed to create window. Error code: {}",
				GetLastError()
			);
			return std::nullopt;
		}

		const HMONITOR hMonitor = MonitorFromWindow(
			hwnd, MONITOR_DEFAULTTONEAREST
		);
		MONITORINFO mi = {.cbSize = sizeof(mi)};
		if (!GetMonitorInfoW(hMonitor, &mi)) {
			Error(kChannel, "Failed to get monitor info.");
			::DestroyWindow(hwnd);
			return std::nullopt;
		}

		// モニタの中央位置を計算して移動
		const int32_t posX = mi.rcMonitor.left + (
			                     mi.rcMonitor.right - mi.rcMonitor.
			                     left - (rect.right - rect.left)) / 2;
		const int32_t posY = mi.rcMonitor.top + (
			                     mi.rcMonitor.bottom - mi.rcMonitor.
			                     top - (rect.bottom - rect.top)) / 2;

		SetWindowPos(
			hwnd,
			nullptr,
			posX, posY,
			0, 0,
			SWP_NOZORDER | SWP_NOSIZE
		);

		// テーマを設定
		SetUseImmersiveDarkMode(hwnd, WindowsUtils::IsSystemDarkTheme());

		if (desc.visible) {
			ShowWindow(hwnd, SW_SHOW);
			UpdateWindow(hwnd);
			SetFocus(hwnd);
		}

		DevMsg(kChannel, "Created window '{}'", desc.title);

		return hwnd;
	}

	LRESULT WindowManager::StaticWndProc(
		const HWND   hwnd, const UINT msg, const WPARAM wParam,
		const LPARAM lParam
	) {
#ifdef UNNAMED_WITH_EDITOR
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
			return true;
		}
#endif

		// メッセージを他のシステムに横流し
		if (mPlatformEvents) {
			mPlatformEvents->DispatchMessage(hwnd, msg, wParam, lParam);
		}

		// --------------------------------------------------------------------
		// ザ・ワールド対策 Alt || F10キー対策
		// --------------------------------------------------------------------
		if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
			if (wParam == VK_F4 && lParam & 1 << 29) {
				// Alt + F4
				PostMessageW(hwnd, WM_CLOSE, 0, 0);
				return 0;
			}
			return 0;
		}

		if (msg == WM_NCCREATE) {
			// 作成時にぶち込んだWindowManagerポインタ引っこ抜く
			const auto cs = reinterpret_cast<const CREATESTRUCTW*>(
				lParam
			);
			auto* manager = static_cast<WindowManager*>(cs->lpCreateParams);
			// ウィンドウデータに保存しておく
			SetWindowLongPtrW(
				hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(manager)
			);
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		// WM_NCDESTROY は最終破棄
		if (msg == WM_NCDESTROY) {
			auto* manager = reinterpret_cast<WindowManager*>(GetWindowLongPtrW(
				hwnd, GWLP_USERDATA
			));
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);

			if (manager) {
				WindowId targetId{};
				for (const auto& wnd : manager->mWindows | std::views::values) {
					if (wnd && wnd->GetHwnd() == hwnd) {
						targetId = wnd->GetId();
						break;
					}
				}

				if (targetId.value != 0) {
					// メイン以外はここで確実に破棄。
					if (targetId != manager->mMainWindowId) {
						manager->mWindows.erase(targetId.value);
					} else {
						PostQuitMessage(0);
					}
				}
			}

			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		auto* manager = reinterpret_cast<WindowManager*>(GetWindowLongPtrW(
			hwnd, GWLP_USERDATA
		));
		if (!manager) {
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		// hwndからWindowを探す
		Window* target = nullptr;
		for (const auto& wnd : manager->mWindows | std::views::values) {
			if (wnd && wnd->GetHwnd() == hwnd) {
				target = wnd.get();
				break;
			}
		}

		if (target) {
			const WindowId targetId = target->GetId();
			const LRESULT  result   = target->HandleMessage(
				hwnd, msg, wParam, lParam
			);
			(void)targetId;

			return result;
		}

		if (msg == WM_SETTINGCHANGE) {
			SetUseImmersiveDarkMode(hwnd, WindowsUtils::IsSystemDarkTheme());
		}

		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void WindowManager::SetUseImmersiveDarkMode(
		const HWND hwnd, const bool isSystemDarkTheme
	) {
		const BOOL    value = isSystemDarkTheme;
		const HRESULT hr    = DwmSetWindowAttribute(
			hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value)
		);
		if (FAILED(hr)) {
			const std::string errorMessage =
				WindowsUtils::GetHresultMessage(hr);
			Error(
				kChannel, "Failed to set immersive dark mode: {}", errorMessage
			);
		}
	}

	IPlatformEvents* WindowManager::mPlatformEvents = nullptr;
}
