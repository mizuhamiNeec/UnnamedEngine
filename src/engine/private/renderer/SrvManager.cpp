#include <engine/public/renderer/SrvManager.h>

#include <engine/public/utils/properties.h>

#include <cassert>

#include "engine/public/OldConsole/Console.h"

void SrvManager::Init(D3D12* d3d12) {
	// 引数で受け取ってメンバ変数に記録する
	d3d12_ = d3d12;
	assert(d3d12_ != nullptr && "D3D12 is null in SrvManager::Init");

	// ディスクリプタヒープの生成
	// SRV用のヒープでディスクリプタの数はkMaxSRVCount。SRVはShader内で触るものなので、ShaderVisibleはtrue
	descriptorHeap_ = d3d12_->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSrvCount, true);

	// ディスクリプタヒープが正常に作成されたかチェック
	assert(
		descriptorHeap_ != nullptr &&
		"Failed to create descriptor heap in SrvManager::Init");

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
	// デバッグログ：ストラクチャードバッファSRV作成状況
	Console::Print(
		std::format(
			"[SrvManager] CreateSRVForStructuredBuffer - srvIndex: {}, numElements: {}, structureByteStride: {}\n",
			srvIndex, numElements, structureByteStride),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

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
	// インデックスが有効範囲内かチェック
	if (srvIndex >= kMaxSrvCount) {
		Console::Print(
			std::format("[SrvManager] エラー: インデックス {}が上限({})を超えています\n",
			            srvIndex, kMaxSrvCount),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE handle = GetGPUDescriptorHandle(srvIndex);
	d3d12_->GetCommandList()->SetGraphicsRootDescriptorTable(
		rootParameterIndex, handle);

	// フレーム単位で一定数の詳細ログのみ出力（大量のログを避けるため）
	static int logCountThisFrame = 0;
	static int maxLogsPerFrame   = 10;

	if (logCountThisFrame < maxLogsPerFrame) {
		// フレームあたり最大10件のログに制限
		// デバッグログ出力（テクスチャ問題調査用）
		Console::Print(
			std::format(
				"[SrvManager] SRVバインド実行: rootParameter={}, srvIndex={}, handle={}\n",
				rootParameterIndex, srvIndex, handle.ptr),
			kConTextColorCompleted,
			Channel::RenderSystem
		);
		logCountThisFrame++;
	}

	// 問題のテクスチャインデックスを監視 - これは常に出力
	if (srvIndex == 0) {
		Console::Print(
			std::format(
				"[SrvManager] 警告: インデックス0（デフォルトテクスチャ）がバインドされました - rootParameter={}\n",
				rootParameterIndex),
			kConTextColorWarning,
			Channel::RenderSystem
		);
	}
}

uint32_t SrvManager::Allocate() {
	// 上限に達していないかチェックしてassert
	assert(useIndex_ < kMaxSrvCount);

	// return する番号を一旦記録しておく
	const int index = useIndex_;
	// 次回のために番号を1進める
	useIndex_++;

	// デバッグログ：SRVインデックス割り当て状況を出力
	Console::Print(
		std::format(
			"[SrvManager] Allocate - Allocated SRV index: {}, Next useIndex_: {}\n",
			index, useIndex_),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	// 上で記録した番号をreturn
	return index;
}

uint32_t SrvManager::AllocateForTexture() {
	// テクスチャ用インデックス範囲の上限チェック
	assert(textureIndex_ < kTextureEndIndex);

	// return する番号を一旦記録しておく
	const uint32_t index = textureIndex_;
	// 次回のために番号を1進める
	textureIndex_++;

	// デバッグログ：テクスチャ用SRVインデックス割り当て状況を出力
	Console::Print(
		std::format(
			"[SrvManager] AllocateForTexture - Allocated texture SRV index: {}, Next textureIndex_: {}\n",
			index, textureIndex_),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	// 上で記録した番号をreturn
	return index;
}

uint32_t SrvManager::AllocateForStructuredBuffer() {
	// ストラクチャードバッファ用インデックス範囲の上限チェック
	assert(structuredBufferIndex_ < kStructuredBufferEndIndex);

	// return する番号を一旦記録しておく
	const uint32_t index = structuredBufferIndex_;
	// 次回のために番号を1進める
	structuredBufferIndex_++;

	// デバッグログ：ストラクチャードバッファ用SRVインデックス割り当て状況を出力
	Console::Print(
		std::format(
			"[SrvManager] AllocateForStructuredBuffer - Allocated structured buffer SRV index: {}, Next structuredBufferIndex_: {}\n",
			index, structuredBufferIndex_),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	// 上で記録した番号をreturn
	return index;
}

uint32_t SrvManager::AllocateConsecutiveTextureSlots(uint32_t count) {
	// 連続したテクスチャスロットが確保できるかチェック
	if (textureIndex_ + count > kTextureEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 連続した{}個のテクスチャスロットを確保できません（現在のインデックス: {}）\n",
				count, textureIndex_),
			kConTextColorError,
			Channel::RenderSystem
		);
		return 0; // エラー値
	}

	// 開始インデックスを記録
	uint32_t startIndex = textureIndex_;
	// インデックスを進める
	textureIndex_ += count;

	Console::Print(
		std::format("[SrvManager] 連続した{}個のテクスチャスロットを確保: {}-{}\n",
		            count, startIndex, startIndex + count - 1),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	return startIndex;
}

ID3D12DescriptorHeap* SrvManager::GetDescriptorHeap() const {
	assert(
		descriptorHeap_ != nullptr &&
		"Descriptor heap is null in SrvManager::GetDescriptorHeap");
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
