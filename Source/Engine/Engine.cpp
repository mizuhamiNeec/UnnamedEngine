#include "Engine.h"

#include "../Input/Input.h"
#include "Camera/Camera.h"

#ifdef _DEBUG
#include "imgui/imgui_internal.h"
#endif

#include "Debug/Debug.h"

#include "Lib/Console/Console.h"
#include "Lib/Console/ConVarManager.h"
#include "Lib/Utils/ClientProperties.h"
#include "Model/ModelManager.h"
#include "Object3D/Object3DCommon.h"
#include "Particle/ParticleCommon.h"
#include "Renderer/D3D12.h"
#include "Renderer/SrvManager.h"
#include "TextureManager/TextureManager.h"
#include "Window/Window.h"
#include "Window/WindowsUtils.h"

Engine::Engine() = default;

void Engine::Run() {
	Init();
	while (true) {
		if (Window::ProcessMessage() || bWishShutdown) break; // ゲームループを抜ける
		Update();
	}
	Shutdown();
}

void Engine::DrawGrid(const float gridSize, const float range, const Vec4& color, const Vec4& majorColor,
                      const Vec4& axisColor, const Vec4& minorColor) {
	//const float range = 16384.0f;
	constexpr float majorInterval = 1024.0f;
	const float minorInterval = gridSize * 8.0f;

	for (float x = -range; x <= range; x += gridSize) {
		Vec4 lineColor = color;
		if (fmod(x, majorInterval) == 0) {
			lineColor = majorColor;
		}
		else if (fmod(x, minorInterval) == 0) {
			lineColor = minorColor;
		}
		if (x == 0) {
			lineColor = axisColor;
		}
		Debug::DrawLine(Vec3(x, 0, -range), Vec3(x, 0, range), lineColor);
	}

	for (float z = -range; z <= range; z += gridSize) {
		Vec4 lineColor = color;
		if (fmod(z, majorInterval) == 0) {
			lineColor = majorColor;
		}
		else if (fmod(z, minorInterval) == 0) {
			lineColor = minorColor;
		}
		if (z == 0) {
			lineColor = axisColor;
		}
		Debug::DrawLine(Vec3(-range, 0, z), Vec3(range, 0, z), lineColor);
	}
}

