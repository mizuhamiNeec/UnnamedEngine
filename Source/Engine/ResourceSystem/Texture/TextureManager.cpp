#include "TextureManager.h"

#include <Lib/Console/Console.h>

void TextureManager::Init(D3D12* d3d12, ShaderResourceViewManager* srvManager) {
	Console::Print(
		"TextureManager を初期化しています...\n",
		kConsoleColorGray,
		Channel::ResourceSystem
	);
	d3d12_ = d3d12;
	srvManager_ = srvManager;
}

std::shared_ptr<Texture> TextureManager::GetTexture(const std::string& filePath) {
	// キャッシュを検索
	auto it = textureCache_.find(filePath);
	if (it != textureCache_.end()) {
		return it->second;
	}

	auto texture = std::make_shared<Texture>();
	if (!texture->LoadFromFile(d3d12_, srvManager_, filePath)) {
		Console::Print("テクスチャの読み込みに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceSystem);
		return errorTexture_;
	}

	textureCache_[filePath] = texture;
	return texture;
}

void TextureManager::UnloadTexture(const std::string& filePath) {
	// 検索してあったら削除
	if (textureCache_.contains(filePath)) {
		textureCache_.erase(filePath);
	} else {
		Console::Print("テクスチャのアンロードに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceSystem);
	}
}

void TextureManager::Shutdown() {
	textureCache_.clear();
	errorTexture_ = nullptr;
	d3d12_ = nullptr;
	srvManager_ = nullptr;
}

void TextureManager::InitErrorTexture() const {
	errorTexture_ = std::make_shared<Texture>();
	if (!CreateErrorTexture(errorTexture_)) {
		Console::Print("エラーテクスチャの作成に失敗しました\n", kConsoleColorError, Channel::ResourceSystem);
	}
}

std::shared_ptr<Texture> TextureManager::GetErrorTexture() {
	return errorTexture_;
}

bool TextureManager::CreateErrorTexture(const std::shared_ptr<Texture>& texture) const {
	return texture->CreateErrorTexture(d3d12_, srvManager_);
}

std::shared_ptr<Texture> TextureManager::errorTexture_ = nullptr;
