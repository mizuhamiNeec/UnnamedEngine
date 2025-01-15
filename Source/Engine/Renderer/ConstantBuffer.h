#pragma once

#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class ConstantBuffer {
public:
	ConstantBuffer(const ComPtr<ID3D12Device>& device, size_t size);
	D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const;
	D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc() const;

	void* GetPtr() const;

	ID3D12Resource* GetResource() const;

	template <typename T>
	T* GetPtr() {
		return static_cast<T*>(GetPtr());
	}

private:
	ComPtr<ID3D12Resource> buffer_;
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc_;
	void* mappedPtr = nullptr;

	ConstantBuffer(const ConstantBuffer&) = delete;
	void operator=(const ConstantBuffer&) = delete;
};
