#include "IndexBuffer.h"

#include <cassert>
#include <iterator>
#include "../SubSystem/Console/Console.h"

IndexBuffer::IndexBuffer(
	const ComPtr<ID3D12Device>& device, const size_t size,
	const void* pInitData
) : size_(size) {
	device_ = device;

	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = size; // リソースのサイズ
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(buffer_.GetAddressOf())
	);

	assert(SUCCEEDED(hr));

	view_.BufferLocation = buffer_->GetGPUVirtualAddress();
	view_.Format = DXGI_FORMAT_R32_UINT;

	if (pInitData != nullptr) {
		void* ptr = nullptr;
		hr = buffer_->Map(0, nullptr, &ptr);
		assert(SUCCEEDED(hr));

		memcpy(ptr, pInitData, size);
		buffer_->Unmap(0, nullptr);
	}
}

D3D12_INDEX_BUFFER_VIEW IndexBuffer::View() {
	view_.SizeInBytes = static_cast<UINT>(size_); // すでにバイト数になっている
	view_.BufferLocation = buffer_->GetGPUVirtualAddress();
	return view_;
}

//-----------------------------------------------------------------------------
// Purpose: インデックスバッファの更新.
//-----------------------------------------------------------------------------
void IndexBuffer::Update(const void* pInitData, const size_t size) const {
	assert(pInitData != nullptr);
	assert(size <= size_);

	void* ptr = nullptr;
	[[maybe_unused]] HRESULT hr = buffer_->Map(0, nullptr, &ptr);
	assert(SUCCEEDED(hr));

	memcpy(ptr, pInitData, size);
	buffer_->Unmap(0, nullptr);
}

size_t IndexBuffer::GetSize() const {
	return size_;
}

std::vector<uint32_t>& IndexBuffer::GetIndices() const {
	indices_.resize(size_ / sizeof(uint32_t));
	void* ptr = nullptr;
	[[maybe_unused]] HRESULT hr = buffer_->Map(0, nullptr, &ptr);
	assert(SUCCEEDED(hr));

	memcpy(indices_.data(), ptr, size_);
	buffer_->Unmap(0, nullptr);

	return indices_;
}
