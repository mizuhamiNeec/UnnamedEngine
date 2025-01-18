#pragma once

#include <d3d12.h>
#include <vector>
#include <wrl.h>

struct RootSignatureDesc;
using Microsoft::WRL::ComPtr;

class RootSignature2 {
public:
	RootSignature2() = default;

	void AddConstantBuffer(UINT shaderRegister, UINT registerSpace = 0);
	void AddShaderResourceView(UINT shaderRegister, UINT registerSpace = 0);
	void AddUnorderedAccessView(UINT shaderRegister, UINT registerSpace = 0);
	void AddDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, UINT numRanges);
	void AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& samplerDesc);

	void Init(ID3D12Device* device, const RootSignatureDesc& desc);
	void Build(ID3D12Device* device);
	ID3D12RootSignature* Get() const;

private:
	std::vector<D3D12_ROOT_PARAMETER> rootParameters_;
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> descriptorTableRanges_;
	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers_;
	ComPtr<ID3D12RootSignature> rootSignature_;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc_;
};