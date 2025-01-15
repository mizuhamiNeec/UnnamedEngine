#pragma once
#include <Renderer/D3D12.h>
#include <UnnamedResource/Texture.h>
#include <UnnamedResource/Manager/ShaderResourceViewManager.h>

class TextureManager {
public:
	TextureManager(D3D12* d3d12, ShaderResourceViewManager* srvManager) : d3d12_(d3d12), srvManager_(srvManager) {}

	std::shared_ptr<Texture> GetTexture(const std::string& filePath);
	void UnloadTexture(const std::string& filePath);
private:
	D3D12* d3d12_ = nullptr;
	ShaderResourceViewManager* srvManager_ = nullptr;

	std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache_;  // テクスチャのキャッシュ
};

