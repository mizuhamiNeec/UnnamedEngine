#include "Engine.h"

#include "../Input/Input.h"
#include "Camera/Camera.h"

#ifdef _DEBUG
#include "imgui/imgui_internal.h"
#endif

#include "Lib/Console/Console.h"
#include "Lib/Console/ConVarManager.h"
#include "Lib/Math/Quaternion/Quaternion.h"
#include "Lib/Utils/ClientProperties.h"
#include "Model/ModelManager.h"
#include "Object3D/Object3DCommon.h"
#include "Particle/ParticleCommon.h"
#include "Renderer/D3D12.h"
#include "Renderer/SrvManager.h"
#include "TextureManager/TextureManager.h"
#include "Window/Window.h"
#include "Window/WindowsUtils.h"

std::unique_ptr<Line> Engine::line_;

Engine::Engine() = default;

void Engine::Run() {
	Init();
	while (true) {
		if (Window::ProcessMessage()) break; // ゲームループを抜ける
		if (bWishShutdown) break;
		Update();
	}
	Shutdown();
}

void Engine::DrawLine(const Vec3& start, const Vec3& end, const Vec4& color) {
	line_->AddLine(start, end, color);
}

void Engine::DrawRay(const Vec3& position, const Vec3& dir, const Vec4& color) {
	line_->AddLine(position, position + dir, color);
}

void Engine::DrawAxis(const Vec3& position, const Quaternion& orientation) {
	const Vec3 right = orientation * Vec3::right;
	const Vec3 up = orientation * Vec3::up;
	const Vec3 forward = orientation * Vec3::forward;

	DrawRay(position, right, { 1.0f, 0.0f, 0.0f, 1.0f });
	DrawRay(position, up, { 0.0f, 1.0f, 0.0f, 1.0f });
	DrawRay(position, forward, { 0.0f, 0.0f, 1.0f, 1.0f });
}

void Engine::DrawCircle(
	const Vec3& position, const Quaternion& rotation, const float& radius, const Vec4& color,
	const uint32_t& segments
) {
	// 描画できない形状の場合
	if (radius <= 0.0f || segments <= 0) {
		// 返す
		return;
	}

	float angleStep = (360.0f / static_cast<float>(segments));

	// ラジアンに変換
	angleStep *= Math::deg2Rad;

	// とりあえず原点で計算する
	Vec3 lineStart = Vec3::zero;
	Vec3 lineEnd = Vec3::zero;

	for (int i = 0; i < static_cast<int>(segments); i++) {
		// 開始点
		lineStart.x = std::cos(angleStep * static_cast<float>(i));
		lineStart.y = std::sin(angleStep * static_cast<float>(i));
		lineStart.z = 0.0f;

		// 終了点
		lineEnd.x = std::cos(angleStep * static_cast<float>(i + 1));
		lineEnd.y = std::sin(angleStep * static_cast<float>(i + 1));
		lineEnd.z = 0.0f;

		// 目的の半径にする
		lineStart *= radius;
		lineEnd *= radius;

		// 回転させる
		lineStart = rotation * lineStart;
		lineEnd = rotation * lineEnd;

		// 目的の座標に移動
		lineStart += position;
		lineEnd += position;

		// なんやかんやした線を描画
		Engine::DrawLine(lineStart, lineEnd, color);
	}
}

