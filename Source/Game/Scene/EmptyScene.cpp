#include "EmptyScene.h"
#include <Engine.h>
#include <Input/InputSystem.h>
#include <Debug/Debug.h>

EmptyScene::~EmptyScene() {
	// クリーンアップ処理
}

void EmptyScene::Init() {
	renderer_   = Engine::GetRenderer();
	srvManager_ = Engine::GetSrvManager();

	// キューブマップのみ初期化
	cubeMap_ = std::make_unique<CubeMap>(
		renderer_->GetDevice(),
		srvManager_,
		"./Resources/Textures/wave.dds"
	);

	Console::Print("EmptyScene initialized");
}

void EmptyScene::Update(float deltaTime) {
	// 基本的な更新処理

	// シーン内のすべてのエンティティを更新
	for (auto entity : entities_) {
		if (entity->IsActive()) {
			entity->Update(deltaTime);
		}
	}
}

void EmptyScene::Render() {
	// キューブマップの描画のみ実行
	if (cubeMap_) {
		cubeMap_->Render(renderer_->GetCommandList());
	}

	// シーン内のすべてのエンティティを描画
	for (auto entity : entities_) {
		if (entity->IsActive()) {
			entity->Render(renderer_->GetCommandList());
		}
	}
}

void EmptyScene::Shutdown() {
	// リソースの解放
	cubeMap_.reset();
}
