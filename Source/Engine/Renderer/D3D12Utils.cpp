#include "D3D12Utils.h"

#include <DirectXTex/d3dx12.h>

#include <Lib/Utils/StrUtils.h>

std::string D3D12Utils::GetResourceName(const ComPtr<ID3D12Resource>& resource) {
	if (!resource) {
		return "";
	}

	// リソース名の長さを取得
	UINT size = 0;
	HRESULT hr = resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, nullptr);
	if (FAILED(hr) || size == 0) {
		return "";
	}

	// リソース名を取得
	std::string name(size, '\0');
	hr = resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, name.data());
	if (FAILED(hr)) {
		return "";
	}

	// 末尾のヌル文字を削除
	if (!name.empty() && name.back() == '\0') {
		name.pop_back();
	}

	return name;
}
