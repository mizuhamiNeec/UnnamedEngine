#include "GameScene.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>

#include "../../Player.h"
#include "../../RailMover.h"
#include "../Engine/LevelData/LevelData.h"
#include "../Engine/Lib/Timer/EngineTimer.h"
#include "../Engine/Model/ModelManager.h"
#include "../Engine/Window/WindowsUtils.h"
#include "../ImGuiManager/ImGuiManager.h"
#include "../Lib/Console/ConVar.h"
#include "../Lib/Console/ConVars.h"
#include "../Lib/Math/MathLib.h"
#include "../Object3D/Object3D.h"
#include "../Particle/ParticleCommon.h"
#include "../Sprite/SpriteCommon.h"
#include "../TextureManager/TextureManager.h"
#include "nlohmann/json.hpp"

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif
#include "../Engine/Lib/Console/Console.h"


void SaveFile(const std::string& groupName) {
	using json = nlohmann::json;

	json root;
	root = json::object();

	// jsonオブジェクト登録
	root[groupName] = json::object();
}

const std::string kExtension = ".scene";

// 子オブジェクトを処理するための再帰関数
void LoadChildObject(LevelData* levelData, const nlohmann::json& child) {
	assert(child.contains("type"));

	// 子オブジェクトの種類を取得
	std::string type = child["type"].get<std::string>();

	// MESH
	if (type == "MESH") {
		// 要素追加
		levelData->objects.emplace_back(LevelData::ObjectData{});
		// 今追加した要素の参照を得る
		LevelData::ObjectData& objectData = levelData->objects.back();

		if (child.contains("file_name")) {
			// ファイル名
			objectData.fileName = child["file_name"];
		}

		// トランスフォームのパラメータ読み込み
		if (child.contains("transform")) {
			auto& transform = child["transform"];
			objectData.position = {
				transform["translation"][0], transform["translation"][2], transform["translation"][1]
			};
			objectData.rotation = { transform["rotation"][0], transform["rotation"][1], transform["rotation"][2] };
			objectData.rotation *= -1.0f * Math::deg2Rad;
			objectData.scale = { transform["scaling"][0], transform["scaling"][2], transform["scaling"][1] };
		}

		// コライダーのパラメータ読み込み
		if (child.contains("collider")) {
			auto& collider = child["collider"];
			objectData.colliderType = collider["type"].get<std::string>();
			objectData.colliderCenter = { collider["center"][0], collider["center"][2], collider["center"][1] };
			objectData.colliderSize = { collider["size"][0], collider["size"][2], collider["size"][1] };
		}
	}

	// 子オブジェクトがある場合、再帰的に処理
	if (child.contains("children")) {
		for (const auto& grandChild : child["children"]) {
			LoadChildObject(levelData, grandChild);
		}
	}
}

std::vector<Transform> matoTransforms;

void LoadFile(const std::string& relativePath) {
	// 連結してフルパスを得る
	const std::string fullPath = std::filesystem::current_path().string() + relativePath + kExtension;

	// ファイルストリーム
	std::ifstream file;

	// ファイルを開く
	file.open(fullPath);
	// ファイルオープン失敗をチェック
	if (file.fail()) {
		assert(0);
	}

	// JSON文字列から解凍したデータ
	nlohmann::json deserialized;

	// 解凍
	file >> deserialized;

	// 正しいレベルデータファイル化チェック
	assert(deserialized.is_object());
	assert(deserialized.contains("name"));
	assert(deserialized["name"].is_string());

	// "name"を文字列として取得
	std::string name = deserialized["name"].get<std::string>();
	// 正しいレベルデータファイルかチェック
	assert(name == "scene");

	// レベルデータ格納用インスタンスを生成
	LevelData* levelData = new LevelData();

	// "objects"の全オブジェクトを走査
	for (nlohmann::json& object : deserialized["objects"]) {
		assert(object.contains("type"));

		// 種別を取得
		std::string type = object["type"].get<std::string>();

		// MESHの場合、LoadChildObjectを呼び出す
		if (type.compare("MESH") == 0) {
			LoadChildObject(levelData, object);
			LevelData::ObjectData& data = levelData->objects.back();
			if (data.fileName == "mato") {
				// レールのモデルを生成
				Transform mato = {
					.scale = data.scale,
					.rotate = data.rotation,
					.translate = data.position
				};
				matoTransforms.push_back(mato);
			}
		}
		// CURVEの場合、制御点を配列に格納
		else if (type.compare("CURVE") == 0) {
			if (object.contains("control_points")) {
				for (const auto& point : object["control_points"]) {
					// 制御点を配列に追加
					controlPoints.push_back({
						point["x"].get<float>(),
						point["z"].get<float>(),
						point["y"].get<float>()
						});
				}
			}
		}
	}
}

