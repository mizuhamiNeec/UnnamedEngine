#pragma once
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class RootSignatureManager {
public:
	RootSignatureManager(ID3D12Device* device) : device_(device) {}

	bool CreateRootSignature(const std::string& name, const std::vector<D3D12_ROOT_PARAMETER>& rootParameters, const D3D12_STATIC_SAMPLER_DESC* staticSamplers = nullptr, UINT numStaticSamplers = 0);

	ID3D12RootSignature* Get(const std::string& name) const {
		auto it = rootSignatures_.find(name);
		if (it != rootSignatures_.end()) {
			return it->second.Get();
		}
		return nullptr;
	}

private:
	ID3D12Device* device_;
	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> rootSignatures_;
	RootSignatureManager* rootSignatureManager_ = nullptr;
};

