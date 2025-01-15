#pragma once
#include <memory>
#include <string>
#include <Lib/Console/Console.h>
#include <UnnamedResource/Texture.h>
#include <UnnamedResource/SrvAllocator/SrvAllocator.h>


class ResourceManager {
public:
	ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT descriptorCount);
	~ResourceManager() = default;

	std::shared_ptr<Texture> LoadTexture(const std::string& filePath);

	static D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle(const std::shared_ptr<Texture>& texture);

private:
	ID3D12Device* device_;
	ID3D12GraphicsCommandList* commandList_;
	SrvAllocator srvAllocator_;

	std::unordered_map<std::string, std::shared_ptr<Texture>> textures_;
};