Vec4 MultiplyMat4Vec4(const Mat4& mat, const Vec4& vec) {
	Vec4 result;
	result.x = mat.m[0][0] * vec.x + mat.m[0][1] * vec.y + mat.m[0][2] * vec.z + mat.m[0][3] * vec.w;
	result.y = mat.m[1][0] * vec.x + mat.m[1][1] * vec.y + mat.m[1][2] * vec.z + mat.m[1][3] * vec.w;
	result.z = mat.m[2][0] * vec.x + mat.m[2][1] * vec.y + mat.m[2][2] * vec.z + mat.m[2][3] * vec.w;
	result.w = mat.m[3][0] * vec.x + mat.m[3][1] * vec.y + mat.m[3][2] * vec.z + mat.m[3][3] * vec.w;
	return result;
}

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
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/world.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/reticle.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/mato.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/axis.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/white.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/gaugeFrame.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/gaugeBar.png");
#pragma endregion

#pragma region スプライト類
	reticleSprite_ = std::make_unique<Sprite>();
	reticleSprite_->Init(spriteCommon_, "./Resources/Textures/reticle.png");
	reticleSprite_->SetSize({ 64.0f, 64.0f, 0.0f });
	reticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	gaugeFrame_ = std::make_unique<Sprite>();
	gaugeFrame_->Init(spriteCommon_, "./Resources/Textures/gaugeFrame.png");
	gaugeFrame_->SetAnchorPoint({ 0.0f,1.0f });
	gaugeFrame_->SetPos({ 64.0f, kClientHeight - 64.0f });

	gaugeBar_ = std::make_unique<Sprite>();
	gaugeBar_->Init(spriteCommon_, "./Resources/Textures/gaugeBar.png");
	gaugeBar_->SetAnchorPoint({ 0.0f,1.0f });
	gaugeBar_->SetPos(gaugeFrame_->GetPos() + Vec2(4.0f, -4.0f));
	gaugeBar_->SetCropBottomRight({ 24.0f,248.0f });
#pragma endregion

#pragma region 3Dオブジェクト類
	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("cart.obj");
	object3D_ = std::make_unique<Object3D>();
	object3D_->Init(object3DCommon_, modelCommon_);
	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object3D_->SetModel("cart.obj");
	object3D_->SetPos({ 0.0f, 0.0f, 0.0f });

	ModelManager::GetInstance()->LoadModel("axis.obj");
	axis_ = std::make_unique<Object3D>();
	axis_->Init(object3DCommon_, modelCommon_);
	axis_->SetModel("axis.obj");

	ModelManager::GetInstance()->LoadModel("line.obj");
	line_ = std::make_unique<Object3D>();
	line_->Init(object3DCommon_, modelCommon_);
	line_->SetModel("line.obj");
	line_->SetLighting(false);
#pragma endregion

#pragma region パーティクル類
	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleCommon_, "./Resources/Textures/circle.png");
