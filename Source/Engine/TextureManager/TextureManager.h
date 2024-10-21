#pragma once

#include <d3d12.h>
#include <string>
#include <wrl/client.h>

#include "DirectXTex/DirectXTex.h"

class D3D12;
using namespace Microsoft::WRL;

class TextureManager {
public:
	// シングルトンインスタンスの取得
	static TextureManager* GetInstance();

	void Initialize(D3D12* renderer);
	void Shutdown();

	HRESULT CreateBufferResource(size_t size) const;
	void LoadTexture(const std::string& filePath);

	static void UploadTextureData(const ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

	// SRVインデックスの開始番号
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	// テクスチャ番号からGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

	uint32_t GetLoadedTextureCount() const {
		return static_cast<uint32_t>(textureData_.size());
	}

private:
	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata) const;

private:
	// テクスチャ1枚分のデータ
	struct TextureData {
		std::string filePath;
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	// テクスチャデータ
	std::vector<TextureData> textureData_;

	D3D12* renderer_;

	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

private:
	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;
};
