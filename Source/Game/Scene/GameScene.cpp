#include "GameScene.h"

#include <Camera/CameraManager.h>
#include <Components/CameraRotator.h>
#include <Components/PlayerMovement.h>
#include <Debug/Debug.h>
#include <Engine.h>
#include <ImGuiManager/ImGuiManager.h>
#include <Lib/Console/ConVarManager.h>
#include <Lib/Math/MathLib.h>
#include <Lib/Math/Random/Random.h>
#include <Lib/Timer/EngineTimer.h>
#include <Model/ModelManager.h>
#include <Object3D/Object3D.h>
#include <Particle/ParticleManager.h>
#include <Sprite/SpriteCommon.h>
#include <TextureManager/TextureManager.h>
#include <Components/EnemyMovement.h>

#include "Input/InputSystem.h"

constexpr int enemyCount = 10;

void GameScene::Init(Engine* engine) {
	renderer_ = engine->GetRenderer();
	window_ = engine->GetWindow();
	spriteCommon_ = engine->GetSpriteCommon();
	particleManager_ = engine->GetParticleManager();
	object3DCommon_ = engine->GetObject3DCommon();
	modelCommon_ = engine->GetModelCommon();
	srvManager_ = engine->GetSrvManager();
	timer_ = engine->GetEngineTimer();

#pragma region テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/circle.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/weaponNinjaSword.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/fakeShadow.png");
#pragma endregion

#pragma region スプライト類
	sprite_ = std::make_unique<Sprite>();
	sprite_->Init(spriteCommon_, "./Resources/Textures/uvChecker.png");
	sprite_->SetSize({ 512.0f, 512.0f, 0.0f });
#pragma endregion

#pragma region 3Dオブジェクト類
	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("ground.obj");
	ModelManager::GetInstance()->LoadModel("weaponNinjaSword.obj");
	ModelManager::GetInstance()->LoadModel("fakeShadow.obj");
	ModelManager::GetInstance()->LoadModel("player.obj");

	ground_ = std::make_unique<Object3D>();
	ground_->Init(object3DCommon_);
	// 初期化済みの3Dオブジェクトにモデルを紐づける
	ground_->SetModel("ground.obj");
	ground_->SetPos(Vec3::zero);

	sword_ = std::make_unique<Object3D>();
	sword_->Init(object3DCommon_);
	sword_->SetModel("weaponNinjaSword.obj");
	sword_->SetPos(Vec3::zero);

	shadow_ = std::make_unique<Object3D>();
	shadow_->Init(object3DCommon_);
	shadow_->SetModel("fakeShadow.obj");
	shadow_->SetPos(Vec3::zero);

	playerObject_ = std::make_unique<Object3D>();
	playerObject_->Init(object3DCommon_);
	playerObject_->SetModel("player.obj");
	playerObject_->SetPos(Vec3::zero);

	for (int i = 0; i < enemyCount; ++i) {
		std::unique_ptr<Object3D> enemyObject;
		enemyObject = std::make_unique<Object3D>();
		enemyObject->Init(object3DCommon_);
		enemyObject->SetModel("player.obj");
		enemyObject->SetPos(Vec3::zero);
		enemyObjects_.push_back(std::move(enemyObject));
	}
#pragma endregion

#pragma region パーティクル類
	particleManager_->CreateParticleGroup("circle", "./Resources/Textures/circle.png");

	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleManager_, "./Resources/Textures/circle.png");
#pragma endregion

