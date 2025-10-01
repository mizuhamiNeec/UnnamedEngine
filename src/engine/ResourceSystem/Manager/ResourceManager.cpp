

#include "engine/ResourceSystem/Manager/ResourceManager.h"

#include "engine/OldConsole/Console.h"
#include "engine/renderer/D3D12.h"
#include "engine/renderer/SrvManager.h"
#include "engine/ResourceSystem/RootSignature/RootSignatureManager2.h"
#include "engine/TextureManager/TexManager.h"

ResourceManager::ResourceManager(D3D12* d3d12) :
	d3d12_(d3d12),
	srvManager_(nullptr),
	shaderManager_(nullptr),
	materialManager_(nullptr),
	meshManager_(nullptr) {
	srvManager_       = std::make_unique<SrvManager>();
	shaderManager_    = std::make_unique<ShaderManager>();
	materialManager_  = std::make_unique<MaterialManager>();
	meshManager_      = std::make_unique<MeshManager>();
	animationManager_ = std::make_unique<AnimationManager>();
}

void ResourceManager::Init() const {
	Console::Print("ResourceManager を初期化しています...\n", kConTextColorWait,
	               Channel::ResourceSystem);
	// マネージャーを初期化
	srvManager_->Init(d3d12_);
	TexManager::GetInstance()->Init(d3d12_, srvManager_.get());
	RootSignatureManager2::Init(d3d12_->GetDevice());
	shaderManager_->Init();
	materialManager_->Init();
	meshManager_->Init(d3d12_->GetDevice(),
	                   shaderManager_.get(), materialManager_.get());

	animationManager_->Init();

	Console::Print("ResourceManager の初期化が完了しました\n", kConTextColorCompleted,
	               Channel::ResourceSystem);
}

void ResourceManager::Shutdown() {
	Console::Print(
		"ResourceManager を終了しています...\n", kConTextColorWait,
		Channel::ResourceSystem
	);
	
	// 全てのリソースマネージャーを終了

	if (animationManager_) {
		animationManager_->Shutdown();
		animationManager_.reset();
	}

	if (meshManager_) {
		meshManager_->Shutdown();
		meshManager_.reset();
	}

	if (materialManager_) {
		materialManager_->Shutdown();
		materialManager_.reset();
	}

	if (shaderManager_) {
		shaderManager_->Shutdown();
		shaderManager_.reset();
	}

	TexManager::Shutdown();

	RootSignatureManager2::Shutdown();
}

SrvManager* ResourceManager::GetSrvManager() const {
	return srvManager_.get();
}

TexManager* ResourceManager::GetTexManager() const {
	return TexManager::GetInstance();
}

MeshManager* ResourceManager::GetMeshManager() const {
	return meshManager_.get();
}

AnimationManager* ResourceManager::GetAnimationManager() const {
	return animationManager_.get();
}

MaterialManager* ResourceManager::GetMaterialManager() const {
	return materialManager_.get();
}

ShaderManager* ResourceManager::GetShaderManager() const {
	return shaderManager_.get();
}
