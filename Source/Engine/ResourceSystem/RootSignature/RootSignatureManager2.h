#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <ResourceSystem/RootSignature/RootSignature2.h>

struct RootSignatureDesc {
	std::vector<D3D12_ROOT_PARAMETER> parameters;
	std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
	D3D12_ROOT_SIGNATURE_FLAGS flags;
};

class RootSignatureManager2 {
public:
	static RootSignature2* GetOrCreateRootSignature(const std::string& key, const RootSignatureDesc& desc);
	static void Init(ID3D12Device* device);
	static void Shutdown();

private:
	static std::unordered_map<std::string, std::unique_ptr<RootSignature2>> rootSignatures_;
	static ID3D12Device* device_;
};