#pragma region エンティティ
	camera_ = std::make_unique<Entity>("camera");
	// 生ポインタを取得
	CameraComponent* rawCameraPtr = camera_->AddComponent<CameraComponent>();

	// 生ポインタを std::shared_ptr に変換
	const std::shared_ptr<CameraComponent> camera = std::shared_ptr<CameraComponent>(rawCameraPtr, [](CameraComponent*) {});

	// カメラを CameraManager に追加
	CameraManager::AddCamera(camera);
	// アクティブカメラに設定
	CameraManager::SetActiveCamera(camera);

	testEnt_ = std::make_unique<Entity>("testEnt");
	testEntColliderComponent_ = testEnt_->AddComponent<ColliderComponent>();
	testEnt_->GetTransform()->SetLocalPos(Vec3::zero);

	testEnt2_ = std::make_unique<Entity>("testEnt2");
	testEnt2_->GetTransform()->SetLocalPos(Vec3::right * 2.0f);
	testEnt2_->SetParent(testEnt_.get());

	testEnt3_ = std::make_unique<Entity>("testEnt3");
	testEnt3_->GetTransform()->SetLocalPos(Vec3::right * 2.0f);
	testEnt3_->SetParent(testEnt2_.get());

	player_ = std::make_unique<Entity>("player");
	playerColliderComponent_ = player_->AddComponent<ColliderComponent>();
	player_->GetTransform()->SetLocalPos(Vec3::zero);
	playerMovementComponent_ = player_->AddComponent<PlayerMovement>();

	// 敵の生成
	for (int i = 0; i < enemyCount; ++i) {
		std::unique_ptr<Entity> enemy = std::make_unique<Entity>("enemy" + std::to_string(i));
		enemy->GetTransform()->SetLocalPos(Random::Vec3Range({ -32.0f, 0.0f, -32.0f }, { 32.0f , 32.0f, 32.0f }));
		enemyMovementComponents_.push_back(enemy->AddComponent<EnemyMovement>());
		enemyColliderComponents_.push_back(enemy->AddComponent<ColliderComponent>());
		enemyPreviousRotations_.push_back(Quaternion::identity);
		enemyTargetRotations_.push_back(Quaternion::identity);
		enemy->AddComponent<ColliderComponent>();
		enemies_.push_back(std::move(enemy));
	}

	// コリジョンコールバックの登録
	playerColliderComponent_->RegisterCollisionCallback([this](Entity* other) {
		Console::Print("Player collided with: " + other->GetName());
		}
	);

	testEntColliderComponent_->RegisterCollisionCallback([this](Entity* other) {
		Console::Print("testEnt collided with: " + other->GetName());
		}
	);

	// 剣のエンティティを作成
	swordEntity_ = std::make_unique<Entity>("sword");
	swordEntity_->GetTransform()->SetLocalPos(Vec3::zero);

	cameraRoot_ = std::make_unique<Entity>("cameraBase");
	cameraRotator_ = cameraRoot_->AddComponent<CameraRotator>();
	cameraRoot_->SetParent(player_.get());
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);

	// プレイヤーにカメラをアタッチ
	camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::forward * -2.5f);

#pragma endregion
}

void GameScene::RotateMesh(Quaternion& prev, Quaternion target, const float deltaTime, Vec3 velocity) {
	velocity.y = 0.0f;
	// プレイヤーの速度に基づいて傾きを計算
	if (velocity.Length() > 0.01f) { // 速度がほぼゼロでない場合
		Vec3 direction = velocity.Normalized();
		float tiltAngle = std::atan2(direction.x, direction.z);
		Quaternion tiltRotation = Quaternion::Euler(Vec3(0.0f, tiltAngle, 0.0f));

		// 進行方向に基づいて傾きを計算
		float tiltAmount = min(velocity.Length() * 0.1f, 30.0f * Math::deg2Rad); // 最大30度まで傾ける
		Quaternion forwardTilt = Quaternion::Euler(Vec3(tiltAmount, 0.0f, 0.0f));

		target = tiltRotation * forwardTilt; // 目標の回転を設定

		// 現在の回転と目標の回転を補間
		prev = Quaternion::Slerp(prev, target, deltaTime * 20.0f);
	}

	// NaNチェック
	if (std::isnan(prev.x) || std::isnan(prev.y) || std::isnan(prev.z) || std::isnan(prev.w)) {
		prev = Quaternion::identity; // NaNが含まれている場合は初期値に戻す
	}
}

