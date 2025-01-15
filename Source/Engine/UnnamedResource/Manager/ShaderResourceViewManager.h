#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <Lib/Utils/ClientProperties.h>

using Microsoft::WRL::ComPtr;

class ShaderResourceViewManager {
public:
	ShaderResourceViewManager(ID3D12Device* device);
	~ShaderResourceViewManager() = default;

	void Init();

	D3D12_GPU_DESCRIPTOR_HANDLE RegisterShaderResourceView(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);

	ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() const {
		return descriptorHeap_;
	}

private:
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	UINT descriptorSize_;
	UINT currentDescriptorIndex_ = 0;

	ID3D12Device* device_;

	std::unordered_map<ID3D12Resource*, D3D12_GPU_DESCRIPTOR_HANDLE> srvCache_;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(UINT index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(UINT index) const;
};
