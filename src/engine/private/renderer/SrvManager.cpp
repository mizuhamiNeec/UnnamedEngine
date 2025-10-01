#include <engine/public/renderer/SrvManager.h>

#include <runtime/core/Properties.h>

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

	// 使用状況管理配列の初期化
	usedTexture2DIndices_.resize(kTexture2DEndIndex - kTexture2DStartIndex,
	                             false);
	usedTextureCubeIndices_.resize(
		kTextureCubeEndIndex - kTextureCubeStartIndex, false);
	usedTextureArrayIndices_.resize(
		kTextureArrayEndIndex - kTextureArrayStartIndex, false);
	usedStructuredBufferIndices_.resize(
		kStructuredBufferEndIndex - kStructuredBufferStartIndex, false);

	Console::Print(
		"[SrvManager] フリーリスト方式でSRV管理を初期化しました\n",
		kConTextColorCompleted,
		Channel::RenderSystem
	);
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

	// 上で記録した番号をreturn
	return index;
}

uint32_t SrvManager::AllocateForTexture2D() {
	uint32_t index;

	// フリーリストに返却されたインデックスがあるかチェック
	if (!freeTexture2DIndices_.empty()) {
		// 再利用可能なインデックスを取得
		index = freeTexture2DIndices_.front();
		freeTexture2DIndices_.pop();
	} else {
		// 2Dテクスチャ用インデックス範囲の上限チェック
		assert(texture2DIndex_ < kTexture2DEndIndex);

		// 新しいインデックスを割り当て
		index = texture2DIndex_;
		texture2DIndex_++;
	}

	// 使用状況を記録
	size_t arrayIndex = index - kTexture2DStartIndex;
	if (arrayIndex < usedTexture2DIndices_.size()) {
		usedTexture2DIndices_[arrayIndex] = true;
	}

	return index;
}

uint32_t SrvManager::AllocateForTextureCube() {
	uint32_t index;

	// フリーリストに返却されたインデックスがあるかチェック
	if (!freeTextureCubeIndices_.empty()) {
		// 再利用可能なインデックスを取得
		index = freeTextureCubeIndices_.front();
		freeTextureCubeIndices_.pop();
	} else {
		// キューブマップテクスチャ用インデックス範囲の上限チェック
		assert(textureCubeIndex_ < kTextureCubeEndIndex);

		// 新しいインデックスを割り当て
		index = textureCubeIndex_;
		textureCubeIndex_++;
	}

	// 使用状況を記録
	size_t arrayIndex = index - kTextureCubeStartIndex;
	if (arrayIndex < usedTextureCubeIndices_.size()) {
		usedTextureCubeIndices_[arrayIndex] = true;
	}

	return index;
}

