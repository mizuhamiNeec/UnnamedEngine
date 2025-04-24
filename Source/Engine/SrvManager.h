#pragma once
#include <cstdint>

#include <wrl/client.h>

#include "D3D12.h"

class D3D12;

class SrvManager {
public:
	void Init(D3D12* d3d12);
	void PreDraw() const;

	uint32_t Allocate();

	ID3D12DescriptorHeap* GetDescriptorHeap() const;
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels) const;
	void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements,
		UINT structureByteStride) const;
	void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex) const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(std::uint32_t index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

	bool CanAllocate() const;

private:
	D3D12* d3d12_ = nullptr;

	// 次に使用するSRVインデックス
	uint32_t useIndex_ = 0;

	// SRV用のディスクリプタサイズ
	uint32_t descriptorSize_ = 0;
	// SRV用ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
};