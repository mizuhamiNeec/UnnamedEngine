#include "ResourceManager.h"

#include "SubSystem/Console/Console.h"
#include "ResourceSystem/RootSignature/RootSignatureManager2.h"

ResourceManager::ResourceManager(D3D12* d3d12) :
	d3d12_(d3d12),
	shaderManager_(nullptr),
	materialManager_(nullptr),
	meshManager_(nullptr) {
	shaderManager_   = std::make_unique<ShaderManager>();
	materialManager_ = std::make_unique<MaterialManager>();
	meshManager_     = std::make_unique<MeshManager>();
}

void ResourceManager::Init() const {
	Console::Print("ResourceManager を初期化しています...\n", kConTextColorWait,
	               Channel::ResourceSystem);
	// マネージャーを初期化
	RootSignatureManager2::Init(d3d12_->GetDevice());
	shaderManager_->Init();
	materialManager_->Init();
	meshManager_->Init(d3d12_->GetDevice(),
	                   shaderManager_.get(), materialManager_.get());

	Console::Print("ResourceManager の初期化が完了しました\n", kConTextColorCompleted,
	               Channel::ResourceSystem);
}

void ResourceManager::Shutdown() {
	Console::Print(
		"ResourceManager を終了しています...\n", kConTextColorWait,
		Channel::ResourceSystem
	);

	// 全てのリソースマネージャーを終了
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
	
	RootSignatureManager2::Shutdown();
}

MeshManager* ResourceManager::GetMeshManager() const {
	return meshManager_.get();
}

MaterialManager* ResourceManager::GetMaterialManager() const {
	return materialManager_.get();
}

ShaderManager* ResourceManager::GetShaderManager() const {
	return shaderManager_.get();
}
