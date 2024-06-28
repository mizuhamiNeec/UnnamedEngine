#pragma once

#include <string>
#include <wrl/client.h>

#include "../../Engine/Renderer/D3D12.h"

using namespace Microsoft::WRL;

class Texture {
public:
	Texture(ID3D12Device* device, const std::wstring& filename);
	~Texture();

	ID3D12Resource* GetResource() const { return textureResource_.Get(); }
	ID3D12DescriptorHeap* GetSRVHeap() const { return srvHeap_.Get(); }

private:
	ComPtr<ID3D12Resource> textureResource_;
	ComPtr<ID3D12DescriptorHeap> srvHeap_;

	void LoadTextureFromFile(ID3D12Device* device, const std::wstring& filename);
	void CreateShaderResourceView(ID3D12Device* device);
};
