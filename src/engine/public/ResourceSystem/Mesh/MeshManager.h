#pragma once
#include <string>

#include <assimp/scene.h>

#include <engine/public/ResourceSystem/Mesh/SkeletalMesh.h>
#include <engine/public/ResourceSystem/Mesh/StaticMesh.h>

class D3D12;
class MaterialManager;
class ShaderManager;
class TexManager;

class MeshManager {
public:
	void Init(ID3D12Device*    device, ShaderManager* shaderManager,
	          MaterialManager* materialManager
	);
	void Shutdown();

	// StaticMesh
	bool                      LoadMeshFromFile(const std::string& filePath);
	bool                      ReloadMeshFromFile(const std::string& filePath);
	[[nodiscard]] StaticMesh* GetStaticMesh(const std::string& name) const;
	StaticMesh*               CreateStaticMesh(const std::string& name);

	// SkeletalMesh
	bool LoadSkeletalMeshFromFile(const std::string& filePath);
	[[nodiscard]] SkeletalMesh* GetSkeletalMesh(const std::string& name) const;
	SkeletalMesh* CreateSkeletalMesh(const std::string& name);

	SubMesh* CreateSubMesh(const std::string& name);

private:
	void ProcessStaticMeshNode(const aiNode* node, const aiScene* scene,
	                           StaticMesh*   staticMesh);
	void ProcessSkeletalMeshNode(aiNode*       node, const aiScene* scene,
	                             SkeletalMesh* skeletalMesh);

	SubMesh* ProcessMesh(const aiMesh* mesh, const aiScene* scene,
	                     StaticMesh* staticMesh, const aiMatrix4x4& transform);

	// スケルタルメッシュ専用の処理関数
	SubMesh* ProcessSkeletalMesh(const aiMesh*      mesh, const aiScene* scene,
	                             SkeletalMesh*      skeletalMesh,
	                             const aiMatrix4x4& transform);

	// スケルトン読み込み関数
	static Skeleton    LoadSkeleton(const aiScene* scene);
	static Node LoadNode(const aiNode* aiNode);

	// アニメーション読み込み関数
	static void             LoadAnimations(const aiScene* scene, SkeletalMesh* skeletalMesh);
	static Animation LoadAnimation(const aiAnimation* aiAnim);

	TexManager*      mTexManager      = nullptr;
	ShaderManager*   mShaderManager   = nullptr;
	MaterialManager* mMaterialManager = nullptr;
	D3D12*           mRenderer        = nullptr;
	ID3D12Device*    mDevice          = nullptr;

	std::unordered_map<std::string, std::unique_ptr<StaticMesh>> mStaticMeshes;
	std::unordered_map<std::string, std::unique_ptr<SubMesh>>    mSubMeshes;
	std::unordered_map<std::string, std::unique_ptr<SkeletalMesh>>
	mSkeletalMeshes;
};
