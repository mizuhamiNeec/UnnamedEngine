#include "ShaderResourceViewManager.h"

#include "Lib/Console/Console.h"

ShaderResourceViewManager::ShaderResourceViewManager(
	ID3D12Device* device
) : device_(device) {
	// ディスクリプタヒープを作成
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = kMaxSrvCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	HRESULT hr = device_->CreateDescriptorHeap(
		&desc, IID_PPV_ARGS(&descriptorHeap_)
	);
	if (FAILED(hr)) {
		Console::Print(
			"Failed to create descriptor heap\n", kConsoleColorError,
			Channel::ResourceSystem
		);
	}
}

void ShaderResourceViewManager::Init() {
	descriptorSize_ = device_->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	currentDescriptorIndex_ = kSrvIndexTop;
}

D3D12_GPU_DESCRIPTOR_HANDLE
ShaderResourceViewManager::RegisterShaderResourceView(
	ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
	// 既存のSRVがあればそれを返す
	auto it = srvCache_.find(resource);
	if (it != srvCache_.end()) {
		return it->second;
	}

	// 上限を超えていたらエラー
	if (currentDescriptorIndex_ >= descriptorHeap_->GetDesc().NumDescriptors) {
		Console::Print(
			"ディスクリプタの上限を超えました\n", kConsoleColorError, Channel::ResourceSystem
		);
		return {};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = GetCPUDescriptorHandle(
		currentDescriptorIndex_
	);
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = GetGPUDescriptorHandle(
		currentDescriptorIndex_
	);

	device_->CreateShaderResourceView(resource, &srvDesc, handleCPU);

	// キャッシュに登録
	srvCache_[resource] = handleGPU;

	// ディスクリプタのインデックスを進める
	++currentDescriptorIndex_;

	return handleGPU;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderResourceViewManager::GetCPUDescriptorHandle(
	const UINT index
) const {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->
		GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize_ * index;
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE ShaderResourceViewManager::GetGPUDescriptorHandle(
	const UINT index
) const {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->
		GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize_ * index;
	return handleGPU;
}
