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

constexpr int enemyCount = 24;

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
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/hud.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/num.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/combo.png");
#pragma endregion

#pragma region スプライト類
	hudSprite_ = std::make_unique<Sprite>();
	hudSprite_->Init(spriteCommon_, "./Resources/Textures/hud.png");
	hudSprite_->SetSize({ 1280.0f, 720.0f, 0.0f });

	comboSprite_ = std::make_unique<Sprite>();
	comboSprite_->Init(spriteCommon_, "./Resources/Textures/combo.png");
	comboSprite_->SetSize({ 154.0f, 47.0f, 0.0f });

	for (int i = 0; i < 5; ++i) {
		Number number;
		number.sprite = std::make_unique<Sprite>();
		number.sprite->Init(spriteCommon_, "./Resources/Textures/num.png");
		number.sprite->SetSize({ 64.0f, 128.0f, 0.0f });
		digits_.push_back(std::move(number));
	}
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
	particleManager_->CreateParticleGroup("circle", "./Resources/Textures/doublestar.png");

	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleManager_, "./Resources/Textures/doublestar.png");
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

	player_ = std::make_unique<Entity>("player");
	playerColliderComponent_ = player_->AddComponent<ColliderComponent>();
	player_->GetTransform()->SetLocalPos(Vec3::zero);
	playerMovementComponent_ = player_->AddComponent<PlayerMovement>();

	// 敵の生成
	for (int i = 0; i < enemyCount; ++i) {
		std::unique_ptr<Entity> enemy = std::make_unique<Entity>("enemy" + std::to_string(i));
		enemy->GetTransform()->SetLocalPos(Random::Vec3Range({ -32.0f, 0.0f, -32.0f }, { 32.0f , 32.0f, 32.0f }));
		enemyMovementComponents_.push_back(enemy->AddComponent<EnemyMovement>());
		auto collider = enemy->AddComponent<ColliderComponent>();
		if (collider) {
			enemyColliderComponents_.push_back(collider);
		} else {
			Console::Print("Failed to add ColliderComponent to enemy " + std::to_string(i));
		}
		enemyPreviousRotations_.push_back(Quaternion::identity);
		enemyTargetRotations_.push_back(Quaternion::identity);
		enemies_.push_back(std::move(enemy));
	}

	//// コリジョンコールバックの登録
	//playerColliderComponent_->RegisterCollisionCallback([this](Entity* other) {
	//	Console::Print("Player collided with: " + other->GetName());
	//	}
	//);

	//testEntColliderComponent_->RegisterCollisionCallback([this](Entity* other) {
	//	Console::Print("testEnt collided with: " + other->GetName());
	//	}
	//);

	// 剣のエンティティを作成
	swordEntity_ = std::make_unique<Entity>("sword");
	swordEntity_->GetTransform()->SetLocalPos(Vec3::zero);
	swordTransform_ = swordEntity_->GetTransform();

	cameraRoot_ = std::make_unique<Entity>("cameraBase");
	cameraRotator_ = cameraRoot_->AddComponent<CameraRotator>();
	cameraRoot_->SetParent(player_.get());
	cameraRoot_->GetTransform()->SetLocalPos(Vec3::up * 1.7f);

	// プレイヤーにカメラをアタッチ
	camera_->SetParent(cameraRoot_.get());
	camera_->GetTransform()->SetLocalPos(Vec3::forward * -2.5f);

#pragma endregion

#pragma region コンソール変数/コマンド
	ConVarManager::RegisterConVar<float>("sw_interpspeed", 15.0f, "Speed of interpolation for sword movement");
	Console::SubmitCommand("bind f11 +attack1");
	Console::SubmitCommand("bind f12 +attack2");
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
	hudSprite_->Update();
	ground_->Update();

	particleManager_->Update(EngineTimer::GetScaledDeltaTime());
	particle_->Update(EngineTimer::GetScaledDeltaTime());

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

	// 敵同士の衝突判定
	for (int i = 0; i < enemyCount; ++i) {
		for (int j = i + 1; j < enemyCount; ++j) {
			if (CheckSphereCollision(*enemyColliderComponents_[i], *enemyColliderComponents_[j])) {
				enemyMovementComponents_[i]->OnCollisionWithEnemy(enemies_[j].get());
				enemyMovementComponents_[j]->OnCollisionWithEnemy(enemies_[i].get());
			}
		}
	}

	Vec3 velocity = playerMovementComponent_->GetVelocity() * 0.254f;
	RotateMesh(previousRotation_, targetRotation_, deltaTime, velocity);

	playerObject_->SetRot(previousRotation_.ToEulerAngles());
	playerObject_->SetPos(player_->GetTransform()->GetWorldPos() + Vec3::up * 1.0f);
	playerObject_->Update();


	BehaviorInit();

	// attack3のそれぞれのパラメーターを編集
