#include "EmptyScene.h"

#include "engine/Engine.h"
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/Components/MeshRenderer/SkeletalMeshRenderer.h>
#include <engine/Components/MeshRenderer/StaticMeshRenderer.h>
#include <engine/TextureManager/TexManager.h>

EmptyScene::~EmptyScene() = default;

void EmptyScene::Init() {
	mRenderer = Unnamed::Engine::GetRenderer();
	mSrvManager = Unnamed::Engine::GetSrvManager();

	mResourceManager = Unnamed::Engine::GetResourceManager();

	{
		TexManager::GetInstance()->LoadTexture("./content/core/textures/uvChecker.png", false);

		TexManager::GetInstance()->LoadTexture(
			"./content/core/textures/wave.dds", true
		);

		mCubeMap = std::make_unique<CubeMap>(
			mRenderer->GetDevice(),
			mSrvManager,
			"./content/core/textures/wave.dds"
		);
	}

	mMeshEntity = std::make_unique<Entity>(
		"meshEntity",
		EntityType::Shared
	);

	mResourceManager->GetMeshManager()->LoadMeshFromFile(
		"./content/core/models/reflectionTest.obj"
	);

	auto meshRenderer = mMeshEntity->AddComponent<StaticMeshRenderer>();
	meshRenderer->SetStaticMesh(
		mResourceManager->GetMeshManager()->GetStaticMesh(
			"./content/core/models/reflectionTest.obj"
		)
	);

	[[maybe_unused]] auto meshCollider = mMeshEntity->AddComponent<
		MeshColliderComponent>();
	mMeshEntity->GetTransform()->SetLocalPos(
		Vec3::one * 16.0f
	);

	AddEntity(mMeshEntity.get());

	mPhysicsEngine = std::make_unique<UPhysics::Engine>();
	mPhysicsEngine->Init();

	mPhysicsEngine->RegisterEntity(mMeshEntity.get());
}

void EmptyScene::Update(const float deltaTime) {
	// 基本的な更新処理
	mCubeMap->Update(deltaTime);


	// シーン内のすべてのエンティティを更新
	for (auto entity : mEntities) {
		if (entity->IsActive() && !entity->GetParent()) {
			entity->Update(deltaTime);
		}
	}

	mPhysicsEngine->Update(deltaTime);
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