void Engine::Init() {
	RegisterConsoleCommandsAndVariables();

	// ウィンドウの作成
	window_ = std::make_unique<Window>(L"Window", kClientWidth, kClientHeight);
	window_->Create(nullptr);

	// レンダラ
	renderer_ = std::make_unique<D3D12>();
	renderer_->Init(window_.get());

	// SRVマネージャの初期化
	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Init(renderer_.get());

#ifdef _DEBUG
	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Init(renderer_.get(), window_.get(), srvManager_.get());

	console_ = std::make_unique<Console>();
#endif

	// テクスチャマネージャ
	TextureManager::GetInstance()->Init(renderer_.get(), srvManager_.get());

	// 3Dモデルマネージャ
	ModelManager::GetInstance()->Init(renderer_.get());

	// カメラの作成
	camera_ = std::make_unique<Camera>();
	camera_->SetPos({0.0f, 0.0f, -10.0f});

	// モデル
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Init(renderer_.get());

	// オブジェクト3D
	object3DCommon_ = std::make_unique<Object3DCommon>();
	object3DCommon_->Init(renderer_.get());
	object3DCommon_->SetDefaultCamera(camera_.get());

	// スプライト
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Init(renderer_.get());

	// パーティクル
	particleCommon_ = std::make_unique<ParticleCommon>();
	particleCommon_->Init(renderer_.get(), srvManager_.get());
	particleCommon_->SetDefaultCamera(camera_.get());

	// ライン
	lineCommon_ = std::make_unique<LineCommon>();
	lineCommon_->Init(renderer_.get());
	lineCommon_->SetDefaultCamera(camera_.get());

	Debug::Init(lineCommon_.get());

	// 入力
	input_ = Input::GetInstance();
	input_->Init(window_.get());

	//-------------------------------------------------------------------------
	// コマンドのリセット
	//-------------------------------------------------------------------------
	HRESULT hr = renderer_->GetCommandAllocator()->Reset();
	assert(SUCCEEDED(hr));
	hr = renderer_->GetCommandList()->Reset(
		renderer_->GetCommandAllocator(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	time_ = std::make_unique<EngineTimer>();

	// シーン
	gameScene_ = std::make_unique<GameScene>();
	gameScene_->Init(
		renderer_.get(),
		window_.get(),
		spriteCommon_.get(),
		object3DCommon_.get(),
		modelCommon_.get(),
		particleCommon_.get(),
		time_.get()
	);

	hr = renderer_->GetCommandList()->Close();
	assert(SUCCEEDED(hr));
}

void Engine::Update() const {
#ifdef _DEBUG
	ImGuiManager::NewFrame();
	Console::Update();
#endif

	time_->StartFrame();

	/* ----------- 更新処理 ---------- */
	Input::GetInstance()->Update();

	// コンソール表示切り替え
	if (Input::GetInstance()->TriggerKey(DIK_GRAVE)) {
		Console::SubmitCommand("toggleconsole");
	}


#ifdef _DEBUG
	static bool firstReset = true; // 初回リセットフラグ
	
	if (ImGui::GetIO().MouseDown[1]) {
		// マウスの移動量を取得
		POINT currentCursorPos;
		GetCursorPos(&currentCursorPos);
		static POINT prevCursorPos = {
			static_cast<LONG>(window_->GetClientWidth() / 2), static_cast<LONG>(window_->GetClientHeight() / 2)
		};

		if (!firstReset) {
			int deltaX = currentCursorPos.x - prevCursorPos.x;
			int deltaY = currentCursorPos.y - prevCursorPos.y;

			// カメラの回転を更新
			float sensitivity = std::stof(ConVarManager::GetConVar("sensitivity")->GetValueAsString()) * 0.022f;
			camera_->SetRotate(
				camera_->GetRotate() + Vec3(deltaY * sensitivity, deltaX * sensitivity, 0.0f) * Math::deg2Rad);

			Vec3 moveInput = { 0.0f, 0.0f, 0.0f };

			auto input = Input::GetInstance();
			if (input->PushKey(DIK_W) && !input->PushKey(DIK_S)) {
				moveInput.z = 1.0f;
			} else if (!input->PushKey(DIK_W) && input->PushKey(DIK_S)) {
				moveInput.z = -1.0f;
			}

			if (input->PushKey(DIK_D) && !input->PushKey(DIK_A)) {
				moveInput.x = 1.0f;
			} else if (!input->PushKey(DIK_D) && input->PushKey(DIK_A)) {
				moveInput.x = -1.0f;
			}

			if (input->PushKey(DIK_E) && !input->PushKey(DIK_Q)) {
				moveInput.y = 1.0f;
			} else if (!input->PushKey(DIK_E) && input->PushKey(DIK_Q)) {
				moveInput.y = -1.0f;
			}

			moveInput.Normalize();

			Quaternion camRot = Quaternion::Euler(camera_->GetRotate());
			Vec3 cameraForward = camRot * Vec3::forward;
			Vec3 cameraRight = camRot * Vec3::right;
			Vec3 cameraUp = Vec3::up;

			camera_->SetPos(
				camera_->GetPos() + (cameraForward * moveInput.z + cameraRight * moveInput.x + cameraUp *
					moveInput.y) * 5.0f * time_->GetScaledDeltaTime()
			);
		}

		// カーソルをウィンドウの中央にリセット
		POINT centerCursorPos = {
			static_cast<LONG>(window_->GetClientWidth() / 2), static_cast<LONG>(window_->GetClientHeight() / 2)
		};
		ClientToScreen(window_->GetWindowHandle(), &centerCursorPos); // クライアント座標をスクリーン座標に変換
		SetCursorPos(centerCursorPos.x, centerCursorPos.y);
		prevCursorPos = centerCursorPos;

		firstReset = false; // 初回リセット完了
	} else {
		firstReset = true; // マウスボタンが離されたら初回リセットフラグをリセット
	}
#endif

	camera_->SetAspectRatio(
		static_cast<float>(window_->GetClientWidth()) / static_cast<float>(window_->GetClientHeight())
	);
	camera_->Update();

	// ゲームシーンの更新
	gameScene_->Update();

	// グリッドの表示
	DrawGrid(
		1.0f,
		64,
		{.x = 0.28f, .y = 0.28f, .z = 0.28f, .w = 1.0f},
		{.x = 0.39f, .y = 0.2f, .z = 0.02f, .w = 1.0f},
		{.x = 0.0f, .y = 0.39f, .z = 0.39f, .w = 1.0f},
		{.x = 0.39f, .y = 0.39f, .z = 0.39f, .w = 1.0f}
	);

#ifdef _DEBUG
	ImGuiViewportP* viewport = static_cast<ImGuiViewportP*>(static_cast<void*>(ImGui::GetMainViewport()));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.0f);

	if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, 38, window_flags)) {
		if (ImGui::BeginMenuBar()) {
			ImGui::PopStyleVar();

			ImGui::Text("ハリボテ");

			ImGui::BeginDisabled(true);

			// アングルスナップ
			{
				const float windowHeight = ImGui::GetWindowSize().y;
				const char* items[] = {
					"0.25°", "0.5°", "1°", "5°", "5.625°", "11.25°", "15°", "22.5°", "30°", "45°", "90°"
				};
				static int itemCurrentIndex = 6;
				const char* comboLabel = items[itemCurrentIndex];

				ImGui::Text("Angle: ");

				// 垂直中央に配置
				float comboHeight = ImGui::GetFrameHeight();
				float offsetY = (windowHeight - comboHeight) * 0.5f;
				ImGui::SetCursorPosY(offsetY);

				if (ImGui::BeginCombo("##angle", comboLabel)) {
					for (int n = 0; n < IM_ARRAYSIZE(items); ++n) {
						const bool isSelected = (itemCurrentIndex == n);
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}

			ImGui::EndDisabled();

			ImGui::EndMenuBar();
		}
		ImGui::End();
	}
#endif


#ifdef _DEBUG // cl_showfps
	if (ConVarManager::GetConVar("cl_showfps")->GetValueAsString() != "0") {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

		ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImVec2 windowPos = ImVec2(0.0f, 128.0f);

		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
		std::string text;
		float fps;
		if (ConVarManager::GetConVar("cl_showfps")->GetValueAsString() == "1") {
			fps = 1.0f / time_->GetScaledDeltaTime();
		}
		if (ConVarManager::GetConVar("cl_showfps")->GetValueAsString() == "2") {
			ImGuiIO io = ImGui::GetIO();
			fps = io.Framerate;
		}

		text = std::format("{:.2f} fps", fps);

		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f); // 余白を追加
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("##cl_showfps", nullptr, windowFlags);

		ImVec2 textPos = ImGui::GetCursorScreenPos();

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		float outlineSize = 1.0f;

		ImU32 textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorError);
		if (fps >= 59.9f) {
			textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorFloat);
		} else if (fps >= 29.9f) {
			textColor = ImGui::ColorConvertFloat4ToU32(kConsoleColorWarning);
		}

		ImU32 outlineColor = IM_COL32(0, 0, 0, 94);

		ImGuiManager::TextOutlined(
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

#ifdef _DEBUG
	Debug::Update();
#endif

	/* ---------- Pre ----------- */
	renderer_->PreRender();
	srvManager_->PreDraw();
	/* ---------- コマンド積み ----------- */

	gameScene_->Render();

	//-------------------------------------------------------------------------
	lineCommon_->Render();
	Debug::Draw();
	//-------------------------------------------------------------------------

#ifdef _DEBUG
	imGuiManager_->EndFrame();
#endif

	/* ---------- Post ----------- */
	renderer_->PostRender();
	/* ---------- ゲームループ終了 ---------- */

	time_->EndFrame();
}