uint32_t SrvManager::AllocateForTextureArray() {
	// テクスチャ配列用インデックス範囲の上限チェック
	assert(textureArrayIndex_ < kTextureArrayEndIndex);

	// return する番号を一旦記録しておく
	const uint32_t index = textureArrayIndex_;
	// 次回のために番号を1進める
	textureArrayIndex_++;

	// デバッグログ：テクスチャ配列用SRVインデックス割り当て状況を出力
	Console::Print(
		std::format(
			"[SrvManager] AllocateForTextureArray - Allocated texture array SRV index: {}, Next textureArrayIndex_: {}\n",
			index, textureArrayIndex_),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	// 上で記録した番号をreturn
	return index;
}

uint32_t SrvManager::AllocateForStructuredBuffer() {
	uint32_t index;

	// フリーリストに返却されたインデックスがあるかチェック
	if (!freeStructuredBufferIndices_.empty()) {
		// 再利用可能なインデックスを取得
		index = freeStructuredBufferIndices_.front();
		freeStructuredBufferIndices_.pop();

		Console::Print(
			std::format(
				"[SrvManager] AllocateForStructuredBuffer - Reused structured buffer SRV index: {} (from free list)\n",
				index),
			kConTextColorCompleted,
			Channel::RenderSystem
		);
	} else {
		// ストラクチャードバッファ用インデックス範囲の上限チェック
		assert(structuredBufferIndex_ < kStructuredBufferEndIndex);

		// 新しいインデックスを割り当て
		index = structuredBufferIndex_;
		structuredBufferIndex_++;

		Console::Print(
			std::format(
				"[SrvManager] AllocateForStructuredBuffer - Allocated new structured buffer SRV index: {}, Next structuredBufferIndex_: {}\n",
				index, structuredBufferIndex_),
			kConTextColorCompleted,
			Channel::RenderSystem
		);
	}

	// 使用状況を記録
	size_t arrayIndex = index - kStructuredBufferStartIndex;
	if (arrayIndex < usedStructuredBufferIndices_.size()) {
		usedStructuredBufferIndices_[arrayIndex] = true;
	}

	return index;
}

uint32_t SrvManager::AllocateConsecutiveTexture2DSlots(uint32_t count) {
	// 連続した2Dテクスチャスロットが確保できるかチェック
	if (texture2DIndex_ + count > kTexture2DEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] 警告: 連続した{}個の2Dテクスチャスロットを確保できません（現在のインデックス: {}）\n"
				"           代替として最初から再割り当てします\n",
				count, texture2DIndex_),
			kConTextColorWarning,
			Channel::RenderSystem
		);

		// 代替案：最初から再割り当て（デバッグ用の一時的な対処）
		// 本来はフリーリストから連続スロットを探すか、より適切な管理が必要
		uint32_t startIndex = kTexture2DStartIndex;

		// 使用可能な連続スロットがあるかチェック
		if (startIndex + count <= kTexture2DEndIndex) {
			return startIndex;
		} else {
			Console::Print(
				std::format(
					"[SrvManager] エラー: 連続した{}個のスロットは利用できません\n",
					count),
				kConTextColorError,
				Channel::RenderSystem
			);
			return 0; // エラー値
		}
	}

	// 開始インデックスを記録
	uint32_t startIndex = texture2DIndex_;
	// インデックスを進める
	texture2DIndex_ += count;

	return startIndex;
}

uint32_t SrvManager::AllocateConsecutiveTextureCubeSlots(uint32_t count) {
	// 連続したキューブマップテクスチャスロットが確保できるかチェック
	if (textureCubeIndex_ + count > kTextureCubeEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 連続した{}個のキューブマップテクスチャスロットを確保できません（現在のインデックス: {}）\n",
				count, textureCubeIndex_),
			kConTextColorError,
			Channel::RenderSystem
		);
		return 0; // エラー値
	}

	// 開始インデックスを記録
	uint32_t startIndex = textureCubeIndex_;
	// インデックスを進める
	textureCubeIndex_ += count;

	Console::Print(
		std::format("[SrvManager] 連続した{}個のキューブマップテクスチャスロットを確保: {}-{}\n",
		            count, startIndex, startIndex + count - 1),
		kConTextColorCompleted,
		Channel::RenderSystem
	);

	return startIndex;
}

// 互換性のためのメソッド - AllocateForTexture2Dにリダイレクト
uint32_t SrvManager::AllocateForTexture() {
	return AllocateForTexture2D();
}

// 互換性のためのメソッド - AllocateConsecutiveTexture2DSlotsにリダイレクト  
uint32_t SrvManager::AllocateConsecutiveTextureSlots(uint32_t count) {
	return AllocateConsecutiveTexture2DSlots(count);
}

