#include "TextureManager.h"

#include "Lib/Console/Console.h"

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

	// TODO: テクスチャ読み込み失敗時にデフォルトテクスチャを読み込む もしくは真っ白のテクスチャを作成して返す

	Console::Print("[TextureManager] テクスチャの読み込みに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceManager);

	return nullptr;
}

void TextureManager::UnloadTexture(const std::string& filePath) {
	// 検索してあったら削除
	if (textureCache_.contains(filePath)) {
		textureCache_.erase(filePath);
	} else {
		Console::Print("テクスチャのアンロードに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceManager);
	}
}
