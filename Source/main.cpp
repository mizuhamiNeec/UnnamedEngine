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
#include "Game/GameScene.h"
#include "../Input.h"
#include "../Sprite.h"
#include "../SpriteManager.h"
#include "../Console.h"
#include "../ConVar.h"

//-----------------------------------------------------------------------------
// エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR pCmdLine, const int nCmdShow) {
	Console::Print(
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

	std::unique_ptr<D3D12> renderer = std::make_unique<D3D12>();
	renderer->Init(window.get());

	std::unique_ptr<SpriteManager> spriteManager = std::make_unique<SpriteManager>();
	spriteManager->Init(renderer.get());

	// ---------------------------------------------------------------------------
	// 入力
	// ---------------------------------------------------------------------------
	std::unique_ptr<Input> input = std::make_unique<Input>();
	input->Init(window.get());

#ifdef _DEBUG
	ImGuiManager imGuiManager;
	imGuiManager.Init(renderer.get(), window.get());

	Console console;
	console.Init();
#endif

#pragma region シーンの初期化

	GameScene gameScene;
	gameScene.Init(renderer.get(), window.get());

	Sprite* sprite = new Sprite();
	sprite->Init();

#pragma endregion

	while (true) {
		if (window->ProcessMessage()) {
			break; // ゲームループを抜ける
		}

		input->Update();

		if (input->TriggerKey(DIK_0)) {
			Console::Print("TriggerKey 0\n");
		}

		if (input->PushKey(DIK_0)) {
			Console::Print("PressKey 0\n");
		}

		if (input->TriggerKey(DIK_GRAVE)) {
			Console::ToggleConsole();
			Console::Print("PressKey `\n");
		}

		ConVar cl_showpos("cl_showpos", 0, "Draw current position at top of screen");

		//Console::Print("This is Normal.", kConsoleNormal);
		//Console::Print("This is Error.", kConsoleError);
		//Console::Print("This is Warning.", kConsoleWarning);

#ifdef _DEBUG
		imGuiManager.NewFrame();
#endif

		// ゲームシーンの更新
		gameScene.Update();

		console.Update();

		// レンダリングの前処理
		renderer->PreRender();

		// ゲームシーンのレンダリング
		gameScene.Render();

#ifdef _DEBUG
		imGuiManager.EndFrame();
#endif

		// レンダリングの後処理
		renderer->PostRender();
	}

	gameScene.Shutdown();

#ifdef _DEBUG
	imGuiManager.Shutdown();
#endif

	delete sprite;

	TextureManager::GetInstance().Shutdown();

	return 0;
}