void Engine::DrawArc(
	const float& startAngle, const float& endAngle, const Vec3& position,
	const Quaternion& orientation, const float& radius, const Vec4& color,
	const bool& drawChord, const bool& drawSector, const int& arcSegments
) {
	float arcSpan = Math::DeltaAngle(startAngle, endAngle);

	if (arcSpan <= 0) {
		arcSpan += 360.0f;
	}

	float angleStep = (arcSpan / static_cast<float>(arcSegments)) * Math::deg2Rad;
	float stepOffset = startAngle * Math::deg2Rad;

	Vec3 lineStart = Vec3::zero;
	Vec3 lineEnd = Vec3::zero;

	Vec3 arcStart = Vec3::zero;
	Vec3 arcEnd = Vec3::zero;

	Vec3 arcOrigin = position;

	for (int i = 0; i < arcSegments; i++) {
		const float stepStart = angleStep * static_cast<float>(i) + stepOffset;
		const float stepEnd = angleStep * static_cast<float>(i + 1) + stepOffset;

		lineStart.x = std::cos(stepStart);
		lineStart.y = std::sin(stepStart);
		lineStart.z = 0.0f;

		lineEnd.x = std::cos(stepEnd);
		lineEnd.y = std::sin(stepEnd);
		lineEnd.z = 0.0f;

		lineStart *= radius;
		lineEnd *= radius;

		lineStart = orientation * lineStart;
		lineEnd = orientation * lineEnd;

		lineStart += position;
		lineEnd += position;

		if (i == 0) {
			arcStart = lineStart;
		}

		if (i == arcSegments - 1) {
			arcEnd = lineEnd;
		}

		Engine::DrawLine(lineStart, lineEnd, color);
	}

	if (drawChord) {
		Engine::DrawLine(arcStart, arcEnd, color);
	}
	if (drawSector) {
		Engine::DrawLine(arcStart, arcOrigin, color);
		Engine::DrawLine(arcEnd, arcOrigin, color);
	}
}

void Engine::DrawArrow(
	const Vec3& position, const Vec3& direction,
	const Vec4& color,
	float headSize
) {
	// 矢印の終点
	const Vec3 end = position + direction;

	// 頭のりサイズ
	headSize = min((position - end).Length(), headSize);

	// 矢印の方向を正規化
	const Vec3 dirNormalized = direction.Normalized();

	// 矢印の頭部を描画するための垂直なベクトルを計算
	Vec3 up = Vec3::up; // Y軸を基準に取る（特殊ケースで方向と一致する場合を後で処理）
	if (dirNormalized.IsParallel(up)) {
		up = Vec3::right; // 代わりにX軸を使用
	}
	const Vec3 right = dirNormalized.Cross(up).Normalized();
	const Vec3 adjustedUp = right.Cross(dirNormalized).Normalized();

	// 頭部の羽根を描画
	const Vec3 arrowLeft = end - (dirNormalized * headSize) + (right * headSize * 0.5f);
	const Vec3 arrowRight = end - (dirNormalized * headSize) - (right * headSize * 0.5f);

	// 主体の線
	DrawLine(position, end, color);

	// 羽根の線
	DrawLine(end, arrowLeft, color);
	DrawLine(end, arrowRight, color);
}

void Engine::DrawQuad(
	const Vec3& pointA, const Vec3& pointB, const Vec3& pointC, const Vec3& pointD, const Vec4& color
) {
	Engine::DrawLine(pointA, pointB, color);
	Engine::DrawLine(pointB, pointC, color);
	Engine::DrawLine(pointC, pointD, color);
	Engine::DrawLine(pointD, pointA, color);
}

void Engine::DrawRect(const Vec3& position, const Quaternion& orientation, const Vec2& extent, const Vec4& color) {
	Vec3 rightOffset = Vec3::right * extent.x * 0.5f;
	Vec3 upOffset = Vec3::up * extent.y * 0.5f;

	Vec3 offsetA = orientation * (rightOffset + upOffset);
	Vec3 offsetB = orientation * (-rightOffset + upOffset);
	Vec3 offsetC = orientation * (-rightOffset - upOffset);
	Vec3 offsetD = orientation * (rightOffset - upOffset);

	DrawQuad(
		position + offsetA,
		position + offsetB,
		position + offsetC,
		position + offsetD,
		color
	);
}

void Engine::DrawRect(
	const Vec2& point1, const Vec2& point2, const Vec3& origin, const Quaternion& orientation,
	const Vec4& color
) {
	float extentX = abs(point1.x - point2.x);
	float extentY = abs(point1.y - point2.y);

	Vec3 rotatedRight = orientation * Vec3::right;
	Vec3 rotatedUp = orientation * Vec3::up;

	Vec3 pointA = origin + rotatedRight * point1.x + rotatedUp * point1.y;
	Vec3 pointB = pointA + rotatedRight * extentX;
	Vec3 pointC = pointB + rotatedUp * extentY;
	Vec3 pointD = pointA + rotatedUp * extentY;

	DrawQuad(pointA, pointB, pointC, pointD, color);
}

