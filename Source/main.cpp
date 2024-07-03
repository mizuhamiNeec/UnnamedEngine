#include <format>
#include <Windows.h>

#ifdef _DEBUG
#include <imgui/imgui.h>
#include "Engine/ImGuiManager/ImGuiManager.h"
#endif

#include "Engine/Renderer/D3D12.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Utils/ClientProperties.h"
#include "Engine/Utils/ConvertString.h"
#include "Engine/Utils/Logger.h"
#include "Game/GameScene.h"
#include "../Input.h"

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
	renderer.Init(window.get());

	// ---------------------------------------------------------------------------
	// 入力
	// ---------------------------------------------------------------------------
	Input* input = new Input();
	input->Init(window.get());

#ifdef _DEBUG
	ImGuiManager imGuiManager;
	imGuiManager.Init(&renderer, window.get());
#endif

	GameScene gameScene;
	gameScene.Init(&renderer, window.get());

	while (true) {
		if (Window::ProcessMessage()) {
			break;
		}

		input->Update();

		if (input->TriggerKey(DIK_0)) {
			Log("TriggerKey 0\n");
		}

		if (input->PushKey(DIK_0)) {
			Log("PressKey 0\n");
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

	delete input;

	renderer.Shutdown();

	TextureManager::GetInstance().Shutdown();

	return 0;
}
