#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <stdexcept>
#include <mutex>

using Microsoft::WRL::ComPtr;

class SrvAllocator {
public:
	SrvAllocator(ID3D12Device* device, UINT descriptorCount);
	~SrvAllocator() = default;

	D3D12_CPU_DESCRIPTOR_HANDLE Allocate();
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetHeapStart() const;

private:
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	UINT descriptorSize_;
	UINT capacity_;
	UINT currentIndex_;
	D3D12_CPU_DESCRIPTOR_HANDLE heapStart_;
	std::mutex mutex_;
};

