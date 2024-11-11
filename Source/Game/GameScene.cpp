#include "GameScene.h"

#include <format>

#include "../Engine/Lib/Timer/EngineTimer.h"
#include "../Engine/Model/ModelManager.h"
#include "../Engine/Window/WindowsUtils.h"

#include "../../Player.h"
#include "../../RailCamera.h"

#include "../ImGuiManager/ImGuiManager.h"

#include "../Lib/Console/ConVar.h"
#include "../Lib/Console/ConVars.h"
#include "../Lib/Math/MathLib.h"

#include "../Object3D/Object3D.h"

#include "../Particle/ParticleCommon.h"

#include "../Sprite/SpriteCommon.h"

#include "../TextureManager/TextureManager.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

void GameScene::Init(
	D3D12* renderer, Window* window,
	SpriteCommon* spriteCommon, Object3DCommon* object3DCommon,
	ModelCommon* modelCommon, ParticleCommon* particleCommon,
	EngineTimer* engineTimer
) {
	renderer_ = renderer;
	window_ = window;
	spriteCommon_ = spriteCommon;
	object3DCommon_ = object3DCommon;
	modelCommon_ = modelCommon;
	particleCommon_ = particleCommon;
	timer_ = engineTimer;

#pragma region テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/circle.png");
#pragma endregion

#pragma region スプライト類
	sprite_ = std::make_unique<Sprite>();
	sprite_->Init(spriteCommon_, "./Resources/Textures/uvChecker.png");
	sprite_->SetSize({512.0f, 512.0f, 0.0f});
#pragma endregion

#pragma region 3Dオブジェクト類
	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("axis.obj");

	object3D_ = std::make_unique<Object3D>();
	object3D_->Init(object3DCommon_, modelCommon_);
	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object3D_->SetModel("axis.obj");
	object3D_->SetPos({1.0f, -0.3f, 0.6f});
#pragma endregion

#pragma region パーティクル類
	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleCommon_, "./Resources/Textures/circle.png");


	ModelManager::GetInstance()->LoadModel("sky.obj");
	sky_ = std::make_unique<Object3D>();
	sky_->Init(object3DCommon_, modelCommon_);
	sky_->SetModel("sky.obj");
	sky_->SetPos({ 0.0f,0.0f,0.0f });
	sky_->SetLighting(false);

	player_ = std::make_unique<Player>();
	player_->Init(object3DCommon_, modelCommon_);
	player_->SetModel("axis.obj");
	player_->SetPos({ 0.0f,0.0f,0.0f });
	player_->Initialize();

	ModelManager::GetInstance()->LoadModel("rail.obj");
	PlaceRailsAlongSpline();
#pragma endregion

	railCamera_ = std::make_unique<RailCamera>();
	railCamera_->Initialize(object3DCommon_->GetDefaultCamera(), { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f });
}

