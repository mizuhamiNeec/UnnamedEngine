
#include <engine/public/renderer/ConstantBuffer.h>

#include <cassert>
#include <string>

ConstantBuffer::ConstantBuffer(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const size_t size, std::string name): mName(
	std::move(name)
) {
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
		IID_PPV_ARGS(mBuffer.GetAddressOf())
	);

	assert(SUCCEEDED(hr));

	hr = mBuffer->Map(0, nullptr, &mAppedPtr);
	assert(SUCCEEDED(hr));

	mBuffer->SetName(std::wstring(name.begin(), name.end()).c_str());

	mDesc.BufferLocation = mBuffer->GetGPUVirtualAddress();
	mDesc.SizeInBytes = static_cast<UINT>(sizeAligned);
}

ConstantBuffer::~ConstantBuffer() {
	if (mBuffer) {
		mBuffer->Unmap(0, nullptr);
	}
}

D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer::GetAddress() const {
	return mDesc.BufferLocation;
}

D3D12_CONSTANT_BUFFER_VIEW_DESC ConstantBuffer::ViewDesc() const {
	return mDesc;
}

void* ConstantBuffer::GetPtr() const {
	return mAppedPtr;
}

ID3D12Resource* ConstantBuffer::GetResource() const {
	return mBuffer.Get();
}