//-----------------------------------------------------------------------------
// インデックス返却メソッド（フリーリスト方式）
//-----------------------------------------------------------------------------
void SrvManager::DeallocateTexture2D(uint32_t index) {
	// 範囲チェック
	if (index < kTexture2DStartIndex || index >= kTexture2DEndIndex) {
		Console::Print(
			std::format("[SrvManager] エラー: 無効な2Dテクスチャインデックス {}を返却しようとしました\n",
			            index),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kTexture2DStartIndex;
	if (arrayIndex < usedTexture2DIndices_.size()) {
		if (!usedTexture2DIndices_[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用の2Dテクスチャインデックス {}を返却しようとしました\n",
					index),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		usedTexture2DIndices_[arrayIndex] = false;
	}

	// フリーリストに追加
	freeTexture2DIndices_.push(index);

	Console::Print(
		std::format("[SrvManager] 2Dテクスチャインデックス {}を返却しました（フリーリストサイズ: {}）\n",
		            index, freeTexture2DIndices_.size()),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

void SrvManager::DeallocateTextureCube(uint32_t index) {
	// 範囲チェック
	if (index < kTextureCubeStartIndex || index >= kTextureCubeEndIndex) {
		Console::Print(
			std::format("[SrvManager] エラー: 無効なキューブマップインデックス {}を返却しようとしました\n",
			            index),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kTextureCubeStartIndex;
	if (arrayIndex < usedTextureCubeIndices_.size()) {
		if (!usedTextureCubeIndices_[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用のキューブマップインデックス {}を返却しようとしました\n",
					index),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		usedTextureCubeIndices_[arrayIndex] = false;
	}

	// フリーリストに追加
	freeTextureCubeIndices_.push(index);

	Console::Print(
		std::format("[SrvManager] キューブマップインデックス {}を返却しました（フリーリストサイズ: {}）\n",
		            index, freeTextureCubeIndices_.size()),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

void SrvManager::DeallocateStructuredBuffer(uint32_t index) {
	// 範囲チェック
	if (index < kStructuredBufferStartIndex || index >=
		kStructuredBufferEndIndex) {
		Console::Print(
			std::format(
				"[SrvManager] エラー: 無効なストラクチャードバッファインデックス {}を返却しようとしました\n",
				index),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kStructuredBufferStartIndex;
	if (arrayIndex < usedStructuredBufferIndices_.size()) {
		if (!usedStructuredBufferIndices_[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用のストラクチャードバッファインデックス {}を返却しようとしました\n",
					index),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		usedStructuredBufferIndices_[arrayIndex] = false;
	}

	// フリーリストに追加
	freeStructuredBufferIndices_.push(index);

	Console::Print(
		std::format(
			"[SrvManager] ストラクチャードバッファインデックス {}を返却しました（フリーリストサイズ: {}）\n",
			index, freeStructuredBufferIndices_.size()),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
}

// 互換性のためのメソッド
void SrvManager::DeallocateTexture(uint32_t index) {
	DeallocateTexture2D(index);
}

void SrvManager::DeallocateTextureArray(uint32_t index) {
	// 範囲チェック
	if (index < kTextureArrayStartIndex || index >= kTextureArrayEndIndex) {
		Console::Print(
			std::format("[SrvManager] エラー: 無効なテクスチャ配列インデックス {}を返却しようとしました\n",
			            index),
			kConTextColorError,
			Channel::RenderSystem
		);
		return;
	}

	// 使用状況をクリア
	size_t arrayIndex = index - kTextureArrayStartIndex;
	if (arrayIndex < usedTextureArrayIndices_.size()) {
		if (!usedTextureArrayIndices_[arrayIndex]) {
			Console::Print(
				std::format(
					"[SrvManager] 警告: 未使用のテクスチャ配列インデックス {}を返却しようとしました\n",
					index),
				kConTextColorWarning,
				Channel::RenderSystem
			);
			return;
		}
		usedTextureArrayIndices_[arrayIndex] = false;
	}

	// フリーリストに追加
	freeTextureArrayIndices_.push(index);

	Console::Print(
		std::format("[SrvManager] テクスチャ配列インデックス {}を返却しました（フリーリストサイズ: {}）\n",
		            index, freeTextureArrayIndices_.size()),
		kConTextColorCompleted,
		Channel::RenderSystem
	);
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

uint32_t SrvManager::GetIndexFromCPUHandle(
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle
) const {
	const uint32_t index = static_cast<uint32_t>(
		(
			cpuHandle.ptr - descriptorHeap_->
			                GetCPUDescriptorHandleForHeapStart().
			                ptr
		) / descriptorSize_
	);
	return index;
}

uint32_t SrvManager::GetIndexFromGPUHandle(
	const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle
) const {
	const uint32_t index = static_cast<uint32_t>(
		(
			gpuHandle.ptr - descriptorHeap_->
			                GetGPUDescriptorHandleForHeapStart().
			                ptr
		) / descriptorSize_
	);
	return index;
}

bool SrvManager::CanAllocate() const {
	return useIndex_ < kMaxSrvCount;
}
