#include "game/public/scene/EmptyScene.h"

#include "engine/public/Engine.h"
#include "engine/public/Components/MeshRenderer/SkeletalMeshRenderer.h"
#include "engine/public/TextureManager/TexManager.h"

EmptyScene::~EmptyScene() {
	// クリーンアップ処理
}

void EmptyScene::Init() {
	mRenderer   = Unnamed::Engine::GetRenderer();
	mSrvManager = Unnamed::Engine::GetSrvManager();

	mResourceManager = Unnamed::Engine::GetResourceManager();

	{
		TexManager::GetInstance()->LoadTexture(
			"./resources/textures/wave.dds", true
		);

		mCubeMap = std::make_unique<CubeMap>(
			mRenderer->GetDevice(),
			mSrvManager,
			"./resources/textures/wave.dds"
		);
	}
}

void EmptyScene::Update(float deltaTime) {
	// 基本的な更新処理
	mCubeMap->Update(deltaTime);

	// シーン内のすべてのエンティティを更新
	for (auto entity : mEntities) {
		if (entity->IsActive() && !entity->GetParent()) {
			entity->Update(deltaTime);
		}
	}
}

void EmptyScene::Render() {
	if (mCubeMap) {
		mCubeMap->Render(mRenderer->GetCommandList());
	}

	for (auto entity : mEntities) {
		if (entity->IsActive()) {
			entity->Render(mRenderer->GetCommandList());
		}
	}
}

void EmptyScene::Shutdown() {
}
