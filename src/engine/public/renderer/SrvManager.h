#pragma once
#include <cstdint>
#include <queue>
#include <vector>

#include <wrl/client.h>

#include <engine/public/renderer/D3D12.h>

class D3D12;

class SrvManager {
public:
	void Init(D3D12* d3d12);
	void PreDraw() const;

	uint32_t Allocate();  // 従来のAllocate（互換性のため残す）
	uint32_t AllocateForTexture2D();             // 2Dテクスチャ用SRVの割り当て
	uint32_t AllocateForTextureCube();           // キューブマップテクスチャ用SRVの割り当て
	uint32_t AllocateForTextureArray();          // テクスチャ配列用SRVの割り当て（将来用）
	uint32_t AllocateForStructuredBuffer();      // ストラクチャードバッファ用SRVの割り当て

	// 互換性のためのメソッド
	uint32_t AllocateForTexture();               // AllocateForTexture2Dにリダイレクト

	// 連続したテクスチャスロットを確保
	uint32_t AllocateConsecutiveTexture2DSlots(uint32_t count);
	uint32_t AllocateConsecutiveTextureCubeSlots(uint32_t count);
	
	// 互換性のためのメソッド
	uint32_t AllocateConsecutiveTextureSlots(uint32_t count); // AllocateConsecutiveTexture2DSlotsにリダイレクト

	// インデックス返却（再利用可能にする）
	void DeallocateTexture2D(uint32_t index);
	void DeallocateTextureCube(uint32_t index);
	void DeallocateTextureArray(uint32_t index);
	void DeallocateStructuredBuffer(uint32_t index);
	
	// 互換性のためのメソッド
	void DeallocateTexture(uint32_t index);     // DeallocateTexture2Dにリダイレクト

	// 連続したスロットの返却
	void DeallocateConsecutiveTexture2DSlots(uint32_t startIndex, uint32_t count);
	void DeallocateConsecutiveTextureCubeSlots(uint32_t startIndex, uint32_t count);
	
	// 互換性のためのメソッド
	void DeallocateConsecutiveTextureSlots(uint32_t startIndex, uint32_t count); // DeallocateConsecutiveTexture2DSlotsにリダイレクト
	
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
	
	// 2Dテクスチャ用SRVの次のインデックス
	uint32_t texture2DIndex_ = kTexture2DStartIndex;
	
	// キューブマップテクスチャ用SRVの次のインデックス
	uint32_t textureCubeIndex_ = kTextureCubeStartIndex;
	
	// テクスチャ配列用SRVの次のインデックス（将来用）
	uint32_t textureArrayIndex_ = kTextureArrayStartIndex;
	
	// ストラクチャードバッファ用SRVの次のインデックス
	uint32_t structuredBufferIndex_ = kStructuredBufferStartIndex;

	// 互換性のため残す
	uint32_t textureIndex_ = kTexture2DStartIndex;

	// SRV用のディスクリプタサイズ
	uint32_t descriptorSize_ = 0;
	// SRV用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	// フリーリスト（返却されたインデックスの再利用用）
	std::queue<uint32_t> freeTexture2DIndices_;
	std::queue<uint32_t> freeTextureCubeIndices_;
	std::queue<uint32_t> freeTextureArrayIndices_;
	std::queue<uint32_t> freeStructuredBufferIndices_;

	// 使用中のインデックス管理（デバッグ用）
	std::vector<bool> usedTexture2DIndices_;
	std::vector<bool> usedTextureCubeIndices_;
	std::vector<bool> usedTextureArrayIndices_;
	std::vector<bool> usedStructuredBufferIndices_;
};