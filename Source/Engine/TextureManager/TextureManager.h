#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <wrl/client.h>

#include "DirectXTex/DirectXTex.h"
#include "Texture.h"

using namespace Microsoft::WRL;

class TextureManager {
public:
	// シングルトンインスタンスの取得
	static TextureManager* GetInstance();

	std::shared_ptr<Texture> GetTexture(ID3D12Device* device, const std::wstring& filename); // TODO : 削除予定

	void Initialize(const ComPtr<ID3D12Device>& device);
	void Shutdown();

	void LoadTexture(const std::string& filePath);

	void ReleaseUnusedTextures(); // TODO : 削除予定

private:
	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata) const;

private:
	// テクスチャ1枚分のデータ
	struct TextureData {
		std::string filePath;
		DirectX::TexMetadata metadata;
		ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	// テクスチャデータ
	std::vector<TextureData> textureData_;

	ComPtr<ID3D12Device> device_ = nullptr;

	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

	std::unordered_map<std::wstring, std::shared_ptr<Texture>> textures; // TODO : 削除予定

private:
	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;
};