void GameScene::Update(const float deltaTime) {
	sprite_->Update();
	ground_->Update();

	particleManager_->Update(EngineTimer::GetScaledDeltaTime());
	particle_->Update(EngineTimer::GetScaledDeltaTime());

	testEnt_->GetTransform()->SetWorldRot(testEnt_->GetTransform()->GetWorldRot() * Quaternion::Euler(Vec3::up * 1.0f * deltaTime));
	testEnt2_->GetTransform()->SetLocalRot(testEnt2_->GetTransform()->GetLocalRot() * Quaternion::Euler(Vec3::up * 1.0f * deltaTime));
	testEnt3_->GetTransform()->SetLocalRot(testEnt3_->GetTransform()->GetLocalRot() * Quaternion::Euler(Vec3::up * 1.0f * deltaTime));
	testEnt_->Update(deltaTime);

	player_->Update(deltaTime);

	// 敵の更新
	for (int i = 0; i < enemyCount; ++i) {
		if (enemies_[i]->GetTransform()->GetWorldPos().Distance(player_->GetTransform()->GetWorldPos()) >= 2.0f) {
			enemyMovementComponents_[i]->SetMoveInput(player_->GetTransform()->GetWorldPos() - enemies_[i]->GetTransform()->GetWorldPos());
		} else {
			enemyMovementComponents_[i]->SetMoveInput(Vec3::zero);
		}
		enemies_[i]->Update(deltaTime);
		Vec3 enemyVel = enemyMovementComponents_[i]->GetVelocity();
		RotateMesh(enemyPreviousRotations_[i], enemyTargetRotations_[i], deltaTime, enemyVel);
		enemyObjects_[i]->SetRot(enemyPreviousRotations_[i].ToEulerAngles());
		enemyObjects_[i]->SetPos(enemies_[i]->GetTransform()->GetWorldPos() + Vec3::up * 1.0f);
		enemyObjects_[i]->Update();
	}

	Vec3 velocity = playerMovementComponent_->GetVelocity() * 0.254f;
	RotateMesh(previousRotation_, targetRotation_, deltaTime, velocity);

	playerObject_->SetRot(previousRotation_.ToEulerAngles());
	playerObject_->SetPos(player_->GetTransform()->GetWorldPos() + Vec3::up * 1.0f);
	playerObject_->Update();


	BehaviorInit();

	BehaviorUpdate(deltaTime);

	shadow_->SetPos({ player_->GetTransform()->GetWorldPos().x,0.0f, player_->GetTransform()->GetWorldPos().z });
	shadow_->Update();

	 // 衝突判定
	if (CheckSphereCollision(*playerColliderComponent_, *testEntColliderComponent_)) {
		playerColliderComponent_->OnCollision(testEnt_.get());
		testEntColliderComponent_->OnCollision(player_.get());
	}

#ifdef _DEBUG
#pragma region cl_showpos
	if (ConVarManager::GetConVar("cl_showpos")->GetValueAsString() == "1") {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking;
		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);

		Vec3 camPos = CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetTranslate();
		Vec3 camRot = CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetRotate();

		// テキストのサイズを取得
		ImGuiIO io = ImGui::GetIO();
		std::string text = std::format(
			"name: {}\n"
			"pos : {:.2f} {:.2f} {:.2f}\n"
			"rot : {:.2f} {:.2f} {:.2f}\n"
			"vel : {:.2f}\n",
			ConVarManager::GetConVar("name")->GetValueAsString(),
			camPos.x, camPos.y, camPos.z,
			camRot.x * Math::rad2Deg,
			camRot.y * Math::rad2Deg,
			camRot.z * Math::rad2Deg,
			Math::MtoH(playerMovementComponent_->GetVelocity().Length())
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
		ImGuiManager::TextOutlined(drawList, textPos, text.c_str(), textColor, outlineColor, outlineSize);

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
	ground_->Draw();
	sword_->Draw();
	shadow_->Draw();
	playerObject_->Draw();
	// 敵の描画
	for (auto& enemy : enemyObjects_) {
		enemy->Draw();
	}

	//----------------------------------------
	// パーティクル共通描画設定
	particleManager_->Render();
	//----------------------------------------
	particle_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
	// sprite_->Draw();
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleManager_->Shutdown();
}

bool GameScene::CheckSphereCollision(const ColliderComponent& collider1, const ColliderComponent& collider2) {
	Vec3 pos1 = collider1.GetPosition();
	Vec3 pos2 = collider2.GetPosition();

	float distance = (pos1 - pos2).Length();
	return distance <= (collider1.GetRadius() + collider2.GetRadius());
}

void GameScene::RequestBehavior(SwordBehavior request) {
	behaviorRequest_ = request;
}

//-----------------------------------------------------------------------------
// Purpose: アイドル
//-----------------------------------------------------------------------------
void GameScene::IdleInit() {

}

void GameScene::IdleUpdate([[maybe_unused]] float deltaTime) {
	// プレイヤーの背中に剣を配置
	static Vec3 swordOffset = Vec3(0.2f, 0.6f, -0.3f); // 背中の位置をオフセットで指定
	Vec3 swordPosition = playerObject_->GetPos() + previousRotation_ * swordOffset;
	TransformComponent* swordTransform = swordEntity_->GetTransform();

	swordTransform->SetWorldPos(Math::Lerp(swordTransform->GetWorldPos(), swordPosition, deltaTime * 20.0f));
	swordTransform->SetLocalRot(Quaternion::Slerp(swordTransform->GetLocalRot(), previousRotation_ * Quaternion::Euler(Vec3::forward * (160.0f * Math::deg2Rad)), deltaTime * 10.0f));

	sword_->SetPos(swordTransform->GetWorldPos());
	sword_->SetRot(swordTransform->GetWorldRot().ToEulerAngles());
	sword_->Update();

	if (InputSystem::IsPressed("attack1")) {
		RequestBehavior(SwordBehavior::Attack1);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 攻撃1
//-----------------------------------------------------------------------------
void GameScene::Attack1Init() {
}

void GameScene::Attack1Update([[maybe_unused]] float deltaTime) {
	// プレイヤーの手らへんに剣を配置
	static Vec3 swordOffset = Vec3(0.2f, 0.6f, -0.3f); // 背中の位置をオフセットで指定
	ImGuiManager::DragVec3("swordOffset", swordOffset, 0.01f, "%.2f");
	Vec3 swordPosition = playerObject_->GetPos() + previousRotation_ * swordOffset;
	TransformComponent* swordTransform = swordEntity_->GetTransform();

	swordTransform->SetWorldPos(Math::Lerp(swordTransform->GetWorldPos(), swordPosition, deltaTime * 20.0f));

	static Vec3 swordRotation = Vec3(160.0f, 0.0f, 0.0f);
	ImGuiManager::DragVec3("swordRotation", swordRotation, 0.01f, "%.2f");
	swordTransform->SetLocalRot(
		Quaternion::Slerp(
			swordTransform->GetLocalRot(),
			previousRotation_ * Quaternion::Euler(swordRotation * Math::deg2Rad),
			deltaTime * 10.0f)
	);

	sword_->SetPos(swordTransform->GetWorldPos());
	sword_->SetRot(swordTransform->GetWorldRot().ToEulerAngles());
	sword_->Update();
}

//-----------------------------------------------------------------------------
// Purpose: 攻撃2
//-----------------------------------------------------------------------------
void GameScene::Attack2Init() {
}

void GameScene::Attack2Update([[maybe_unused]] float deltaTime) {
}

//-----------------------------------------------------------------------------
// Purpose: 攻撃3
//-----------------------------------------------------------------------------
void GameScene::Attack3Init() {
}

void GameScene::Attack3Update([[maybe_unused]] float deltaTime) {
}

void GameScene::BehaviorInit() {
	if (behaviorRequest_) {
		behavior_ = behaviorRequest_.value();
		switch (behavior_) {
		case SwordBehavior::Idle:
			IdleInit();
			break;
		case SwordBehavior::Attack1:
			Attack1Init();
			break;
		case SwordBehavior::Attack2:
			Attack2Init();
			break;
		case SwordBehavior::Attack3:
			Attack3Init();
			break;
		}
		behaviorRequest_ = std::nullopt;
	}
}

void GameScene::BehaviorUpdate(float deltaTime) {
	switch (behavior_) {
	case SwordBehavior::Idle:
		IdleUpdate(deltaTime);
		break;
	case SwordBehavior::Attack1:
		Attack1Update(deltaTime);
		break;
	case SwordBehavior::Attack2:
		Attack2Update(deltaTime);
		break;
	case SwordBehavior::Attack3:
		Attack3Update(deltaTime);
		break;
	}
}
