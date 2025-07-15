#pragma once

#include <d3d12.h>
#include <vector>
#include <wrl.h>

struct RootSignatureDesc;

class RootSignature2 {
public:
	RootSignature2() = default;

	void AddConstantBuffer(UINT                    shaderRegister,
	                       D3D12_SHADER_VISIBILITY visibility,
	                       UINT                    registerSpace = 0);
	void AddShaderResourceView(UINT shaderRegister, UINT registerSpace = 0);
	void AddUnorderedAccessView(UINT shaderRegister, UINT registerSpace = 0);
	void AddDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges,
	                        UINT                          numRanges);
	void AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& samplerDesc);
	void AddRootParameter(const D3D12_ROOT_PARAMETER1& param1);

	void Init(ID3D12Device* device, const RootSignatureDesc& desc);
	void Build(ID3D12Device* device);
	ID3D12RootSignature* Get() const;
	bool HasStaticSampler() const;

	void Release();

private:
	std::vector<D3D12_ROOT_PARAMETER>                mRootParameters;
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> mDescriptorTableRanges;
	std::vector<D3D12_STATIC_SAMPLER_DESC>           mStaticSamplers;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>      mRootSignature;
	D3D12_ROOT_SIGNATURE_DESC                        mRootSignatureDesc;
};
