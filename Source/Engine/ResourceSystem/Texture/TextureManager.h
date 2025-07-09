#pragma once
#include <Renderer/D3D12.h>

#include <ResourceSystem/SRV/ShaderResourceViewManager.h>
#include <ResourceSystem/Texture/Texture.h>

class TextureManager {
public:
	void Init(D3D12* d3d12, SrvManager* srvManager);

	std::shared_ptr<Texture> GetTexture(const std::string& filePath);
	void                     UnloadTexture(const std::string& filePath);
	void                     Shutdown();

	void InitErrorTexture() const;

	static std::shared_ptr<Texture> GetErrorTexture();

private:
	[[nodiscard]] bool CreateErrorTexture(
		const std::shared_ptr<Texture>& texture) const;

	D3D12*      d3d12_      = nullptr;
	SrvManager* srvManager_ = nullptr;

	std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache_;
	// テクスチャのキャッシュ

	static std::shared_ptr<Texture> errorTexture_;
};
