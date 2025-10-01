#include <pch.h>

#include <engine/platform/IPlatformEvents.h>
#include <engine/subsystem/window/Win32/Win32Window.h>
#include <engine/subsystem/window/Win32/Win32WindowSystem.h>
#include <engine/Window/WindowsUtils.h>

Win32WindowSystem::~Win32WindowSystem() = default;

bool Win32WindowSystem::Init() {
	mHInstance = GetModuleHandle(nullptr);
	RegisterClassOnce();
	ServiceLocator::Register<Win32WindowSystem>(this);
	return true;
}

void Win32WindowSystem::Update(float) {
	MSG msg;
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			for (const auto& w : mWindows) {
				w->RequestClose();
			}
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

void Win32WindowSystem::Shutdown() {
	mHWndMap.clear();
	mWindows.clear();
}

void Win32WindowSystem::RegisterPlatformEvent(
	Unnamed::IPlatformEvents* events
) {
	mPlatformEvents = events;
}

IWindow* Win32WindowSystem::CreateNewWindow(
	const IWindow::WindowInfo& windowInfo
) {
	auto win = std::make_unique<Win32Window>(windowInfo);
	if (
		!win->Create(
			mHInstance,
			mClassName,
			static_cast<HWND>(
				mWindows.empty() ? nullptr : mWindows.front()->GetNativeHandle()
			)
		)
	) {
		auto error = GetLastError();
		Fatal(
			"Win32WindowSystem",
			"Failed to create window. Error: {}",
			error
		);
		return nullptr;
	}
	auto hwnd      = static_cast<HWND>(win->GetNativeHandle());
	mHWndMap[hwnd] = win.get();
	auto* rawPtr   = win.get();
	mWindows.emplace_back(std::move(win));
	return rawPtr;
}

const std::vector<std::unique_ptr<IWindow>>&
Win32WindowSystem::GetWindows() const {
	return mWindows;
}

bool Win32WindowSystem::AllClosed() const {
	if (mWindows.empty()) {
		return true;
	}
	for (auto& window : mWindows) {
		if (!window->ShouldClose()) {
			return false;
		}
	}
	return true;
}

bool Win32WindowSystem::IsInactiveWindow() {
	if (mWindows.empty()) {
		return true;
	}
	for (auto& window : mWindows) {
		if (window) {
			return window->GetInfo().bIsInactive;
		}
	}
	return true;
}

LRESULT Win32WindowSystem::WndProc(
	const HWND   hWnd, const UINT     msg,
	const WPARAM wParam, const LPARAM lParam
) {
	const auto it = mHWndMap.find(hWnd);
	auto*      w  = it != mHWndMap.end() ? it->second : nullptr;

	if (mPlatformEvents) {
		if (mPlatformEvents->DispatchMessage(hWnd, msg, wParam, lParam)) {
			return 0;
		}
	}

	switch (msg) {
	case WM_NCCREATE: {
		auto* cs      = reinterpret_cast<CREATESTRUCTW*>(lParam);
		auto* created = static_cast<Win32Window*>(cs->lpCreateParams);
		if (created) {
			mHWndMap[hWnd] = created;
			SetWindowLongPtrW(
				hWnd,
				GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(created)
			);
		}
		break;
	}
	case WM_SETTINGCHANGE: {
		w->SetImmersiveDarkMode(WindowsUtils::IsSystemDarkTheme());
		break;
	}
	case WM_ACTIVATE: {
		w->SetInactive(LOWORD(wParam) == WA_INACTIVE);
		break;
	}
	case WM_CLOSE: {
		if (w) {
			w->RequestClose();
			DestroyWindow(hWnd);
		}
		return 0;
	}
	case WM_DESTROY: {
		if (w) {
			w->RequestClose();
			mHWndMap.erase(hWnd);
			if (mHWndMap.empty()) {
				PostQuitMessage(0);
			}
		}
		return 0;
	}
	case WM_SIZE: {
		if (w) {
			const auto width  = LOWORD(lParam);
			const auto height = HIWORD(lParam);
			if (width != 0 && height != 0) {
				w->OnResize(width, height);
			}
		}
		break;
	}
	default: break;
	}

	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Win32WindowSystem::RegisterClassOnce() {
	if (mRegistered) {
		return;
	}

	auto classNameW = StrUtil::ToWString(mClassName);

	WNDCLASSEXW wc   = {};
	wc.cbSize        = sizeof(WNDCLASSEXW);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = &Win32WindowSystem::WndProc;
	wc.hInstance     = mHInstance;
	wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName  = nullptr;
	wc.lpszClassName = classNameW.c_str();
	wc.hIcon         = LoadIcon(mHInstance, IDI_APPLICATION);
	wc.hIconSm       = LoadIcon(mHInstance, IDI_APPLICATION);

	if (!RegisterClassExW(&wc)) {
		auto error = GetLastError();
		Fatal(
			"Win32WindowSystem",
			"Failed to register window class. Error: {}",
			error
		);
		return;
	}
	mRegistered = true;
}

std::vector<std::unique_ptr<IWindow>> Win32WindowSystem::mWindows;
std::unordered_map<HWND, class Win32Window*> Win32WindowSystem::mHWndMap;
Unnamed::IPlatformEvents* Win32WindowSystem::mPlatformEvents = nullptr;
