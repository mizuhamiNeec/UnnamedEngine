#include "MeshManager.h"

#include <codecvt>
#include <filesystem>
#include <iostream>
#include <locale>
#include <string>

#include <Lib/Console/Console.h>

#include <ResourceSystem/Manager/ResourceManager.h>
#include <ResourceSystem/Material/MaterialManager.h>
#include <ResourceSystem/Shader/DefaultShader.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "Lib/Utils/StrUtils.h"

void MeshManager::Init(const ComPtr<ID3D12Device>& device, TextureManager* textureManager, ShaderManager* shaderManager, MaterialManager* materialManager) {
	Console::Print(
		"MeshManager を初期化しています...\n",
		kConsoleColorGray,
		Channel::ResourceSystem
	);
	device_ = device;
	textureManager_ = textureManager;
	shaderManager_ = shaderManager;
	materialManager_ = materialManager;
}

void MeshManager::Shutdown() {
	Console::Print(
		"MeshManager を終了しています...\n",
		kConsoleColorGray,
		Channel::ResourceSystem
	);

	for (auto& pair : subMeshes_) {
		pair.second->ReleaseResource();
		pair.second.reset();
	}
	subMeshes_.clear();

	for (auto& pair : staticMeshes_) {
		if (pair.second) {
			pair.second->ReleaseResource();
		}
	}
	staticMeshes_.clear();

	device_ = nullptr;
	textureManager_ = nullptr;
	shaderManager_ = nullptr;
	materialManager_ = nullptr;
}

StaticMesh* MeshManager::CreateStaticMesh(const std::string& name) {
	const auto it = staticMeshes_.find(name);
	if (it != staticMeshes_.end()) {
		return it->second.get();
	}
	auto mesh = std::make_unique<StaticMesh>(name);
	auto meshPtr = mesh.get();
	staticMeshes_[name] = std::move(mesh);
	return meshPtr;
}

SubMesh* MeshManager::CreateSubMesh(const std::string& name) {
	const auto it = subMeshes_.find(name);
	if (it != subMeshes_.end()) {
		return it->second.get();
	}
	auto subMesh = std::make_unique<SubMesh>(device_, name);
	auto subMeshPtr = subMesh.get();
	subMeshes_[name] = std::move(subMesh);
	return subMeshPtr;
}

bool MeshManager::LoadMeshFromFile(const std::string& filePath) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		filePath,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_ConvertToLeftHanded
	);
	if (!scene) {
		Console::Print("メッシュの読み込みに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceSystem);
		Console::Print("エラーメッセージ: " + std::string(importer.GetErrorString()) + "\n", kConsoleColorError, Channel::ResourceSystem);
		assert(scene); // 読み込み失敗
		return false;
	}

	if (!scene->HasMeshes()) {
		Console::Print("メッシュがありません: " + filePath + "\n", kConsoleColorError, Channel::ResourceSystem);
		assert(scene->HasMeshes()); // メッシュがない場合はエラー
	}

	StaticMesh* staticMesh = CreateStaticMesh(filePath);
	ProcessNode(scene->mRootNode, scene, staticMesh);
	return true;
}

StaticMesh* MeshManager::GetStaticMesh(const std::string& name) const {
	const auto it = staticMeshes_.find(name);
	return it != staticMeshes_.end() ? it->second.get() : nullptr;
}

void MeshManager::ProcessNode(const aiNode* node, const aiScene* scene, StaticMesh* staticMesh) {
	if (!node || !scene || !scene->mMeshes) {
		Console::Print("無効なノードまたはシーン\n", kConsoleColorError, Channel::ResourceSystem);
		return;
	}

	// ノードの変換行列を取得
	aiMatrix4x4 transform = node->mTransformation;

	for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
		if (node->mMeshes[meshIndex] >= scene->mNumMeshes) {
			Console::Print("メッシュインデックスが範囲外です\n", kConsoleColorError, Channel::ResourceSystem);
			continue;
		}
		const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
		std::unique_ptr<SubMesh> subMesh = std::unique_ptr<SubMesh>(ProcessMesh(mesh, scene, staticMesh, transform));
		staticMesh->AddSubMesh(std::move(subMesh));
	}

	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		ProcessNode(node->mChildren[childIndex], scene, staticMesh);
	}
}

SubMesh* MeshManager::ProcessMesh(const aiMesh* mesh, const aiScene* scene, StaticMesh* staticMesh, const aiMatrix4x4& transform) {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	// 頂点情報の取得
	for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
		Vertex vertex;

		// 変換行列を頂点に適用
		aiVector3D pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		aiVector3D transformedPos = transform * pos;
		vertex.position = Vec4(transformedPos.x, transformedPos.y, transformedPos.z, 1.0f);

		// 法線にも回転を適用（スケールは除外）
		aiMatrix3x3 normalMatrix(transform);
		aiVector3D normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		aiVector3D transformedNormal = normalMatrix * normal;
		transformedNormal.Normalize();
		vertex.normal = Vec3(transformedNormal.x, transformedNormal.y, transformedNormal.z);

		if (mesh->mTextureCoords[0]) {
			vertex.uv = Vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		} else {
			vertex.uv = Vec2::zero;
		}
		vertices.push_back(vertex);
	}

	Console::Print("頂点数: " + std::to_string(mesh->mNumVertices) + "\n", kConsoleColorGray, Channel::ResourceSystem);

	// 面情報の取得
	for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
		const aiFace face = mesh->mFaces[faceIndex];
		assert(face.mNumIndices == 3); // 三角形以外はエラー
		for (uint32_t j = 0; j < face.mNumIndices; ++j) {
			indices.push_back(face.mIndices[j]);
		}
	}

	Console::Print("三角面数: " + std::to_string(mesh->mNumFaces) + "\n", kConsoleColorGray, Channel::ResourceSystem);

	// マテリアルの設定
	Material* material = nullptr;
	if (mesh->mMaterialIndex >= 0) {
		const aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
		const std::string materialName = aiMat->GetName().C_Str();

		// マテリアルの作成またはキャッシュから取得
		material = materialManager_->GetOrCreateMaterial(materialName, DefaultShader::CreateDefaultShader(shaderManager_));

		// テクスチャの設定
		aiString texturePath;
		if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
			// モデルのディレクトリパスを取得
			std::filesystem::path modelPath(staticMesh->GetName());
			std::filesystem::path modelDir = modelPath.parent_path();

			// texturePath.C_Str() の値からテクスチャパスを作成
			const char* texturePathStr = texturePath.C_Str();
			if (texturePathStr && strlen(texturePathStr) > 0) {
				// テクスチャパスを結合して完全なパスを作成
				std::filesystem::path texPath(texturePathStr);
				std::filesystem::path fullTexturePath = modelDir / texPath;

				Console::Print(
					"Loading texture: " + fullTexturePath.string() + "\n",
					kConsoleColorCompleted,
					Channel::ResourceSystem
				);

				Texture* texture = textureManager_->GetTexture(fullTexturePath.string()).get();
				if (texture) {
					material->SetTexture("diffuseTexture", texture);
				}
			}
		} else {
			material->SetTexture("diffuseTexture", TextureManager::GetErrorTexture().get());
		}
	}

	auto subMesh = std::make_unique<SubMesh>(device_, mesh->mName.C_Str());
	subMesh->SetVertexBuffer(vertices);
	subMesh->SetIndexBuffer(indices);
	if (material) {
		subMesh->SetMaterial(material);
	}
	return subMesh.release();
}
