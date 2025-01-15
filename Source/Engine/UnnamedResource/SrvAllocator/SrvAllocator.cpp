#include "SrvAllocator.h"

#include "Lib/Console/Console.h"
#include "Lib/Utils/ClientProperties.h"

//-----------------------------------------------------------------------------
// Purpose: コンストラクタ
// - device: デバイス
// - descriptorCount: ディスクリプタの数
//-----------------------------------------------------------------------------
SrvAllocator::SrvAllocator(ID3D12Device* device, const UINT descriptorCount) : capacity_(descriptorCount), currentIndex_(kSrvIndexTop) {
	// ディスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = descriptorCount;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // GPUで使用するためShaderVisibleを指定
	heapDesc.NodeMask = 0;

	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
	if (FAILED(hr)) {
		Console::Print("ディスクリプタヒープの作成に失敗しました。\n", kConsoleColorError, Channel::ResourceManager);
		throw std::runtime_error("ディスクリプタヒープの作成に失敗しました。");
	}

	// ヒープの開始アドレスを取得
	heapStart_ = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	descriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

//-----------------------------------------------------------------------------
// Purpose: SRVを割り当てます
// Return : CPUディスクリプタハンドル
//-----------------------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE SrvAllocator::Allocate() {
	std::lock_guard<std::mutex> lock(mutex_);

	if (currentIndex_ >= capacity_) {
		Console::Print("SRVの割り当てに失敗しました。\n", kConsoleColorError, Channel::ResourceManager);
		throw std::runtime_error("SRVの割り当てに失敗しました。");
	}

	// 現在のインデックスを取得
	D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart_;
	handle.ptr += currentIndex_ * descriptorSize_;

	++currentIndex_;
	return handle;
}

//-----------------------------------------------------------------------------
// Purpose: GPUディスクリプタハンドルを取得します
// - cpuHandle: CPUディスクリプタハンドル
// Return : GPUディスクリプタハンドル
//-----------------------------------------------------------------------------
D3D12_GPU_DESCRIPTOR_HANDLE SrvAllocator::GetGPUHandle(const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const {
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	gpuHandle.ptr += cpuHandle.ptr - heapStart_.ptr;
	return gpuHandle;
}

//-----------------------------------------------------------------------------
// Purpose: ヒープの開始アドレスを取得します
// Return : CPUディスクリプタハンドル
//-----------------------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE SrvAllocator::GetHeapStart() const {
	return heapStart_;
}

