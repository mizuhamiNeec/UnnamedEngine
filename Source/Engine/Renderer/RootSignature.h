#pragma once
#include <wrl/client.h>

#include "ConstantBuffer.h"

using namespace Microsoft::WRL;

class RootSignature {
public:
	RootSignature(const ComPtr<ID3D12Device>& device);
	ID3D12RootSignature* Get() const;

private:
	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
};
