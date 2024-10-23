#include "Engine.h"

#include "Lib/Console/ConVars.h"
#include "Lib/Console/ConVar.h"
#include "Lib/Utils/ClientProperties.h"
#include "Renderer/D3D12.h"
#include "Lib/Console/Console.h"
#include "../ImGuiManager/ImGuiManager.h"
#include "Object3D/Object3DCommon.h"

#include "TextureManager/TextureManager.h"

#include "Window/Window.h"

void Engine::Run() {
	Initialize();
	Update();
	Shutdown();
}

Engine::Engine() = default;

void Engine::Initialize() {
	// ウィンドウの作成
	window_ = std::make_unique<Window>(L"Window", kClientWidth, kClientHeight);
	// ウィンドウの作成を試みる
	if (!window_->Create(nullptr)) {
		assert(false && "ウィンドウの作成に失敗した");
	}

	// レンダラ
	renderer_ = std::make_unique<D3D12>();
	renderer_->Init(window_.get());

#ifdef _DEBUG
	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Init(renderer_.get(), window_.get());

	console_ = std::make_unique<Console>();
#endif

	// テクスチャマネージャ
	TextureManager::GetInstance()->Initialize(renderer_.get());

	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Init(renderer_.get());

	object3DCommon_ = std::make_unique<Object3DCommon>();
	object3DCommon_->Initialize();

	// 入力
	input_ = std::make_unique<Input>();
	input_->Init(window_.get());

	// シーン
	gameScene_ = std::make_unique<GameScene>();
	gameScene_->Init(renderer_.get(), window_.get(), spriteCommon_.get(), object3DCommon_.get());
}

void Engine::Update() const {
	while (true) {
		/* ----------- 更新処理 ---------- */

		input_->Update();

		// コンソール表示切り替え
		if (input_->TriggerKey(DIK_GRAVE)) {
			Console::ToggleConsole();
		}

#ifdef _DEBUG
		imGuiManager_->NewFrame();
		console_->Update();
#endif

		// ゲームシーンの更新
		gameScene_->Update();

#ifdef _DEBUG // cl_showfps
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

			ImGuiIO io = ImGui::GetIO();

			std::string text = std::format("{:.2f} fps", io.Framerate);

			ImU32 textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorError);
			if (io.Framerate >= 59.9f) {
				textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorFloat);
			} else if (io.Framerate >= 29.9f) {
				textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorWarning);
			}

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
#endif

		/* ---------- Pre ----------- */

		renderer_->PreRender();

		/* ---------- コマンド積み ----------- */

		gameScene_->Render();

#ifdef _DEBUG
		imGuiManager_->EndFrame();
#endif

		/* ---------- Post ----------- */
		renderer_->PostRender();

		/* ---------- ゲームループ終了 ---------- */

		if (Window::ProcessMessage()) {
			break; // ゲームループを抜ける
		}
	}
}

void Engine::Shutdown() const {
	gameScene_->Shutdown();

	TextureManager::Shutdown();

	// ImGuiManagerのシャットダウンは最後に行う
	if (imGuiManager_) {
		imGuiManager_->Shutdown();
	}
}