#pragma endregion

	ModelManager::GetInstance()->LoadModel("sky.obj");
	sky_ = std::make_unique<Object3D>();
	sky_->Init(object3DCommon_, modelCommon_);
	sky_->SetModel("sky.obj");
	sky_->SetPos({ 0.0f, 0.0f, 0.0f });
	sky_->SetLighting(false);

	player_ = std::make_unique<Player>();
	player_->Init(object3DCommon_, modelCommon_);
	player_->SetModel("axis.obj");
	player_->SetPos({ 0.0f, 0.0f, 0.0f });
	player_->Initialize();

	ModelManager::GetInstance()->LoadModel("world.obj");
	world_ = std::make_unique<Object3D>();
	world_->Init(object3DCommon, modelCommon_);
	world_->SetModel("world.obj");
	world_->SetLighting(false);


	ModelManager::GetInstance()->LoadModel("rail.obj");

	LoadFile("./Resources/test");

	ModelManager::GetInstance()->LoadModel("mato.obj");

	for (auto& transform : matoTransforms) {
		auto mato = std::make_unique<Object3D>();
		mato->Init(object3DCommon_, modelCommon_);
		mato->SetModel("mato.obj");
		mato->SetPos(transform.translate);
		mato->SetRot(transform.rotate);
		mato->SetScale(transform.scale);
		mato->SetLighting(false);
		mato_.push_back(std::move(mato));
	}

	ModelManager::GetInstance()->LoadModel("poll.obj");
	for (auto& controlPoint : controlPoints) {
		// レールのモデルを生成
		auto poll = std::make_unique<Object3D>();
		poll->Init(object3DCommon_, modelCommon_);
		poll->SetModel("poll.obj");
		poll->SetPos(controlPoint);
		poll_.push_back(std::move(poll));
	}

	PlaceRailsAlongSpline();

	railMover_ = std::make_unique<RailMover>();
	railMover_->Initialize(object3D_.get(), controlPoints, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

	reticlePos_ = { kClientWidth * 0.5f, kClientHeight * 0.5f, 0.0f };
}

bool IntersectRaySphere(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& sphereCenter, float sphereRadius) {
	Vec3 oc = rayOrigin - sphereCenter;
	float b = 2.0f * oc.Dot(rayDir);
	float c = oc.Dot(oc) - sphereRadius * sphereRadius;
	float discriminant = b * b - 4 * c;
	return (discriminant > 0); // 判定結果
}

Vec3 ScreenToWorldAtDepth(const Vec3 screenPos, Camera* camera, const float kDistanceFromCamera) {
	// スクリーン座標からNDC座標に変換
	Vec4 ndcPos;
	ndcPos.x = ((2.0f * screenPos.x) / kClientWidth) - 1.0f; // X軸を反転
	ndcPos.y = 1.0f - ((2.0f * screenPos.y) / kClientHeight); // Y軸を反転
	ndcPos.z = 1.0f; // ファークリップ面を使用
	ndcPos.w = 1.0f;

	// NDC空間からビュー空間へ変換
	Mat4 invProj = camera->GetProjMat().Inverse();
	Vec4 viewPos = MultiplyMat4Vec4(invProj, ndcPos);
	viewPos.w = 1.0f;

	// 同次座標を3Dビュー座標に変換
	Vec3 viewRayDir = Vec3(viewPos.x / viewPos.w, viewPos.y / viewPos.w, 1.0f).Normalized();

	// ビュー空間からワールド空間へ変換
	Vec4 worldRayDir = MultiplyMat4Vec4(camera->GetViewMat(), Vec4(viewRayDir.x, viewRayDir.y, viewRayDir.z, 0.0f));

	// ワールドレイ方向を正規化
	Vec3 finalRayDir = Vec3(worldRayDir.x, worldRayDir.y, worldRayDir.z).Normalized();

	// 指定距離の位置を計算
	Vec3 cameraPos = camera->GetPos();
	return (cameraPos + finalRayDir * kDistanceFromCamera);
}

