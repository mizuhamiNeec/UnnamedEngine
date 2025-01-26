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
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");
#pragma endregion

#pragma region スプライト類

#pragma endregion

#pragma region 3Dオブジェクト類
	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("bunny.obj");

	ground_ = std::make_unique<Object3D>();
	ground_->Init(object3DCommon_);
	ground_->SetModel("bunny.obj");
	ground_->SetPos(Vec3::zero);
	ground_->SetRot(Vec3::up * 180.0f * Math::deg2Rad);
#pragma endregion

#pragma region パーティクル類
	particleManager_->CreateParticleGroup("circle", "./Resources/Textures/doublestar.png");

	particle_ = std::make_unique<ParticleObject>();
	particle_->Init(particleManager_, "./Resources/Textures/doublestar.png");
#pragma endregion
}

void GameScene::Update(const float deltaTime) {
	deltaTime;
	ground_->Update();
}

void GameScene::Render() {
	//----------------------------------------
	// オブジェクト3D共通描画設定
	object3DCommon_->Render();
	//----------------------------------------
	ground_->Draw();

	//----------------------------------------
	// パーティクル共通描画設定
	particleManager_->Render();
	//----------------------------------------

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------
}

void GameScene::Shutdown() {
	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();
	particleManager_->Shutdown();
}
