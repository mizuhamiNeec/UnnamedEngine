#include "game/public/scene/EmptyScene.h"

#include "engine/public/Engine.h"
#include "engine/public/Camera/CameraManager.h"
#include "engine/public/Components/MeshRenderer/SkeletalMeshRenderer.h"
#include "engine/public/TextureManager/TexManager.h"

#include "game/public/components/PlayerMovement.h"

EmptyScene::~EmptyScene() {
	// クリーンアップ処理
}

void EmptyScene::Init() {
	mRenderer   = Unnamed::Engine::GetRenderer();
	mSrvManager = Unnamed::Engine::GetSrvManager();

	mResourceManager = Unnamed::Engine::GetResourceManager();

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

	mEntSkeletalMesh = std::make_unique<Entity>("SkeletalMeshEntity");
	auto sklMesh     = mEntSkeletalMesh->AddComponent<SkeletalMeshRenderer>();

	auto skeletalMesh = mResourceManager->GetMeshManager()->GetSkeletalMesh(
		"./Resources/Models/man/man.gltf"
	);
	sklMesh->SetSkeletalMesh(skeletalMesh);

	AddEntity(mEntSkeletalMesh.get());

	mEntPlayer    = std::make_unique<Entity>("PlayerEntity");
	auto movement = mEntPlayer->AddComponent<PlayerMovement>();
	movement;

	mEntSkeletalMesh->SetParent(mEntPlayer.get());

	AddEntity(mEntPlayer.get());

	mEntCamera = std::make_unique<Entity>("CameraEntity");
	// 生ポインタを取得
	CameraComponent* rawCameraPtr = mEntCamera->AddComponent<CameraComponent>();
	// 生ポインタを std::shared_ptr に変換
	const auto camera = std::shared_ptr<CameraComponent>(
		rawCameraPtr, [](CameraComponent*) {
		}
	);
	CameraManager::AddCamera(camera);

	mEntCamera->SetParent(mEntPlayer.get());
	AddEntity(mEntCamera.get());

	Console::Print("EmptyScene initialized");
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
