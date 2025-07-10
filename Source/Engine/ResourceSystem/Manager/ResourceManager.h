#pragma once

#include <ResourceSystem/Material/MaterialManager.h>
#include <ResourceSystem/Mesh/MeshManager.h>
#include <ResourceSystem/Shader/ShaderManager.h>
#include <TextureManager/TexManager.h>
#include <SrvManager.h>

class ResourceManager {
public:
	ResourceManager(D3D12* d3d12);
	~ResourceManager() = default;

	void Init() const;
	void Shutdown();

	[[nodiscard]] SrvManager* GetSrvManager() const;
	[[nodiscard]] TexManager* GetTexManager() const;
	[[nodiscard]] ShaderManager* GetShaderManager() const;
	[[nodiscard]] MaterialManager* GetMaterialManager() const;
	[[nodiscard]] MeshManager* GetMeshManager() const;

private:
	D3D12* d3d12_;

	std::unique_ptr<SrvManager> srvManager_;
	std::unique_ptr<ShaderManager> shaderManager_;
	std::unique_ptr<MaterialManager> materialManager_;
	std::unique_ptr<MeshManager> meshManager_;
};
