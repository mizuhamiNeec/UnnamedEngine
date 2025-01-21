#pragma once

#include <UnnamedResource/Material/MaterialManager.h>
#include <UnnamedResource/Mesh/MeshManager.h>
#include <UnnamedResource/Shader/ShaderManager.h>
#include <UnnamedResource/Texture/TextureManager.h>

class ResourceManager {
public:
	ResourceManager(D3D12* d3d12);
	~ResourceManager() = default;

	void Init() const;
	void Shutdown();

	[[nodiscard]] ShaderResourceViewManager* GetShaderResourceViewManager() const;
	[[nodiscard]] TextureManager* GetTextureManager() const;
	[[nodiscard]] ShaderManager* GetShaderManager() const;
	[[nodiscard]] MaterialManager* GetMaterialManager() const;
	[[nodiscard]] MeshManager* GetMeshManager() const;

private:
	D3D12* d3d12_;

	std::unique_ptr<ShaderResourceViewManager> srvManager_;
	std::unique_ptr<TextureManager> textureManager_;
	std::unique_ptr<ShaderManager> shaderManager_;
	std::unique_ptr<MaterialManager> materialManager_;
	std::unique_ptr<MeshManager> meshManager_;
};
