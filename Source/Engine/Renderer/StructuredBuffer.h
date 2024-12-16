#pragma once
#include <cstdint>

#include "D3D12.h"

class StructuredBuffer {
public:
	StructuredBuffer(const D3D12* d3d12, const uint32_t elementSize, const uint32_t numElements) :
		elementSize_(elementSize), numElements_(numElements) {
		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = elementSize_ * numElements_;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heapProperties = {};

		d3d12->GetDevice()->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&bufferResource_));

		bufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_));
	}

	~StructuredBuffer() {
		if (bufferResource_) {
			bufferResource_->Unmap(0, nullptr);
		}
	}

	ID3D12Resource* GetResource() const { return bufferResource_.Get(); }
	void* GetMappedData() const { return mappedData_; }

private:
	ComPtr<ID3D12Resource> bufferResource_;
	void* mappedData_ = nullptr;
	uint32_t elementSize_ = 0;
	uint32_t numElements_ = 0;
};