void GameScene::Update() {
	static bool toggle = false;
	if (Input::GetInstance()->TriggerKey(DIK_V)) {
		toggle = !toggle;
	}

	if (toggle) {
		railCamera_->Update();
	}

	transform_.rotate.y += 0.003f;
	Mat4 worldMat = Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate);
	Mat4 cameraMat = Mat4::Affine(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	Mat4 viewMat = cameraMat.Inverse();
	Mat4 projectionMat = Mat4::PerspectiveFovMat(
		fov_ * Math::deg2Rad, // FieldOfView 90 degree!!
		static_cast<float>(window_->GetClientWidth()) / static_cast<float>(window_->GetClientHeight()),
		0.01f,
		1000.0f
	);
	Mat4 worldViewProjMat = worldMat * viewMat * projectionMat;

	TransformationMatrix* ptr = transformation->GetPtr<TransformationMatrix>();
	ptr->wvp = worldViewProjMat;
	ptr->world = worldMat;

#ifdef _DEBUG
	ImGui::Begin("Sprites");
	for (uint32_t i = 0; i < sprites_.size(); ++i) {
		if (ImGui::CollapsingHeader(std::format("Sprite {}", i).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			Vec3 pos = sprites_[i]->GetPos();
			Vec3 rot = sprites_[i]->GetRot();
			Vec3 size = sprites_[i]->GetSize();
			Vec2 anchor = sprites_[i]->GetAnchorPoint();
			Vec4 color = sprites_[i]->GetColor();
			if (ImGui::DragFloat3(std::format("pos##sprite{}", i).c_str(), &pos.x, 1.0f)) {
				sprites_[i]->SetPos(pos);
			}
			if (ImGui::DragFloat3(std::format("rot##sprite{}", i).c_str(), &rot.x, 0.01f)) {
				sprites_[i]->SetRot(rot);
			}
			if (ImGui::DragFloat3(std::format("scale##sprite{}", i).c_str(), &size.x, 0.01f)) {
				sprites_[i]->SetSize(size);
			}
			if (ImGui::DragFloat2(std::format("anchorPoint#sprite{}", i).c_str(), &anchor.x, 0.01f)) {
				sprites_[i]->SetAnchorPoint(anchor);
			}
			if (ImGui::ColorEdit4(std::format("color##sprite{}", i).c_str(), &color.x)) {
				sprites_[i]->SetColor(color);
			}
			ImGui::Separator();

			if (ImGui::TreeNode(std::format("UV##sprite{}", i).c_str())) {
				Vec2 uvPos = sprites_[i]->GetUvPos();
				Vec2 uvSize = sprites_[i]->GetUvSize();
				float uvRot = sprites_[i]->GetUvRot();
				if (ImGui::DragFloat2(std::format("pos##uv{}", i).c_str(), &uvPos.x, 0.01f)) {
					sprites_[i]->SetUvPos(uvPos);
				}
				if (ImGui::DragFloat2(std::format("scale##uv{}", i).c_str(), &uvSize.x, 0.01f)) {
					sprites_[i]->SetUvSize(uvSize);
				}
				if (ImGui::SliderAngle(std::format("rotZ##uv{}", i).c_str(), &uvRot)) {
					sprites_[i]->SetUvRot(uvRot);
				}

	ImGui::Begin("Test");
	ImGui::DragFloat3("Offset", &offset.x, 0.01f);
	ImGui::DragFloat("interpSpd", &interpSpeed, 0.01f);
	ImGui::End();

	object3D_->SetPos(
		object3D_->GetPos() + Vec3(1.0f, 0.0f, 1.0f) * timer_->GetDeltaTime()
	);

	{
		ImGui::Begin("Texture 0");
		static bool texindex = false;
		if (ImGui::Checkbox("Toggle Texture", &texindex)) {
			if (texindex) {
				sprites_[0]->ChangeTexture("./Resources/Textures/uvChecker.png");
			} else {
				sprites_[0]->ChangeTexture("./Resources/Textures/debugempty.png");
			}
		}
		ImGui::End();
	}

	ImGui::Begin("Object3D");
	// 一旦変数に格納
	Vec3 translate = object3D_->GetPos();
	Vec3 rotate = object3D_->GetRot();
	Vec3 scale = object3D_->GetScale();

	ImGui::DragFloat3("transform##obj", &translate.x, 0.01f);
	ImGui::DragFloat3("rotate##obj", &rotate.x, 0.01f);
	ImGui::DragFloat3("scale##obj", &scale.x, 0.01f);

	// ImGuiでの編集が終わったらSet
	object3D_->SetPos(translate);
	object3D_->SetRot(rotate);
	object3D_->SetScale(scale);
	ImGui::End();
#endif

	sprite_->Update();
	object3D_->Update();
	particle_->Update(timer_->GetDeltaTime());

	sky_->Update();

	player_->Update();

	for (const std::unique_ptr<Object3D>& rail : rails_) {
		rail->Update();
	}

	for (Sprite* sprite : sprites_) {
		sprite->Update();
	}
#ifdef _DEBUG
#pragma region cl_showpos
	if (ConVars::GetInstance().GetConVar("cl_showpos")->GetInt() == 1) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
		const ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking;
		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		// テキストのサイズを取得
		ImGuiIO io = ImGui::GetIO();
		std::string text = std::format(
			"name: {}\n"
			"pos : {:.2f} {:.2f} {:.2f}\n"
			"rot : {:.2f} {:.2f} {:.2f}\n"
			"vel : {:.2f}\n",
			WindowsUtils::GetWindowsUserName(),
			object3DCommon_->GetDefaultCamera()->GetPos().x, object3DCommon_->GetDefaultCamera()->GetPos().y, object3DCommon_->GetDefaultCamera()->GetPos().z,
			object3DCommon_->GetDefaultCamera()->GetRotate().x * Math::rad2Deg, object3DCommon_->GetDefaultCamera()->GetRotate().y * Math::rad2Deg, object3DCommon_->GetDefaultCamera()->GetRotate().z * Math::rad2Deg,
			0.0f
		);
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		// ウィンドウサイズをテキストサイズに基づいて設定
		ImVec2 windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f); // 余白を追加
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("##cl_showpos", nullptr, windowFlags);

		ImVec2 textPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float outlineSize = 1.0f;

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
#pragma endregion
#endif
}

void GameScene::Render() {
	//----------------------------------------
	// オブジェクト3D共通描画設定
	object3DCommon_->Render();
	//----------------------------------------

	object3D_->Draw();
	sky_->Draw();

	player_->Draw();

	for (const std::unique_ptr<Object3D>& rail : rails_) {
		rail->Draw();
	}

	//----------------------------------------
	// パーティクル共通描画設定
	particleCommon_->Render();
	//----------------------------------------
	particle_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleCommon_->Shutdown();
}

void GameScene::PlaceRailsAlongSpline() {
	const float railLength = 1.0f; // レール1つの長さ（メートル単位）
	const float delta = 0.0001f;

	// スプラインの全長を計算
	float splineLength = 0.0f;
	const int lengthSamples = 1000;
	Vec3 prevPos = Math::CatmullRomPosition(controlPoints, 0.0f);

	for (int i = 1; i <= lengthSamples; i++) {
		float t = static_cast<float>(i) / lengthSamples;
		Vec3 currentPos = Math::CatmullRomPosition(controlPoints, t);
		splineLength += (currentPos - prevPos).Length();
		prevPos = currentPos;
	}

	// レールの数を計算（スプラインの長さをレール1つの長さで割る）
	int numRails = static_cast<int>(std::ceil(splineLength / railLength));

	// 実際の配置間隔を計算（均等に分布させる）
	float actualSpacing = splineLength / (numRails - 1);

	// レールを配置
	float accumulatedLength = 0.0f;
	float currentT = 0.0f;
	prevPos = Math::CatmullRomPosition(controlPoints, 0.0f);

	for (int i = 0; i < numRails; ++i) {
		// 目標の距離
		float targetDistance = i * actualSpacing;

		// 目標の距離に達するまでtを進める
		while (accumulatedLength < targetDistance && currentT < 1.0f) {
			currentT += delta;
			Vec3 currentPos = Math::CatmullRomPosition(controlPoints, currentT);
			accumulatedLength += (currentPos - prevPos).Length();
			prevPos = currentPos;
		}

		// レールの位置を取得
		Vec3 railPosition = Math::CatmullRomPosition(controlPoints, currentT);

		// 接線ベクトルを計算
		Vec3 tangent;
		if (i == 0) {
			Vec3 nextPosition = Math::CatmullRomPosition(controlPoints, currentT + delta);
			tangent = (nextPosition - railPosition).Normalized();
		} else if (i == numRails - 1) {
			Vec3 prevPosition = Math::CatmullRomPosition(controlPoints, currentT - delta);
			tangent = (railPosition - prevPosition).Normalized();
		} else {
			Vec3 prevPosition = Math::CatmullRomPosition(controlPoints, currentT - delta);
			Vec3 nextPosition = Math::CatmullRomPosition(controlPoints, currentT + delta);
			tangent = (nextPosition - prevPosition).Normalized();
		}

		// レールのモデルを生成
		auto rail = std::make_unique<Object3D>();
		rail->Init(object3DCommon_, modelCommon_);
		rail->SetModel("rail.obj");
		rail->SetPos(railPosition);

		// 接線ベクトルから回転を計算
		float pitch = std::asin(-tangent.y);
		float yaw = std::atan2(tangent.x, tangent.z);
		rail->SetRot({ pitch, yaw, 0.0f });

		// レールをシーンに追加
		rails_.push_back(std::move(rail));
	}
}
