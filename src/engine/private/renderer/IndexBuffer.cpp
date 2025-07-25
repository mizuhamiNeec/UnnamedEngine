#include <cassert>
#include <iterator>

#include <engine/public/renderer/IndexBuffer.h>

IndexBuffer::IndexBuffer(
	const Microsoft::WRL::ComPtr<ID3D12Device>& device, const size_t size,
	const void*                                 pInitData
) : mSize(size) {
	mDevice = device;

	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width               = size; // リソースのサイズ
	resourceDesc.Height              = 1;
	resourceDesc.DepthOrArraySize    = 1;
	resourceDesc.MipLevels           = 1;
	resourceDesc.SampleDesc.Count    = 1;
	resourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mBuffer.GetAddressOf())
	);

	assert(SUCCEEDED(hr));

	mView.BufferLocation = mBuffer->GetGPUVirtualAddress();
	mView.Format         = DXGI_FORMAT_R32_UINT;

	if (pInitData != nullptr) {
		void* ptr = nullptr;
		hr        = mBuffer->Map(0, nullptr, &ptr);
		assert(SUCCEEDED(hr));

		memcpy(ptr, pInitData, size);
		mBuffer->Unmap(0, nullptr);
	}

	mBuffer->SetName(L"IndexBuffer");
}

D3D12_INDEX_BUFFER_VIEW IndexBuffer::View() {
	mView.SizeInBytes    = static_cast<UINT>(mSize); // すでにバイト数になっている
	mView.BufferLocation = mBuffer->GetGPUVirtualAddress();
	return mView;
}

//-----------------------------------------------------------------------------
// Purpose: インデックスバッファの更新.
//-----------------------------------------------------------------------------
void IndexBuffer::Update(const void* pInitData, const size_t size) const {
	assert(pInitData != nullptr);
	assert(size <= mSize);

	void*                    ptr = nullptr;
	[[maybe_unused]] HRESULT hr  = mBuffer->Map(0, nullptr, &ptr);
	assert(SUCCEEDED(hr));

	memcpy(ptr, pInitData, size);
	mBuffer->Unmap(0, nullptr);
}

size_t IndexBuffer::GetSize() const {
	return mSize;
}

std::vector<uint32_t>& IndexBuffer::GetIndices() const {
	mIndices.resize(mSize / sizeof(uint32_t));
	void*                    ptr = nullptr;
	[[maybe_unused]] HRESULT hr  = mBuffer->Map(0, nullptr, &ptr);
	assert(SUCCEEDED(hr));

	memcpy(mIndices.data(), ptr, mSize);
	mBuffer->Unmap(0, nullptr);

	return mIndices;
}
