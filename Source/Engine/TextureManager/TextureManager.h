#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <wrl/client.h>

#include "DirectXTex/DirectXTex.h"
#include "Texture.h"

using namespace Microsoft::WRL;

class TextureManager {
public:
	static TextureManager& GetInstance();

	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	std::shared_ptr<Texture> GetTexture(ID3D12Device* device, const std::wstring& filename);

	void ReleaseUnusedTextures();

	void Shutdown();

private:
	TextureManager() {}
	~TextureManager() {}

	std::unordered_map<std::wstring, std::shared_ptr<Texture>> textures;
};