void Engine::DrawSphere(
	const Vec3& position, const Quaternion& orientation, float& radius, const Vec4& color, int segments = 4
) {
	if (radius <= 0) {
		radius = 0.01f;
	}
	segments = max(segments, 2);

	int doubleSegments = segments * 2;

	float meridianStep = 180.0f / static_cast<float>(segments);

	for (int i = 0; i < segments; i++) {
		DrawCircle(
			position,
			orientation * Quaternion::Euler(0, meridianStep * static_cast<float>(i) * Math::deg2Rad, 0), radius,
			color,
			doubleSegments
		);
	}

	Vec3 verticalOffset = Vec3::zero;
	float parallelAngleStep = Math::pi / static_cast<float>(segments);
	float stepRadius = 0.0f;

	for (int i = 1; i < segments; i++) {
		float stepAngle = parallelAngleStep * static_cast<float>(i);
		verticalOffset = (orientation * Vec3::up) * cos(stepAngle) * radius;
		stepRadius = sin(stepAngle) * radius;

		DrawCircle(
			position + verticalOffset, orientation * Quaternion::Euler(90.0f * Math::deg2Rad, 0, 0), stepRadius,
			color,
			doubleSegments
		);
	}
}

void Engine::DrawBox(const Vec3& position, const Quaternion& orientation, Vec3& size, const Vec4& color) {
	Vec3 offsetX = orientation * Vec3::right * size.x * 0.5f;
	Vec3 offsetY = orientation * Vec3::up * size.y * 0.5f;
	Vec3 offsetZ = orientation * Vec3::forward * size.z * 0.5f;

	Vec3 pointA = -offsetX + offsetY;
	Vec3 pointB = offsetX + offsetY;
	Vec3 pointC = offsetX - offsetY;
	Vec3 pointD = -offsetX - offsetY;

	DrawRect(position - offsetZ, orientation, { size.x, size.y }, color);
	DrawRect(position + offsetZ, orientation, { size.x, size.y }, color);

	Engine::DrawLine(position + (pointA - offsetZ), position + (pointA + offsetZ), color);
	Engine::DrawLine(position + (pointB - offsetZ), position + (pointB + offsetZ), color);
	Engine::DrawLine(position + (pointC - offsetZ), position + (pointC + offsetZ), color);
	Engine::DrawLine(position + (pointD - offsetZ), position + (pointD + offsetZ), color);
}

void Engine::DrawCylinder(
	const Vec3& position, const Quaternion& orientation, const float& height, const float& radius, const Vec4& color,
	const bool& drawFromBase
) {
	Vec3 localUp = orientation * Vec3::up;
	Vec3 localRight = orientation * Vec3::right;
	Vec3 localForward = orientation * Vec3::forward;

	Vec3 basePositionOffset = drawFromBase ? Vec3::zero : (localUp * height * 0.5f);
	Vec3 basePosition = position - basePositionOffset;
	Vec3 topPosition = basePosition + localUp * height;

	Quaternion circleOrientation = orientation * Quaternion::Euler(90.0f * Math::deg2Rad, 0, 0);

	Vec3 pointA = basePosition + localRight * radius;
	Vec3 pointB = basePosition + localForward * radius;
	Vec3 pointC = basePosition - localRight * radius;
	Vec3 pointD = basePosition - localForward * radius;

	Engine::DrawRay(pointA, localUp * height, color);
	Engine::DrawRay(pointB, localUp * height, color);
	Engine::DrawRay(pointC, localUp * height, color);
	Engine::DrawRay(pointD, localUp * height, color);

	DrawCircle(basePosition, circleOrientation, radius, color, 32);
	DrawCircle(topPosition, circleOrientation, radius, color, 32);
}

void Engine::DrawCapsule(
	const Vec3& position, const Quaternion& orientation, const float& height, float& radius, const Vec4&
	color,
	const bool& drawFromBase
) {
	radius = std::clamp(radius, 0.0f, height * 0.5f);
	Vec3 localUp = orientation * Vec3::up;
	Quaternion arcOrientation = orientation * Quaternion::Euler(0, 90.0f * Math::deg2Rad, 0);

	Vec3 basePositionOffset = drawFromBase ? Vec3::zero : (localUp * height * 0.5f);
	Vec3 baseArcPosition = position + localUp * radius - basePositionOffset;
	DrawArc(180, 360, baseArcPosition, orientation, radius, color);
	DrawArc(180, 360, baseArcPosition, arcOrientation, radius, color);

	float cylinderHeight = height - radius * 2.0f;
	DrawCylinder(baseArcPosition, orientation, cylinderHeight, radius, color, true);

	Vec3 topArcPosition = baseArcPosition + localUp * cylinderHeight;

	DrawArc(0, 180, topArcPosition, orientation, radius, color);
	DrawArc(0, 180, topArcPosition, arcOrientation, radius, color);
}

