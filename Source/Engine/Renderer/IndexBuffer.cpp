#include "IndexBuffer.h"

#include <cassert>
#include <iterator>
#include "../Lib/Console/Console.h"

IndexBuffer::IndexBuffer(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const size_t size,
						 const void* pInitData) : size_(size) {
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
	view_.SizeInBytes = static_cast<UINT>(size_);  // すでにバイト数になっている
	view_.BufferLocation = buffer_->GetGPUVirtualAddress();
	return view_;
}

//-----------------------------------------------------------------------------
// Purpose: インデックスバッファの更新.
//-----------------------------------------------------------------------------
void IndexBuffer::Update(const void* pInitData, const size_t size) {
	// サイズを明示的にバイト数で計算
	size_ = size * sizeof(uint32_t);  // uint32_t のサイズを明示的に掛ける

	if (pInitData != nullptr) {
		void* ptr = nullptr;
		HRESULT hr = buffer_->Map(0, nullptr, &ptr);
		if (FAILED(hr)) {
			Console::Print("Failed to map index buffer\n", {1.0f, 0.0f, 0.0f, 1.0f});
			return;
		}
		memcpy(ptr, pInitData, size_);  // size_ (バイト数) を使用
		buffer_->Unmap(0, nullptr);
	}
}

size_t IndexBuffer::GetSize() const {
	return size_;
}
