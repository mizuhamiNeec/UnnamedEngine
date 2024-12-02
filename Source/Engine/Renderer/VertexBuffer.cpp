#include "VertexBuffer.h"

#include <cassert>
#include "../Lib/Console/Console.h"
#include "../Lib/Structs/Structs.h"

VertexBuffer::VertexBuffer(const ComPtr<ID3D12Device>& device, const size_t size, const size_t stride,
                           const void* pInitData) : size_(size) {
	// リソース用のヒープを設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	D3D12_RESOURCE_DESC resourceDesc = {};

	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = size; // リソースのサイズ
	// バッファの場合はこれらは1にする決まり
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Alignment = 0; // デフォルトのアライメント

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
	view_.SizeInBytes = static_cast<UINT>(size);
	view_.StrideInBytes = static_cast<UINT>(stride);

	if (pInitData != nullptr) {
		void* ptr = nullptr;
		hr = buffer_->Map(0, nullptr, &ptr);
		assert(SUCCEEDED(hr));

		memcpy(ptr, pInitData, size);
		buffer_->Unmap(0, nullptr);
	}
}

D3D12_VERTEX_BUFFER_VIEW VertexBuffer::View() const {
	return view_;
}

void VertexBuffer::Update(const void* newVertices, const size_t vertexCount) {
	if (newVertices != nullptr) {
		void* ptr = nullptr;
		HRESULT hr = buffer_->Map(0, nullptr, &ptr);
		if (FAILED(hr) || ptr == nullptr) { // ptrがnullptrでないことを確認
			Console::Print("Failed to map vertex buffer or ptr is null\n", {1.0f, 0.0f, 0.0f, 1.0f});
			return; // エラー時は処理を中断
		}
		size_ = sizeof(Vertex) * vertexCount;
		
		if (size_ > 0) {
			memcpy(ptr, newVertices, size_);
		}
		buffer_->Unmap(0, nullptr);
	}
}