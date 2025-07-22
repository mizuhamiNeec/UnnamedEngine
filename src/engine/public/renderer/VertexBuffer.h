#pragma once

#include <cassert>
#include <d3d12.h>
#include <wrl.h>
#include <vector>

template <typename VertexType>
class VertexBuffer {
public:
	VertexBuffer(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t size,
	             const VertexType* pInitData);
	[[nodiscard]] D3D12_VERTEX_BUFFER_VIEW View() const;
	void Update(const VertexType* pInitData, size_t size);
	[[nodiscard]] size_t GetSize() const;
	std::vector<VertexType> GetVertices() const;

private:
	Microsoft::WRL::ComPtr<ID3D12Device>     mDevice;
	Microsoft::WRL::ComPtr<ID3D12Resource>   mBuffer;
	D3D12_VERTEX_BUFFER_VIEW mView;
	size_t                   mSize = 0;
};

template <typename VertexType>
VertexBuffer<VertexType>::VertexBuffer(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
                                       const size_t                size,
                                       const VertexType*           pInitData) :
	mDevice(device),
	mSize(size) {
	// リソース用のヒープを設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使用

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = size; // リソースのサイズ
	resourceDesc.Height = 1; // バッファの場合は1
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // バッファの場合はこれ

	// リソースを作成
	HRESULT hr = mDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mBuffer.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	// ビューを設定
	mView.BufferLocation = mBuffer->GetGPUVirtualAddress();
	mView.SizeInBytes    = static_cast<UINT>(mSize);
	mView.StrideInBytes  = static_cast<UINT>(sizeof(VertexType));

	// 初期データがある場合はコピー
	if (pInitData != nullptr) {
		void* ptr = nullptr;
		hr        = mBuffer->Map(0, nullptr, &ptr);
		assert(SUCCEEDED(hr));

		memcpy(ptr, pInitData, mSize);
		mBuffer->Unmap(0, nullptr);
	}

	mBuffer->SetName(L"VertexBuffer");
}

template <typename VertexType>
D3D12_VERTEX_BUFFER_VIEW VertexBuffer<VertexType>::View() const {
	return mView;
}

template <typename VertexType>
void VertexBuffer<
	VertexType>::Update(const VertexType* pInitData, size_t size) {
	void*       mappedData = nullptr;
	D3D12_RANGE range{0, 0};

	// マップしてGPUメモリにデータをコピー
	HRESULT hr = mBuffer->Map(0, &range, &mappedData);
	if (SUCCEEDED(hr)) {
		memcpy(mappedData, pInitData, size);
		mBuffer->Unmap(0, nullptr);
	}
}

template <typename VertexType>
size_t VertexBuffer<VertexType>::GetSize() const {
	return mSize;
}

template <typename VertexType>
std::vector<VertexType> VertexBuffer<VertexType>::GetVertices() const {
	std::vector<VertexType> vertices(mSize / sizeof(VertexType));
	void*                   mappedData = nullptr;
	D3D12_RANGE             range{0, 0};

	[[maybe_unused]] HRESULT hr = mBuffer->Map(0, &range, &mappedData);
	assert(SUCCEEDED(hr));

	memcpy(vertices.data(), mappedData, mSize);
	mBuffer->Unmap(0, nullptr);

	return vertices;
}
