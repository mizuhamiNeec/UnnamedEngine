#include <format>
#include <Windows.h>

#ifdef _DEBUG
#include <imgui/imgui.h>
#include "Engine/Core/Utils/Managers/ImGuiManager/ImGuiManager.h"
#endif

#include "Engine/Core/Utils/ClientProperties.h"
#include "Engine/Core/Utils/ConvertString.h"
#include "Engine/Core/Utils/Logger.h"
#include "Game/GameScene.h"
#include "Engine/Core/Utils/Managers/TextureManager/TextureManager.h"

//-----------------------------------------------------------------------------
// エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR pCmdLine, const int nCmdShow) {
	Log(
		ConvertString(
			std::format(L"Launch Args: {}\n", pCmdLine)
		)
	);

	D3DResourceLeakChecker leakChecker;

	std::unique_ptr<Window> window = std::make_unique<Window>();
	WindowConfig windowConfig = {
		kWindowTitle,
		kClientWidth,
		kClientHeight,
		kWindowClassName,
		WS_OVERLAPPEDWINDOW,
		WS_EX_WINDOWEDGE,
		nCmdShow
	};

	window->CreateMainWindow(windowConfig);

	D3D12 renderer;
	renderer.Initialize(window.get());

#ifdef _DEBUG
	ImGuiManager imGuiManager;
	imGuiManager.Initialize(&renderer, window.get());
#endif

	GameScene gameScene;
	gameScene.Startup(&renderer, window.get());

	while (true) {
		if (Window::ProcessMessage()) {
			break;
		}

#ifdef _DEBUG
		imGuiManager.NewFrame();
#endif

		// ゲームシーンの更新
		gameScene.Update();

		// レンダリングの前処理
		renderer.PreRender();

		// ゲームシーンのレンダリング
		gameScene.Render();

#ifdef _DEBUG
		imGuiManager.EndFrame();
#endif

		// レンダリングの後処理
		renderer.PostRender();
	}

	gameScene.Shutdown();

#ifdef _DEBUG
	imGuiManager.Shutdown();
#endif

	renderer.Terminate();

	TextureManager::GetInstance().ReleaseUnusedTextures();
	TextureManager::GetInstance().Shutdown();

	return 0;
}
