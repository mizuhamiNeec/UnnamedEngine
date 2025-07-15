#include "EmptyScene.h"
#include <Engine.h>
#include <Input/InputSystem.h>
#include <Debug/Debug.h>

#include "Components/MeshRenderer/SkeletalMeshRenderer.h"

EmptyScene::~EmptyScene() {
	// クリーンアップ処理
}

void EmptyScene::Init() {
	mRenderer   = Engine::GetRenderer();
	mSrvManager = Engine::GetSrvManager();

	mResourceManager = Engine::GetResourceManager();

	{
		TexManager::GetInstance()->LoadTexture(
			"./Resources/Textures/wave.dds", true
		);

		// キューブマップのみ初期化
		mCubeMap = std::make_unique<CubeMap>(
			mRenderer->GetDevice(),
			mSrvManager,
			"./Resources/Textures/wave.dds"
		);
	}

	TexManager::GetInstance()->LoadTexture(
		"./Resources/Textures/uvChecker.png"
	);

	//"./Resources/Models/man/man.gltf"
	//"./Resources/Models/human/sneakWalk.gltf"
	mResourceManager->GetMeshManager()->LoadSkeletalMeshFromFile(
		"./Resources/Models/man/man.gltf"
	);

	mSkeletalMeshEntity = std::make_unique<Entity>("SkeletalMeshEntity");
	auto sklMesh = mSkeletalMeshEntity->AddComponent<SkeletalMeshRenderer>();

	auto skeletalMesh = mResourceManager->GetMeshManager()->GetSkeletalMesh(
		"./Resources/Models/man/man.gltf"
	);
	sklMesh->SetSkeletalMesh(skeletalMesh);

	AddEntity(mSkeletalMeshEntity.get());

	Console::Print("EmptyScene initialized");
}

void EmptyScene::Update(float deltaTime) {
	// 基本的な更新処理
	mCubeMap->Update(deltaTime);

	// シーン内のすべてのエンティティを更新
	for (auto entity : mEntities) {
		if (entity->IsActive()) {
			entity->Update(deltaTime);
		}
	}
}

void EmptyScene::Render() {
	// キューブマップの描画のみ実行
	if (mCubeMap) {
		mCubeMap->Render(mRenderer->GetCommandList());
	}

	// シーン内のすべてのエンティティを描画
	for (auto entity : mEntities) {
		if (entity->IsActive()) {
			entity->Render(mRenderer->GetCommandList());
		}
	}
}

void EmptyScene::Shutdown() {
	// リソースの解放
	mCubeMap.reset();
}