#ifdef _DEBUG
	ImGuiManager::DragVec3("attack3Restpos", attack3RestPos, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3Restrot", attack3RestRot, 0.1f, "%.2f");
	ImGui::Separator();
	ImGuiManager::DragVec3("attack3Endpos", attack3EndPos, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3EndRot", attack3EndRot, 0.1f, "%.2f");
#endif

	BehaviorUpdate(deltaTime);

	shadow_->SetPos({ player_->GetTransform()->GetWorldPos().x,0.0f, player_->GetTransform()->GetWorldPos().z });
	shadow_->Update();

	static Vec3 offset = { 760.0f, 130.0f, 0.0f };
	NumbersUpdate(combo_, digits_, offset);

	hudSprite_->Update();

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
	for (const auto& digit : digits_) {
		digit.sprite->Draw();
	}
	comboSprite_->Draw();
	hudSprite_->Draw();
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

void GameScene::CheckSwordCollision([[maybe_unused]] Entity* player, Entity* enemy) {
	// 剣の位置を取得
	Vec3 swordPos = swordEntity_->GetTransform()->GetWorldPos();
	// 敵の位置を取得
	Vec3 enemyPos = enemy->GetTransform()->GetWorldPos();

	// 剣と敵の距離を計算
	float distance = (swordPos - enemyPos).Length();

	// 当たり判定の範囲を設定
	float collisionRange = 2.0f; // 適切な範囲に調整

	// 剣が敵に当たった場合の条件
	if (distance <= collisionRange) {
		float currentTime = EngineTimer::GetTotalTime();
		if (!lastSwordHitTime_.contains(enemy) ||
			currentTime - lastSwordHitTime_[enemy] > kSwordHitInterval) {
			if (EnemyMovement* enemyMovement = enemy->GetComponent<EnemyMovement>()) {
				enemyMovement->OnSwordHit(player); // プレイヤーエンティティを渡す
			}
			combo_++;
			lastSwordHitTime_[enemy] = currentTime;
		}
	}
}

void GameScene::NumbersUpdate(int number, std::vector<Number>& digits, Vec3 offset) {
	std::string numberStr = std::to_string(number);
	while (numberStr.length() < 5) {
		numberStr = "0" + numberStr; // 5桁にするためにゼロパディング
	}

	for (size_t i = 0; i < digits.size(); ++i) {
		int digit = numberStr[i] - '0';
		digits[i].uvPos.x = digit * digits[i].uvSize.x;
		digits[i].sprite->SetUvPos(digits[i].uvPos);
		digits[i].sprite->SetUvSize(digits[i].uvSize);
		digits[i].sprite->SetPos({ offset.x + 64.0f * i, offset.y, offset.z }); // オフセットを考慮して1桁ごとに64ピクセルずらす
		digits[i].sprite->Update();
	}

	// comboSpriteを一番右の数字の右下に設置
	if (!digits.empty()) {
		Vec3 lastDigitPos = digits.back().sprite->GetPos();
		comboSprite_->SetPos({ lastDigitPos.x + 64.0f, lastDigitPos.y + 64.0f, lastDigitPos.z });
		comboSprite_->Update();
	}
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

	if (InputSystem::IsTriggered("attack1")) {
		if (playerMovementComponent_->IsGrounded()) {
			RequestBehavior(SwordBehavior::Attack1);
		} else {
			RequestBehavior(SwordBehavior::Attack3Swing);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 攻撃1
//-----------------------------------------------------------------------------
void GameScene::Attack1Init() {
	swordTransform_->SetWorldPos(playerObject_->GetPos() + previousRotation_ * attack1RestPos);
	swordTransform_->SetLocalRot(previousRotation_ * Quaternion::Euler(attack1RestRot * Math::deg2Rad));
	// 前方にふっとばす
	playerMovementComponent_->SetVelocity(previousRotation_ * Vec3::forward * Math::HtoM(2000.0f));
}

void GameScene::Attack1Update([[maybe_unused]] float deltaTime) {
	Vec3 targetPos = playerObject_->GetPos() + previousRotation_ * attack1EndPos;
	TransformComponent* swordTransform = swordEntity_->GetTransform();

	Debug::DrawAxis(playerObject_->GetPos() + previousRotation_ * attack1RestPos, previousRotation_ * Quaternion::Euler(attack1RestRot * Math::deg2Rad));

#ifdef _DEBUG
	ImGuiManager::DragVec3("attack1RestPos", attack1RestPos, 0.01f, "%.2f");
	ImGuiManager::DragVec3("attack1EndPos", attack1EndPos, 0.01f, "%.2f");
#endif

	//float interpspeed = ConVarManager::GetConVar("sw_interpspeed")->GetValueAsFloat();
	if (playerMovementComponent_->IsGrounded()) {
		swordTransform->SetWorldPos(Math::Lerp(swordTransform->GetWorldPos(), targetPos, deltaTime * 30.0f));
		swordTransform->SetLocalRot(
			Quaternion::Slerp(
				swordTransform->GetLocalRot(),
				previousRotation_ * Quaternion::Euler(attack1EndRot * Math::deg2Rad),
				deltaTime * 10.0f)
		);

		Debug::DrawAxis(targetPos, previousRotation_ * Quaternion::Euler(attack1EndRot * Math::deg2Rad));
	} else {
		// プレイヤーの前方向に剣を配置
		static Vec3 swordOffset = Vec3(0.0f, 0.0f, 1.0f); // 前方向の位置をオフセットで指定
		Vec3 swordPosition = playerObject_->GetPos() + previousRotation_ * swordOffset;

		swordTransform_->SetWorldPos(Math::Lerp(swordTransform_->GetWorldPos(), swordPosition, deltaTime * 30.0f));
		static Vec3 testRot = Vec3(90.0f, 45.0f, 90.0f);
#ifdef _DEBUG
		ImGuiManager::DragVec3("rot", testRot, 0.1f, "%.2f");
#endif
		swordTransform_->SetLocalRot(Quaternion::Slerp(swordTransform_->GetLocalRot(), previousRotation_ * Quaternion::Euler(testRot * Math::deg2Rad), deltaTime * 10.0f));

		Debug::DrawAxis(swordPosition, previousRotation_ * Quaternion::Euler(testRot * Math::deg2Rad));
	}

	sword_->SetPos(swordTransform_->GetWorldPos());
	sword_->SetRot(swordTransform_->GetWorldRot().ToEulerAngles());
	sword_->Update();

	Vec3 swordTipPosition = swordTransform_->GetWorldPos() + swordTransform_->GetLocalRot() * Vec3::up * 1.0f;
	particle_->EmitParticlesAtPosition(swordTipPosition, 0, 0.1f, Vec3::zero, Vec3::zero, 2);

	// 剣の当たり判定をチェック
	for (auto& enemy : enemies_) {
		CheckSwordCollision(player_.get(), enemy.get());
	}

	// endPosに到達したらアイドル状態に戻す
	if (swordTransform->GetWorldPos().Distance(targetPos) < 0.25f) {
		RequestBehavior(SwordBehavior::Idle);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 攻撃2
//-----------------------------------------------------------------------------
void GameScene::Attack2Init() {}

void GameScene::Attack2Update([[maybe_unused]] float deltaTime) {}

void GameScene::Attack3SwingInit() {
	strikeTimer_ = 0.0f;
	swordTransform_->SetWorldPos(playerObject_->GetPos() + previousRotation_ * attack3swingPos);
	swordTransform_->SetLocalRot(previousRotation_ * Quaternion::Euler(attack3swingRot * Math::deg2Rad));
	// 斜上前にふっとばす
	playerMovementComponent_->SetVelocity(previousRotation_ * Vec3::forward * Math::HtoM(200.0f) + Vec3::up * Math::HtoM(600.0f));
}

void GameScene::Attack3SwingUpdate(float deltaTime) {
	Debug::DrawAxis(playerObject_->GetPos() + previousRotation_ * attack3swingPos, previousRotation_ * Quaternion::Euler(attack3swingRot * Math::deg2Rad));

	//float interpspeed = ConVarManager::GetConVar("sw_interpspeed")->GetValueAsFloat();

	// TODO : 通常のdeltaTimeに戻す

	strikeTimer_ += deltaTime;

	Console::Print(std::format("strikeTimer: {}", strikeTimer_), Vec4::orange, Channel::kGame);

	// attack3swingPosの編集
#ifdef _DEBUG
	ImGuiManager::DragVec3("attack3swingPos", attack3swingPos, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3swingRot", attack3swingRot, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3swingEndPos", attack3swingEndPos, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3swingEndRot", attack3swingEndRot, 0.1f, "%.2f");
#endif	

	Debug::DrawAxis(playerObject_->GetPos() + previousRotation_ * attack3swingEndPos, previousRotation_ * Quaternion::Euler(attack3swingEndRot * Math::deg2Rad));

	if (strikeTimer_ < strikeTime_) {
		Vec3 targetPos = playerObject_->GetPos() + previousRotation_ * attack3swingEndPos;
		swordTransform_->SetWorldPos(Math::Lerp(swordTransform_->GetWorldPos(), targetPos, deltaTime * 10));

		swordTransform_->SetLocalRot(
			Quaternion::Slerp(
				swordTransform_->GetLocalRot(),
				previousRotation_ * Quaternion::Euler(attack3swingEndRot * Math::deg2Rad),
				deltaTime * 4)
		);
	} else {
		RequestBehavior(SwordBehavior::Attack3);
	}
	sword_->SetPos(swordTransform_->GetWorldPos());
	sword_->SetRot(swordTransform_->GetWorldRot().ToEulerAngles());
	sword_->Update();

	Vec3 swordTipPosition = swordTransform_->GetWorldPos() + swordTransform_->GetLocalRot() * Vec3::up * 1.0f;
	particle_->EmitParticlesAtPosition(swordTipPosition, 0, 0.1f, Vec3::zero, Vec3::zero, 2);
}

//-----------------------------------------------------------------------------
// Purpose: 攻撃3
//-----------------------------------------------------------------------------
void GameScene::Attack3Init() {
	swordTransform_->SetWorldPos(playerObject_->GetPos() + previousRotation_ * attack3RestPos);
	swordTransform_->SetLocalRot(previousRotation_ * Quaternion::Euler(attack3RestRot * Math::deg2Rad));
	// ストライク!
	playerMovementComponent_->SetVelocity(previousRotation_ * Vec3::forward * Math::HtoM(200.0f) + -Vec3::up * Math::HtoM(1000.0f));
}

void GameScene::Attack3Update([[maybe_unused]] float deltaTime) {
	float interpspeed = ConVarManager::GetConVar("sw_interpspeed")->GetValueAsFloat();

	// TODO : 通常のdeltaTimeに戻す

	Debug::DrawAxis(playerObject_->GetPos() + previousRotation_ * attack3RestPos, previousRotation_ * Quaternion::Euler(attack3RestRot * Math::deg2Rad));

#ifdef _DEBUG
	ImGuiManager::DragVec3("attack3swingPos", attack3RestPos, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3swingRot", attack3RestRot, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3swingEndPos", attack3EndPos, 0.1f, "%.2f");
	ImGuiManager::DragVec3("attack3swingEndRot", attack3EndRot, 0.1f, "%.2f");
#endif

	Debug::DrawAxis(playerObject_->GetPos() + previousRotation_ * attack3RestPos, previousRotation_ * Quaternion::Euler(attack3RestRot));
	Debug::DrawAxis(playerObject_->GetPos() + previousRotation_ * attack3EndPos, previousRotation_ * Quaternion::Euler(attack3EndRot));

	Vec3 targetPos = playerObject_->GetPos() + previousRotation_ * attack3EndPos;
	swordTransform_->SetWorldPos(Math::Lerp(swordTransform_->GetWorldPos(), targetPos, deltaTime * interpspeed));

	swordTransform_->SetLocalRot(
		Quaternion::Slerp(
			swordTransform_->GetLocalRot(),
			previousRotation_ * Quaternion::Euler(attack3EndRot * Math::deg2Rad),
			deltaTime * interpspeed)
	);

	sword_->SetPos(swordTransform_->GetWorldPos());
	sword_->SetRot(swordTransform_->GetWorldRot().ToEulerAngles());
	sword_->Update();


	Vec3 swordTipPosition = swordTransform_->GetWorldPos() + swordTransform_->GetLocalRot() * Vec3::up * 1.0f;
	particle_->EmitParticlesAtPosition(swordTipPosition, 0, 0.1f, Vec3::zero, Vec3::zero, 2);

	// 剣の当たり判定をチェック
	for (auto& enemy : enemies_) {
		CheckSwordCollision(player_.get(), enemy.get());
	}

	if (playerMovementComponent_->IsGrounded()) {
		RequestBehavior(SwordBehavior::Idle);
	}
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
		case SwordBehavior::Attack3Swing:
			Attack3SwingInit();
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
	case SwordBehavior::Attack3Swing:
		Attack3SwingUpdate(deltaTime);
		break;
	}
}
