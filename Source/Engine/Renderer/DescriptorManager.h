#pragma once
#include <wrl/client.h>
#include <d3d12.h>

using namespace Microsoft::WRL;

class DescriptorHandle {
public:
	DescriptorHandle() = default;
	//DescriptorManager(ComPtr<ID3D12Device> device, const D3D12_DESCRIPTOR_HEAP_DESC& desc) : index(0), incrementSize(0)
	//{
	//	device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap))
	//}
};

class DescriptorManager {
};

