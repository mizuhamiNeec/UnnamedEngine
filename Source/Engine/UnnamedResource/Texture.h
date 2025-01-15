#pragma once

#include <d3d12.h>
#include <DirectXTex/DirectXTex.h>
#include <UnnamedResource/Base/Resource.h>
#include <UnnamedResource/SrvAllocator/SrvAllocator.h>
#include <wrl/client.h>

#ifdef _DEBUG
#include "imgui.h"
#endif

class Texture : public Resource {
public:
	Texture(const std::string& filePath, ID3D12Device* device, ID3D12GraphicsCommandList* commandList, SrvAllocator* srvAllocator);
	~Texture() override = default;
	void Unload() override;

	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCPUHandle() const;
	[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGPUHandle() const;
	[[nodiscard]] const DirectX::TexMetadata& GetMetaData() const;
	ComPtr<ID3D12Resource> GetResource() const;

#ifdef _DEBUG
	[[nodiscard]] ImTextureID GetImTextureID() const {
		if (srvGPUHandle_.ptr) {
			return srvGPUHandle_.ptr;
		}
		return ImTextureID();
	}
#endif

private:
	void LoadFromFile(const std::string& filePath);
	void CreateTextureResource();
	void UploadTextureData();
	void CreateShaderResourceView();

private:
	std::string filePath_;
	DirectX::TexMetadata metadata_;
	DirectX::ScratchImage mipImages_;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap_;
	D3D12_CPU_DESCRIPTOR_HANDLE srvCPUHandle_;
	D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle_;

	ID3D12Device* device_;
	ID3D12GraphicsCommandList* commandList_;
	SrvAllocator* srvAllocator_;
};

