#include "ResourceManager.h"

#include "SubSystem/Console/Console.h"
#include "ResourceSystem/RootSignature/RootSignatureManager2.h"

ResourceManager::ResourceManager(D3D12* d3d12) :
	d3d12_(d3d12),
	textureManager_(nullptr),
	shaderManager_(nullptr),
	materialManager_(nullptr),
	meshManager_(nullptr) {
	srvManager_ = std::make_unique<ShaderResourceViewManager>(d3d12->GetDevice());
	textureManager_ = std::make_unique<TextureManager>();
	shaderManager_ = std::make_unique<ShaderManager>();
	materialManager_ = std::make_unique<MaterialManager>();
	meshManager_ = std::make_unique<MeshManager>();
}

void ResourceManager::Init() const {
	Console::Print("ResourceManager を初期化しています...\n", kConTextColorWait, Channel::ResourceSystem);

	// ImGuiで使ったら初期化
	srvManager_->Init();

	// マネージャーを初期化
	RootSignatureManager2::Init(d3d12_->GetDevice());
	textureManager_->Init(d3d12_, srvManager_.get());
	shaderManager_->Init();
	materialManager_->Init();
	meshManager_->Init(d3d12_->GetDevice(), textureManager_.get(), shaderManager_.get(), materialManager_.get());

	Console::Print("ResourceManager の初期化が完了しました\n", kConTextColorCompleted, Channel::ResourceSystem);
}

void ResourceManager::Shutdown() {
	Console::Print("ResourceManager を終了しています...\n", kConTextColorWait, Channel::ResourceSystem);

	//#ifdef _DEBUG
	//	ComPtr<ID3D12DebugDevice> debugDevice;
	//	if (SUCCEEDED(d3d12_->GetDevice()->QueryInterface(IID_PPV_ARGS(&debugDevice)))) {
	//		// 詳細なリーク情報を出力
	//		debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
	//	}
	//#endif

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

	if (textureManager_) {
		textureManager_->Shutdown();
		textureManager_.reset();
	}

	RootSignatureManager2::Shutdown();

	if (srvManager_) {
		srvManager_->Shutdown();
		srvManager_.reset();
	}

	d3d12_ = nullptr;
}

TextureManager* ResourceManager::GetTextureManager() const {
	return textureManager_.get();
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

ShaderResourceViewManager* ResourceManager::GetShaderResourceViewManager() const {
	return srvManager_.get();
}

