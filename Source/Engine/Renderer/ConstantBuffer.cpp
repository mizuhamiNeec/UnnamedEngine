#include "ConstantBuffer.h"

#include <cassert>

ConstantBuffer::ConstantBuffer(const ComPtr<ID3D12Device>& device, const size_t size) {
	size_t align = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	UINT64 sizeAligned = (size + (align - 1)) & ~(align - 1);

	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	// バッファリソース。テクスチャの場合はまた別の設定をする
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeAligned; // リソースのサイズ
	// バッファの場合はこれらは1にする決まり
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(buffer_.GetAddressOf())
	);

	assert(SUCCEEDED(hr));

	hr = buffer_->Map(0, nullptr, &mappedPtr);
	assert(SUCCEEDED(hr));

	desc_ = {};
	desc_.BufferLocation = buffer_->GetGPUVirtualAddress();
	desc_.SizeInBytes = static_cast<UINT>(sizeAligned);
}

D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer::GetAddress() const {
	return desc_.BufferLocation;
}

D3D12_CONSTANT_BUFFER_VIEW_DESC ConstantBuffer::ViewDesc() const {
	return desc_;
}

void* ConstantBuffer::GetPtr() const {
	return mappedPtr;
}