void GameScene::Update(const float& deltaTime) {
	// 発射しているかどうかの判定
	isShooting = Input::GetInstance()->PushKey(DIK_SPACE) || Input::GetInstance()->PushKey(DIK_RETURN);

	// 発射していない場合、熱量が回復する
	if (!isShooting && !isOverheated) {
		heat = min(kMaxHeat, heat + deltaTime * 10.0f); // 最大熱量を超えないように
	}

	// オーバーヒート中の処理
	if (isOverheated) {
		// 一定時間経過したらオーバーヒートを解除
		if (currentTime_ >= overheatStartTime + kOverheatPenaltyTime) {
			isOverheated = false;
		}
	}

	// 発射中の処理
	if (isShooting && !isOverheated) {
		if (!isOverheated && heat > kMinHeat) {
			lastShootTime = currentTime_; // 最後に発射した時間を更新
			heat -= 0.125f; // 発射する度に熱量を減少

			// 熱量が0になったらオーバーヒート
			if (heat <= kMinHeat) {
				isOverheated = true;
				overheatStartTime = currentTime_; // オーバーヒート開始時間を記録
				isShooting = false;
			}
		} else {
			isShooting = false;
		}
	} else {
		isShooting = false;
	}

	// 現在の時間を進める
	currentTime_ += deltaTime;

	reticleMoveInput_ = Vec3::zero;
	if (Input::GetInstance()->PushKey(DIK_W) && !Input::GetInstance()->PushKey(DIK_S)) {
		reticleMoveInput_.y--;
	} else if (!Input::GetInstance()->PushKey(DIK_W) && Input::GetInstance()->PushKey(DIK_S)) {
		reticleMoveInput_.y++;
	}

	if (Input::GetInstance()->PushKey(DIK_D) && !Input::GetInstance()->PushKey(DIK_A)) {
		reticleMoveInput_.x++;
	} else if (!Input::GetInstance()->PushKey(DIK_D) && Input::GetInstance()->PushKey(DIK_A)) {
		reticleMoveInput_.x--;
	}
	reticleMoveInput_.Normalize();

	// 摩擦係数と加速係数
	const float acceleration = 10.0f; // 加速係数 (0～1で設定)

	// 加速処理
	float currentSpeed = reticleMoveVel_.Dot(reticleMoveInput_); // 現在の速度成分
	float addSpeed = reticleMoveSpd_ - currentSpeed; // 加速する必要のある速度

	if (addSpeed > 0.0f) {
		float accelSpeed = acceleration * deltaTime * reticleMoveSpd_; // 加速係数(0.1fは調整可能)
		if (accelSpeed > addSpeed) {
			accelSpeed = addSpeed; // 必要以上の加速を抑制
		}

		// 加速を適用
		reticleMoveVel_ += reticleMoveInput_ * accelSpeed;
	}

	// 摩擦��用
	float speed = reticleMoveVel_.Length();
	if (speed > 0.001f) {
		float friction = 8.0f; // 摩擦係数
		float control = (speed < 1.0f) ? 1.0f : speed; // 最小速度制御値 (任意で調整可能)
		float drop = control * friction * deltaTime;

		// 新しい速度を計算
		float newSpeed = speed - drop;
		newSpeed = max(newSpeed, 0.0f);

		// ベクトル全体にスケール適用
		reticleMoveVel_ *= newSpeed / speed;
	}

	// 位置を更新
	reticlePos_ += reticleMoveVel_ * deltaTime;

	// 画面端に到達したらその軸の速度を0に
	for (int i = 0; i < 2; ++i) {
		if (Math::Clamp(reticlePos_[i], 0, static_cast<float>(i == 0 ? kClientWidth : kClientHeight))) {
			reticleMoveVel_[i] = 0.0f;
		}
	}

	// スプライトの位置を設定
	reticleSprite_->SetPos(reticlePos_);

	Vec3 cameraPos;
	{
		// object3D_の最新位置にカメラを追従させる
		object3DCommon_->GetDefaultCamera()->SetPos(
			object3D_->GetPos() + Vec3(std::sin(object3D_->GetRot().x), 1.0f, std::cos(object3D_->GetRot().x)));
		object3DCommon_->GetDefaultCamera()->SetRot(object3D_->GetRot());

		// オブジェクトの回転角度を取得
		Vec3 rotation = object3D_->GetRot();

		// オフセットのベクトル
		Vec3 offset(0.0f, 1.0f, 0.0f);

		// 回転行列を作成 (ここではX軸回転のみを考慮する例)
		Mat4 rotationMatrix = Mat4::RotateY(rotation.y);

		// オフセットを回転行列で変換して、カメラ位置を計算
		Vec3 rotatedOffset = Mat4::Transform(offset, rotationMatrix);
		cameraPos = object3D_->GetPos() + rotatedOffset;

		// カメラの位置を更新
		object3DCommon_->GetDefaultCamera()->SetPos(cameraPos);
	}

	{
		Vec3 offset(0.0f, 0.0f, 1.0f);
		Mat4 rotationMatrixX = Mat4::RotateX(object3D_->GetRot().x);
		Mat4 rotationMatrixY = Mat4::RotateY(object3D_->GetRot().y);
		Mat4 rotMat = rotationMatrixX * rotationMatrixY;
		Vec3 rotatedOffset = Mat4::Transform(offset, rotMat);
		line_->SetPos(object3D_->GetPos() + rotatedOffset);
	}

	Vec3 direction = (axis_->GetPos() - line_->GetPos()).Normalized();
	Vec3 rot;
	// ヨー
	rot.y = atan2(direction.x, direction.z);

	// ピッチ
	rot.x = -atan2(direction.y, sqrt(direction.x * direction.x + direction.z * direction.z));

	// ロールはさせない
	rot.z = 0.0f;
	line_->SetScale({ 4.0f,0.1f,(axis_->GetPos() - line_->GetPos()).Length() });
	line_->SetRot(rot);

	{
		const float kDistanceFromCamera = 40.0f;

		// // カメラ情報
		Camera* camera = object3DCommon_->GetDefaultCamera();

		// スクリーン座標をワールド座標に変換
		Vec3 screenPos(reticlePos_.x, reticlePos_.y, 0.0f);
		Vec3 worldPos = ScreenToWorldAtDepth(screenPos, camera, kDistanceFromCamera);

		// 照準オブジェクトを配置
		axis_->SetPos(worldPos);
		axis_->Update();
	}

	// レール移動の更新
	railMover_->Update(deltaTime);

	// object3D_を更新
	object3D_->Update();

	world_->Update();

	reticleSprite_->Update();

	line_->Update();

	if (isShooting) {
		for (const std::unique_ptr<Object3D>& mato : mato_) {
			// 衝突判定処理（線分と球の当たり判定）
			if (IsLineSphereColliding(cameraPos, axis_->GetPos(), mato->GetPos(), 1.0f)) {
				mato->SetVisible(false);
			}
		}
	}

	//// 残りのオブジェクトを更新
	//reticleSprite_->Update();
	//particle_->Update(deltaTime);
	sky_->Update();
	//player_->Update();

	for (const std::unique_ptr<Object3D>& rail : rails_) {
		rail->Update();
	}

	for (const std::unique_ptr<Object3D>& mato : mato_) {
		if (mato->IsVisible()) {
			mato->SetRot(object3DCommon_->GetDefaultCamera()->GetRot());
			mato->Update();
		}
	}

	for (const std::unique_ptr<Object3D>& poll : poll_) {
		poll->Update();
	}


#ifdef _DEBUG 
	ImGui::Begin("Test");
	Vec2 leftTop = gaugeBar_->GetTextureLeftTop();
	if (ImGui::DragFloat2("lefttop", &leftTop.x, 1.0f)) {
		gaugeBar_->SetTextureLeftTop(leftTop);
	}
	// クロップ用の変数を追加
	Vec2 cropTopLeft = gaugeBar_->GetCropTopLeft();
	Vec2 cropBottomRight = gaugeBar_->GetCropBottomRight();
	if (ImGui::DragFloat2("Crop Top Left", &cropTopLeft.x, 1.0f)) {
		gaugeBar_->SetCropTopLeft(cropTopLeft);
	}
	if (ImGui::DragFloat2("Crop Bottom Right", &cropBottomRight.x, 1.0f)) {
		gaugeBar_->SetCropBottomRight(cropBottomRight);
	}

	Vec3 size = gaugeBar_->GetSize();
	if (ImGui::DragFloat2("scale", &size.x, 1.0f)) {
		gaugeBar_->SetSize(size);
	}
	ImGui::End();
#endif

	float gaugeRatio = heat / kMaxHeat;
	gaugeBar_->SetSize({ gaugeBar_->GetSize().x, gaugeBar_->GetTextureSize().y * gaugeRatio });

	gaugeFrame_->Update();
	gaugeBar_->Update();

#ifdef _DEBUG
#pragma region cl_showpos
	if (ConVars::GetInstance().GetConVar("cl_showpos")->GetInt() == 1) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
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
			object3DCommon_->GetDefaultCamera()->GetPos().x, object3DCommon_->GetDefaultCamera()->GetPos().y,
			object3DCommon_->GetDefaultCamera()->GetPos().z,
			object3DCommon_->GetDefaultCamera()->GetRot().x * Math::rad2Deg,
			object3DCommon_->GetDefaultCamera()->GetRot().y * Math::rad2Deg,
			object3DCommon_->GetDefaultCamera()->GetRot().z * Math::rad2Deg,
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

	sky_->Draw();

	object3D_->Draw();

	//axis_->Draw();

	world_->Draw();

	if (isShooting) {
		line_->Draw();
	}

	// player_->Draw();

	for (const std::unique_ptr<Object3D>& rail : rails_) {
		if (object3DCommon_->GetDefaultCamera()->GetPos().Distance(rail->GetPos()) < 15.0f) {
			rail->Draw();
		}
	}

	for (const std::unique_ptr<Object3D>& mato : mato_) {
		if (object3DCommon_->GetDefaultCamera()->GetPos().Distance(mato->GetPos()) < 40.0f) {
			mato->Draw();
		}
	}

	for (const std::unique_ptr<Object3D>& poll : poll_) {
		poll->Draw();
	}

	//----------------------------------------
	// パーティクル共通描画設定
	particleCommon_->Render();
	//----------------------------------------
	//particle_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
	reticleSprite_->Draw();
	gaugeFrame_->Draw();
	gaugeBar_->Draw();
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleCommon_->Shutdown();
}

