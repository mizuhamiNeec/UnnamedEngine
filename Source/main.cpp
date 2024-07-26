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
#include "imgui/imgui_internal.h"

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

	ConVar cl_showpos("cl_showpos", 1, "Draw current position at top of screen");
	ConVar cl_showfps("cl_showfps", 1, "Draw fps meter (1 = fps)");

	while (true) {
		if (window->ProcessMessage()) {
			break; // ゲームループを抜ける
		}

		input->Update();

		if (input->TriggerKey(DIK_0)) {
			Console::Print("TriggerKey 0");
		}

		if (input->PushKey(DIK_0)) {
			Console::Print("PressKey 0");
		}

		if (input->TriggerKey(DIK_GRAVE)) {
			Console::ToggleConsole();
			Console::Print("PressKey `");
		}

#ifdef _DEBUG
		imGuiManager.NewFrame();
#endif

		// ゲームシーンの更新
		gameScene.Update();

		console.Update();

		if (ConVars::GetInstance().GetConVar("cl_showpos")->GetInt() == 1) {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });

			ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoBackground |
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings;

			ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
			ImVec2 windowSize = ImVec2(1080.0f, 80.0f);

			windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
			windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;

			ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
			ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

			ImGui::Begin("##cl_showpos", nullptr, windowFlags);

			ImVec2 textPos = ImGui::GetCursorScreenPos();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			float outlineSize = 1.0f;

			std::string text = std::format(
				"name: {}\n"
				"pos : {:.2f} {:.2f} {:.2f}\n"
				"rot : {:.2f} {:.2f} {:.2f}\n"
				"vel : {:.2f}\n",
				"unnamed",
				0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.0f,
				0.0f
			);

			ImU32 textColor = IM_COL32(255, 255, 255, 255);
			ImU32 outlineColor = IM_COL32(0, 0, 0, 94);

			TextOutlined(
				drawList,
				textPos,
				text.c_str(),
				textColor,
				outlineColor,
				outlineSize
			);

			ImGui::PopStyleVar();

			ImGui::End();
		}

		if (ConVars::GetInstance().GetConVar("cl_showfps")->GetInt() == 1) {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });

			ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoBackground |
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings;

			ImVec2 windowPos = ImVec2(0.0f, 128.0f);
			ImVec2 windowSize = ImVec2(1080.0f, 80.0f);

			windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
			windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;

			ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
			ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

			ImGui::Begin("##cl_showfps", nullptr, windowFlags);

			ImVec2 textPos = ImGui::GetCursorScreenPos();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			float outlineSize = 1.0f;

			std::string text = std::format("{:.2f} fps", ImGui::GetIO().Framerate);

			ImU32 textColor = IM_COL32(255, 0, 0, 255);
			ImU32 outlineColor = IM_COL32(0, 0, 0, 94);

			TextOutlined(
				drawList,
				textPos,
				text.c_str(),
				textColor,
				outlineColor,
				outlineSize
			);

			ImGui::PopStyleVar();

			ImGui::End();
		}

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
