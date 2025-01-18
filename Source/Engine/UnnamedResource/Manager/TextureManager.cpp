#include "TextureManager.h"

#include <Lib/Console/Console.h>

std::shared_ptr<Texture> TextureManager::GetTexture(const std::string& filePath) {
	// キャッシュを検索
	auto it = textureCache_.find(filePath);
	if (it != textureCache_.end()) {
		return it->second;
	}

	// テクスチャを新しく読み込む
	auto texture = std::make_shared<Texture>();
	if (texture->LoadFromFile(d3d12_, srvManager_, filePath)) {
		textureCache_[filePath] = texture;
		return texture;
	}

	 // エラーテクスチャを検索
	static const std::string errorTextureKey = "__error_texture__";
	auto errorTexIt = textureCache_.find(errorTextureKey);
	if (errorTexIt != textureCache_.end()) {
		return errorTexIt->second;
	}

	// エラーテクスチャを作成
	auto errorTexture = std::make_shared<Texture>();
	if (CreateErrorTexture(errorTexture)) {
		textureCache_[errorTextureKey] = errorTexture;
		return errorTexture;
	}

	return nullptr;
}

void TextureManager::UnloadTexture(const std::string& filePath) {
	// 検索してあったら削除
	if (textureCache_.contains(filePath)) {
		textureCache_.erase(filePath);
	} else {
		Console::Print("テクスチャのアンロードに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceSystem);
	}
}

bool TextureManager::CreateErrorTexture(const std::shared_ptr<Texture>& texture) const {
	return texture->CreateErrorTexture(d3d12_, srvManager_);
}
