#pragma once

#include <d3d12.h>
#include <DirectXTex/DirectXTex.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace D3D12Utils {
	ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);
	ComPtr<ID3D12Resource> UploadTextureData(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const ComPtr<ID3D12Resource>& textureResource, const DirectX::ScratchImage& mipImages);
	void CreateSRVForTexture2D(ID3D12Device* device, ID3D12Resource* resource, const DirectX::TexMetadata& metadata, D3D12_CPU_DESCRIPTOR_HANDLE
	                           srvHandle);
	void CreateSRVForStructuredBuffer(ID3D12Device* device, ID3D12Resource* resource, UINT numElements, UINT structureByteStride, D3D12_CPU_DESCRIPTOR_HANDLE
	                                  srvHandle);
}
