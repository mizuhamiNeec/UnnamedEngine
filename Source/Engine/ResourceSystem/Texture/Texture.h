#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>

#include <DirectXTex/DirectXTex.h>

#include <Renderer/D3D12.h>

using Microsoft::WRL::ComPtr;

class ShaderResourceViewManager;

class Texture {
public:
	Texture()  = default;
	~Texture() = default;

	bool LoadFromFile(
		D3D12*             d3d12,
		SrvManager*        shaderResourceViewManager,
		const std::string& filePath
	);

	void FromOldTextureManager(const std::string& path);

	D3D12_GPU_DESCRIPTOR_HANDLE GetShaderResourceView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceViewCPUHandle();

	std::string GetFilePath() const;

	ComPtr<ID3D12Resource> GetResource() const;

	bool CreateErrorTexture(D3D12*      d3d12,
	                        SrvManager* shaderResourceViewManager);

private:
	ComPtr<ID3D12Resource>        CreateTextureResource(ID3D12Device* device);
	static ComPtr<ID3D12Resource> UploadTextureData(
		ID3D12Device*                 device,
		ID3D12GraphicsCommandList*    commandList,
		const ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage&  mipImages
	);
	static ComPtr<ID3D12Resource> UploadTextureData(
		ID3D12Device*                 device,
		ID3D12GraphicsCommandList*    commandList,
		const ComPtr<ID3D12Resource>& texture,
		const D3D12_SUBRESOURCE_DATA& textureData
	);

	ComPtr<ID3D12Resource> textureResource_;

	DirectX::TexMetadata metadata_;

	std::string filePath_;

	D3D12_GPU_DESCRIPTOR_HANDLE handle_    = {};
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_ = {};
};
