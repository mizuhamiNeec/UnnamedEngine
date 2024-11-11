#include "IndexBuffer.h"

#include <cassert>
#include <iterator>
#include "../Lib/Console/Console.h"

IndexBuffer::IndexBuffer(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const size_t size, const void* pInitData) {
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
	view_.SizeInBytes = sizeof(uint32_t) * 6;
	view_.Format = DXGI_FORMAT_R32_UINT;

	if (pInitData != nullptr) {
		void* ptr = nullptr;
		hr = buffer_->Map(0, nullptr, &ptr);
		assert(SUCCEEDED(hr));

		memcpy(ptr, pInitData, size);
		buffer_->Unmap(0, nullptr);
	}
}

D3D12_INDEX_BUFFER_VIEW IndexBuffer::View() const {
	return view_;
}

void IndexBuffer::Update(const void* pInitData, const size_t size) const {
	if (pInitData != nullptr) {
		void* ptr = nullptr;
		HRESULT hr = buffer_->Map(0, nullptr, &ptr);
		if (FAILED(hr)) {
			Console::Print("Failed to map index buffer\n", { 1.0f, 0.0f, 0.0f, 1.0f });
			return;
		}

		memcpy(ptr, pInitData, size);
		buffer_->Unmap(0, nullptr);
	}
}
