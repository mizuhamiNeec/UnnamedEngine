#pragma once
#include <string>

#include <ResourceSystem/Mesh/StaticMesh.h>

#include <assimp/scene.h>

#include "ResourceSystem/Material/MaterialManager.h"
#include "ResourceSystem/Shader/ShaderManager.h"
#include "ResourceSystem/Texture/TextureManager.h"

class MeshManager {
public:
	void Init(const ComPtr<ID3D12Device>& device, ShaderManager* shaderManager,
	          MaterialManager*            materialManager
	);
	void Shutdown();

	StaticMesh* CreateStaticMesh(const std::string& name);
	SubMesh*    CreateSubMesh(const std::string& name);

	bool LoadMeshFromFile(const std::string& filePath);

	StaticMesh* GetStaticMesh(const std::string& name) const;

private:
	void ProcessNode(const aiNode* node, const aiScene* scene,
	                 StaticMesh* staticMesh);
	SubMesh* ProcessMesh(const aiMesh* mesh, const aiScene* scene,
	                     StaticMesh* staticMesh, const aiMatrix4x4& transform);

	TextureManager*  textureManager_  = nullptr;
	ShaderManager*   shaderManager_   = nullptr;
	MaterialManager* materialManager_ = nullptr;
	D3D12*           renderer_        = nullptr;

	ComPtr<ID3D12Device>                                         device_;
	std::unordered_map<std::string, std::unique_ptr<StaticMesh>> staticMeshes_;
	std::unordered_map<std::string, std::unique_ptr<SubMesh>>    subMeshes_;
};
