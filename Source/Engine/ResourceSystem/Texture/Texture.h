#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>

#include <DirectXTex/DirectXTex.h>

#include <Renderer/D3D12.h>

#include <ResourceSystem/SRV/ShaderResourceViewManager.h>

using Microsoft::WRL::ComPtr;

class Texture {
public:
	Texture() = default;
	~Texture() = default;

	bool LoadFromFile(
		D3D12* d3d12,
		ShaderResourceViewManager* shaderResourceViewManager,
		const std::string& filePath
	);

	D3D12_GPU_DESCRIPTOR_HANDLE GetShaderResourceView() const;

	ComPtr<ID3D12Resource> GetResource() const {
		return textureResource_;
	}

	bool CreateErrorTexture(D3D12* d3d12, ShaderResourceViewManager* shaderResourceViewManager);

private:
	ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device);
	static ComPtr<ID3D12Resource> UploadTextureData(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		const ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages
	);
	static ComPtr<ID3D12Resource> UploadTextureData(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		const ComPtr<ID3D12Resource>& texture,
		const D3D12_SUBRESOURCE_DATA& textureData
	);

	ComPtr<ID3D12Resource> textureResource_;
	ComPtr<ID3D12Resource> uploadHeap_;

	DirectX::TexMetadata metadata_;

	D3D12_GPU_DESCRIPTOR_HANDLE handle_ = {};
};