void Engine::Shutdown() const {
	gameScene_->Shutdown();

	ModelManager::Shutdown();
	TextureManager::Shutdown();

	Debug::Shutdown();

#ifdef _DEBUG
	// ImGuiManagerのシャットダウンは最後に行う
	if (imGuiManager_) {
		imGuiManager_->Shutdown();
	}
#endif
}

void Engine::RegisterConsoleCommandsAndVariables() {
	// コンソールコマンドを登録
	Console::RegisterCommand("clear", Console::Clear);
	Console::RegisterCommand("cls", Console::Clear);
	Console::RegisterCommand("help", Console::Help);
	Console::RegisterCommand("toggleconsole", Console::ToggleConsole);
	Console::RegisterCommand("neofetch", Console::Neofetch);
	Console::SubmitCommand("neofetch");
	Console::RegisterCommand("quit", Quit);
	// コンソール変数を登録
	ConVarManager::RegisterConVar<int>("cl_showpos", 0, "Draw current position at top of screen");
	ConVarManager::RegisterConVar<int>("cl_showfps", 0, "Draw fps meter (1 = fps, 2 = smooth)");
	ConVarManager::RegisterConVar<int>("cl_fpsmax", kMaxFps, "Frame rate limiter");
	ConVarManager::RegisterConVar<std::string>("name", "unnamed", "Current user name", ConVarFlags::ConVarFlags_Notify);
	Console::SubmitCommand(std::format("name {}", WindowsUtils::GetWindowsUserName()));
	ConVarManager::RegisterConVar<float>("sensitivity", 2.0f, "Mouse sensitivity.");
	ConVarManager::RegisterConVar<float>("host_timescale", 1.0f, "Prescale the clock by this amount.");
}

void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
	bWishShutdown = true;
}

bool Engine::bWishShutdown = false;
