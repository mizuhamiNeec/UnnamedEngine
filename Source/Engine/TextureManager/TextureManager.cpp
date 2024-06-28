#include "TextureManager.h"
#include "../Utils/Logger.h"
#include <format>

#include "../Utils/ConvertString.h"

TextureManager& TextureManager::GetInstance() {
	static TextureManager instance;
	return instance;
}

std::shared_ptr<Texture> TextureManager::GetTexture(ID3D12Device* device, const std::wstring& filename) {
	auto it = textures.find(filename);
	if (it != textures.end()) {
		return it->second;
	}

	std::shared_ptr<Texture> texture = std::make_shared<Texture>(device, filename);
	textures[filename] = texture;
	return texture;
}

void TextureManager::ReleaseUnusedTextures() {
	for (auto it = textures.begin(); it != textures.end(); ) {
		if (it->second.use_count() == 1) {
			it = textures.erase(it);
		} else {
			++it;
		}
	}
}

void TextureManager::Shutdown() {
	for (auto& pair : textures) {
		if (pair.second.use_count() > 1) {
			// テクスチャがまだ参照されている
			Log(ConvertString(std::format(L"Texture {} is still in use, use_count: {}\n", pair.first.c_str(), pair.second.use_count())));
		}
	}
	textures.clear();
}
