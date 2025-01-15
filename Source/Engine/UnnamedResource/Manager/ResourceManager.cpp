#include "ResourceManager.h"

#include "Renderer/D3D12Utils.h"

ResourceManager::ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT descriptorCount) :
	device_(device),
	commandList_(commandList),
	srvAllocator_(device, descriptorCount) {}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& filePath) {
	// 読み込み済みテクスチャを検索
	if (textures_.contains(filePath)) {
		return textures_[filePath];
	}

	auto texture = std::make_shared<Texture>(filePath, device_, commandList_, &srvAllocator_);

	return texture;
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::GetSrvCpuHandle(const std::shared_ptr<Texture>& texture) {
	return texture->GetSrvCPUHandle();

}