void GameScene::PlaceRailsAlongSpline() {
	const float railLength = 1.0f; // レール1つの長さ（メートル単位）
	const float delta = 0.0001f;

	// ループのために末尾に最初の制御点を追加する
	controlPoints.push_back(controlPoints[0]);

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

bool GameScene::IsLineSphereColliding(const Vec3& lineStart, const Vec3& lineEnd, const Vec3& sphereCenter,
	float sphereRadius) {
	// 線分の方向ベクトル
	Vec3 lineDirection = lineEnd - lineStart;
	float lineLengthSq = lineDirection.SqrLength();

	// 線分の始点から球体中心へのベクトル
	Vec3 startToSphere = sphereCenter - lineStart;

	// 射影長を計算
	float t = startToSphere.Dot(lineDirection) / lineLengthSq;

	// 最近接点を計算
	Vec3 closestPoint;
	if (t < 0.0f) {
		// 線分の始点が最近接点
		closestPoint = lineStart;
	} else if (t > 1.0f) {
		// 線分の終点が最近接点
		closestPoint = lineEnd;
	} else {
		// 線分上の点が最近接点
		closestPoint = lineStart + lineDirection * t;
	}

	// 最近接点と球体中心との距離を計算
	Vec3 distanceVec = closestPoint - sphereCenter;
	float distanceSq = distanceVec.SqrLength();

	// 距離の2乗が半径の2乗以下なら衝突している
	return distanceSq <= (sphereRadius * sphereRadius);
}
