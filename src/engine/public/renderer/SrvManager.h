#pragma once
#include <cstdint>

#include <wrl/client.h>

#include <engine/public/renderer/D3D12.h>

class D3D12;

class SrvManager {
public:
	void Init(D3D12* d3d12);
	void PreDraw() const;

	uint32_t Allocate();  // 従来のAllocate（互換性のため残す）
	uint32_t AllocateForTexture();           // テクスチャ用SRVの割り当て
	uint32_t AllocateForStructuredBuffer();  // ストラクチャードバッファ用SRVの割り当て

	// 連続したテクスチャスロットを確保
	uint32_t AllocateConsecutiveTextureSlots(uint32_t count);
	
	ID3D12DescriptorHeap* GetDescriptorHeap() const;
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels) const;
	void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements,
		UINT structureByteStride) const;
	void CreateSRVForTextureCube(uint32_t index, Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	                             DXGI_FORMAT format, UINT mipLevels);
	void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex) const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(std::uint32_t index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

	// 連続したスロットの先頭インデックスからGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetConsecutiveGPUHandle(uint32_t startIndex) const;

	bool CanAllocate() const;

private:
	D3D12* d3d12_ = nullptr;

	// 次に使用するSRVインデックス（従来の共通使用）
	uint32_t useIndex_ = kSrvIndexTop;
	
	// テクスチャ用SRVの次のインデックス
	uint32_t textureIndex_ = kTextureStartIndex;
	
	// ストラクチャードバッファ用SRVの次のインデックス
	uint32_t structuredBufferIndex_ = kStructuredBufferStartIndex;

	// SRV用のディスクリプタサイズ
	uint32_t descriptorSize_ = 0;
	// SRV用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
};