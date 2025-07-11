#include "EmptyScene.h"
#include <Engine.h>
#include <Input/InputSystem.h>
#include <Debug/Debug.h>

#include "Components/MeshRenderer/SkeletalMeshRenderer.h"

EmptyScene::~EmptyScene() {
	// クリーンアップ処理
}

void EmptyScene::Init() {
	renderer_   = Engine::GetRenderer();
	srvManager_ = Engine::GetSrvManager();

	resourceManager_ = Engine::GetResourceManager();

	{
		TexManager::GetInstance()->LoadTexture(
			"./Resources/Textures/wave.dds"
		);

		// キューブマップのみ初期化
		cubeMap_ = std::make_unique<CubeMap>(
			renderer_->GetDevice(),
			srvManager_,
			"./Resources/Textures/wave.dds"
		);
	}

	TexManager::GetInstance()->LoadTexture(
		"./Resources/Textures/uvChecker.png"
	);

	resourceManager_->GetMeshManager()->LoadSkeletalMeshFromFile(
		"./Resources/Models/man/man.gltf");

	skeletalMeshEntity_ = std::make_unique<Entity>("SkeletalMeshEntity");
	auto sklMesh = skeletalMeshEntity_->AddComponent<SkeletalMeshRenderer>();

	auto skeletalMesh = resourceManager_->GetMeshManager()->GetSkeletalMesh(
		"./Resources/Models/man/man.gltf");
	sklMesh->SetSkeletalMesh(skeletalMesh);

	AddEntity(skeletalMeshEntity_.get());

	Console::Print("EmptyScene initialized");
}

void EmptyScene::Update(float deltaTime) {
	// 基本的な更新処理
	cubeMap_->Update(deltaTime);

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
