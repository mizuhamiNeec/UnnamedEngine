#pragma once

#include <d3d12.h>
#include <string>
#include <DirectXTex/DirectXTex.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace D3D12Utils {
	std::string GetResourceName(const ComPtr<ID3D12Resource>& resource);
}
