#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>

#include <UnnamedResource/Manager/ShaderResourceViewManager.h>

#include "DirectXTex/DirectXTex.h"
#include "Renderer/D3D12.h"

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

private:
	ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device);
	ComPtr<ID3D12Resource> UploadTextureData(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const ComPtr<ID3D12Resource>& texture, const DirectX::
		ScratchImage& mipImages) const;

	ComPtr<ID3D12Resource> textureResource_;
	ComPtr<ID3D12Resource> uploadHeap_;

	DirectX::TexMetadata metadata_;

	D3D12_GPU_DESCRIPTOR_HANDLE handle_ = {};
};

