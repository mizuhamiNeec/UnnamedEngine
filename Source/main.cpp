#include <format>
#include <Windows.h>
#include <imgui/imgui.h>

#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/Renderer/D3D12.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Utils/ClientProperties.h"
#include "Engine/Utils/ConvertString.h"
#include "Engine/Utils/Logger.h"
#include "Game/GameScene.h"

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

	ImGuiManager imGuiManager;
	imGuiManager.Initialize(&renderer, window.get());

	GameScene gameScene;
	gameScene.Startup(&renderer, window.get());

	while (true) {
		if (Window::ProcessMessage()) {
			break;
		}

		imGuiManager.NewFrame();

		// ゲームシーンの更新
		gameScene.Update();

		ImGui::ShowDemoWindow();

		// レンダリングの前処理
		renderer.PreRender();

		// ゲームシーンのレンダリング
		gameScene.Render();

		imGuiManager.EndFrame();

		// レンダリングの後処理
		renderer.PostRender();
	}

	gameScene.Shutdown();

	imGuiManager.Shutdown();

	renderer.Terminate();

	TextureManager::GetInstance().ReleaseUnusedTextures();
	TextureManager::GetInstance().Shutdown();

	return 0;
}
