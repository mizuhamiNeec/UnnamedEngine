#include "ShaderResourceViewManager.h"

#include "Lib/Console/Console.h"
#include "Lib/Utils/StrUtils.h"

ShaderResourceViewManager::ShaderResourceViewManager(
	ID3D12Device* device
) : descriptorSize_(0),
device_(device) {
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

ShaderResourceViewManager::~ShaderResourceViewManager() {
	Shutdown();
}

void ShaderResourceViewManager::Init() {
	Console::Print(
		"SRV Manager を初期化しています...\n",
		kConsoleColorGray, Channel::ResourceSystem
	);

	descriptorSize_ = device_->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	// ImGuiで使ったあとに初期化
	currentDescriptorIndex_ = kSrvIndexTop;
}

void ShaderResourceViewManager::Shutdown() {
	Console::Print("SRV Manager を終了しています...\n", kConsoleColorWait, Channel::ResourceSystem);

	// キャッシュされているリソースの参照を解放
	for (auto& entry : srvCache_) {
		auto& resource = const_cast<ID3D12Resource*&>(entry.first);
		// リソースの参照のみを解放（リソース自体の解放は所有者に任せる）
		resource = nullptr;
	}
	srvCache_.clear();

	// ディスクリプタヒープを解放
	if (descriptorHeap_) {
		descriptorHeap_.Reset();
	}

	// デバイス参照をクリア（所有権は持っていないので単なるポインタのクリア）
	device_ = nullptr;

	// インデックスをリセット
	currentDescriptorIndex_ = 0;
	descriptorSize_ = 0;
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

	resource->SetName(StrUtils::ToWString("SRV: " + std::to_string(currentDescriptorIndex_)).c_str());

	return handleGPU;
}

ComPtr<ID3D12DescriptorHeap> ShaderResourceViewManager::GetDescriptorHeap() {
	return descriptorHeap_;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderResourceViewManager::GetCPUDescriptorHandle(
	const UINT index
) const {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->
		GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize_ * static_cast<unsigned long long>(index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE ShaderResourceViewManager::GetGPUDescriptorHandle(
	const UINT index
) const {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->
		GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize_ * static_cast<unsigned long long>(index);
	return handleGPU;
}

ComPtr<ID3D12DescriptorHeap> ShaderResourceViewManager::descriptorHeap_;