void Engine::DrawGrid(const float gridSize, const float range, const Vec4& color, const Vec4& majorColor, const Vec4& axisColor, const Vec4& minorColor) {
	//const float range = 16384.0f;
	const float majorInterval = 1024.0f;
	const float minorInterval = gridSize * 8.0f;

	for (float x = -range; x <= range; x += gridSize) {
		Vec4 lineColor = color;
		if (fmod(x, majorInterval) == 0) {
			lineColor = majorColor;
		} else if (fmod(x, minorInterval) == 0) {
			lineColor = minorColor;
		}
		if (x == 0) {
			lineColor = axisColor;
		}
		DrawLine(Vec3(x, 0, -range), Vec3(x, 0, range), lineColor);
	}

	for (float z = -range; z <= range; z += gridSize) {
		Vec4 lineColor = color;
		if (fmod(z, majorInterval) == 0) {
			lineColor = majorColor;
		} else if (fmod(z, minorInterval) == 0) {
			lineColor = minorColor;
		}
		if (z == 0) {
			lineColor = axisColor;
		}
		DrawLine(Vec3(-range, 0, z), Vec3(range, 0, z), lineColor);
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
	camera_->SetPos({ 0.0f, 0.0f, -10.0f });

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

	line_ = std::make_unique<Line>(lineCommon_.get());

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
		{ .x = 0.28f, .y = 0.28f, .z = 0.28f, .w = 1.0f },
		{ .x = 0.39f, .y = 0.2f, .z = 0.02f, .w = 1.0f },
		{ .x = 0.0f, .y = 0.39f, .z = 0.39f, .w = 1.0f },
		{ .x = 0.39f, .y = 0.39f, .z = 0.39f, .w = 1.0f }
	);

#ifdef _DEBUG
	ImGuiViewportP* viewport = static_cast<ImGuiViewportP*>(static_cast<void*>(ImGui::GetMainViewport()));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.0f);

	if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, 38, window_flags)) {
		if (ImGui::BeginMenuBar()) {
			ImGui::PopStyleVar();

			ImGui::Text("ハリボテ");

			ImGui::BeginDisabled(true);

			// アングルスナップ
			{
				const float windowHeight = ImGui::GetWindowSize().y;
				const char* items[] = { "0.25°", "0.5°", "1°", "5°", "5.625°", "11.25°", "15°", "22.5°", "30°", "45°", "90°" };
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

	// ライン更新
	line_->Update();

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

	/* ---------- Pre ----------- */
	renderer_->PreRender();
	srvManager_->PreDraw();
	/* ---------- コマンド積み ----------- */

	gameScene_->Render();

	//-------------------------------------------------------------------------
	lineCommon_->Render();
	// ライン描画
	line_->Draw();
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

	line_.reset();

	ModelManager::Shutdown();
	TextureManager::Shutdown();

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
	ConVarManager::RegisterConVar<int>("cl_showpos", 1, "Draw current position at top of screen");
	ConVarManager::RegisterConVar<int>("cl_showfps", 2, "Draw fps meter (1 = fps, 2 = smooth)");
	ConVarManager::RegisterConVar<int>("cl_fpsmax", kMaxFps, "Frame rate limiter");
	ConVarManager::RegisterConVar<std::string>("name", "unnamed", "Current user name", ConVarFlags::ConVarFlags_Notify);
	Console::SubmitCommand(std::format("name {}", WindowsUtils::GetWindowsUserName()));
	ConVarManager::RegisterConVar<float>("sensitivity", 1.0f, "Mouse sensitivity.");
	ConVarManager::RegisterConVar<float>("host_timescale", 1.0f, "Prescale the clock by this amount.");
}

void Engine::Quit([[maybe_unused]] const std::vector<std::string>& args) {
	bWishShutdown = true;
}
