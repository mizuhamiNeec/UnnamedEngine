#include "SrvManager.h"

#include <cassert>

#include "Lib/Utils/ClientProperties.h"

#include "Renderer/D3D12.h"

void SrvManager::Init(D3D12* d3d12) {
	// 引数で受け取ってメンバ変数に記録する
	d3d12_ = d3d12;

	// ディスクリプタヒープの生成
	// SRV用のヒープでディスクリプタの数はkMaxSRVCount。SRVはShader内で触るものなので、ShaderVisibleはtrue
	descriptorHeap_ = d3d12_->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSrvCount, true);
	// ディスクリプタ1個分のサイズを取得して記録
	descriptorSize_ = d3d12_->GetDevice()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void SrvManager::PreDraw() const {
	// 描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = {descriptorHeap_.Get()};
	d3d12_->GetCommandList()->SetDescriptorHeaps(
		_countof(descriptorHeaps), descriptorHeaps);
}

//-----------------------------------------------------------------------------
// Purpose: SRV生成(テクスチャ用)
//-----------------------------------------------------------------------------
void SrvManager::CreateSRVForTexture2D(uint32_t srvIndex,
                                       ID3D12Resource* pResource,
                                       DXGI_FORMAT format,
                                       UINT mipLevels) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mipLevels;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// SRVを作成
	d3d12_->GetDevice()->CreateShaderResourceView(
		pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

//-----------------------------------------------------------------------------
// Purpose: SRV生成(Structured Buffer用)
//-----------------------------------------------------------------------------
void SrvManager::CreateSRVForStructuredBuffer(uint32_t srvIndex,
                                              ID3D12Resource* pResource,
                                              UINT numElements,
                                              UINT structureByteStride) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;

	// バッファのサイズを取得
	D3D12_RESOURCE_DESC resourceDesc = pResource->GetDesc();
	UINT64              bufferSize   = resourceDesc.Width;

	// numElements をバッファサイズに基づいて制限
	UINT maxElements = static_cast<UINT>(bufferSize / structureByteStride);
	numElements      = (std::min)(numElements, maxElements);

	srvDesc.Buffer.NumElements         = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Format                     = DXGI_FORMAT_UNKNOWN;

	// SRVを作成
	d3d12_->GetDevice()->CreateShaderResourceView(
		pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SrvManager::CreateSRVForTextureCube(
	uint32_t                               index,
	Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	DXGI_FORMAT                            format,
	UINT                                   mipLevels
) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MipLevels = mipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	d3d12_->GetDevice()->CreateShaderResourceView(
		resource.Get(),
		&srvDesc,
		GetCPUDescriptorHandle(index)
	);
}

void SrvManager::SetGraphicsRootDescriptorTable(const UINT rootParameterIndex,
                                                const uint32_t srvIndex) const {
	d3d12_->GetCommandList()->SetGraphicsRootDescriptorTable(
		rootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

uint32_t SrvManager::Allocate() {
	// 上限に達していないかチェックしてassert
	assert(useIndex_ < kMaxSrvCount);

	// return する番号を一旦記録しておく
	const int index = useIndex_;
	// 次回のために番号を1進める
	useIndex_++;
	// 上で記録した番号をreturn
	return index;
}

ID3D12DescriptorHeap* SrvManager::GetDescriptorHeap() const {
	return descriptorHeap_.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(
	const uint32_t index) const {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->
		GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize_ * index;
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(
	const uint32_t index) const {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->
		GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize_ * index;
	return handleGPU;
}

bool SrvManager::CanAllocate() const {
	return useIndex_ < kMaxSrvCount;
}
