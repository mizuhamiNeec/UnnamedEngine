#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class IndexBuffer {
public:
	IndexBuffer(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t size, const void* pInitData);
	D3D12_INDEX_BUFFER_VIEW View();
	void Update(const void* pInitData, size_t size) const;
	size_t GetSize() const;

private:
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;
	D3D12_INDEX_BUFFER_VIEW view_;

	size_t size_ = 0;
};
