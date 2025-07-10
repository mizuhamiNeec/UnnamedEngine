#include "MeshManager.h"

#include <codecvt>
#include <filesystem>
#include <iostream>
#include <locale>
#include <ranges>
#include <string>

#include <SubSystem/Console/Console.h>

#include <ResourceSystem/Manager/ResourceManager.h>
#include <ResourceSystem/Material/MaterialManager.h>
#include <ResourceSystem/Shader/DefaultShader.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "Lib/Utils/StrUtil.h"

void MeshManager::Init(const ComPtr<ID3D12Device>& device,
                       ShaderManager*              shaderManager,
                       MaterialManager*            materialManager) {
	Console::Print(
		"MeshManager を初期化しています...\n",
		kConTextColorGray,
		Channel::ResourceSystem
	);
	device_          = device;
	shaderManager_   = shaderManager;
	materialManager_ = materialManager;
}

void MeshManager::Shutdown() {
	Console::Print(
		"MeshManager を終了しています...\n",
		kConTextColorGray,
		Channel::ResourceSystem
	);

	for (auto& snd : subMeshes_ | std::views::values) {
		Console::Print(
			"MeshMgr: " + snd->GetName() + "\n",
			Vec4::white,
			Channel::ResourceSystem
		);
		snd->ReleaseResource();
		snd.reset();
	}
	subMeshes_.clear();

	for (auto& snd : staticMeshes_ | std::views::values) {
		if (snd) {
			Console::Print(
				"MeshMgr: Releasing " + snd->GetName() + "\n",
				Vec4::white,
				Channel::ResourceSystem
			);
			snd->ReleaseResource();
		}
	}
	staticMeshes_.clear();

	device_         = nullptr;
	texManager_     = nullptr;
	shaderManager_  = nullptr;
	materialManager_ = nullptr;
}

StaticMesh* MeshManager::CreateStaticMesh(const std::string& name) {
	const auto it = staticMeshes_.find(name);
	if (it != staticMeshes_.end()) {
		return it->second.get();
	}
	auto mesh           = std::make_unique<StaticMesh>(name);
	auto meshPtr        = mesh.get();
	staticMeshes_[name] = std::move(mesh);
	return meshPtr;
}

SubMesh* MeshManager::CreateSubMesh(const std::string& name) {
	const auto it = subMeshes_.find(name);
	if (it != subMeshes_.end()) {
		return it->second.get();
	}
	auto subMesh     = std::make_unique<SubMesh>(device_, name);
	auto subMeshPtr  = subMesh.get();
	subMeshes_[name] = std::move(subMesh);
	return subMeshPtr;
}

bool MeshManager::LoadMeshFromFile(const std::string& filePath) {
	Assimp::Importer importer;
	const aiScene*   scene = importer.ReadFile(
		filePath,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_ConvertToLeftHanded
	);
	if (!scene) {
		Console::Print("メッシュの読み込みに失敗しました: " + filePath + "\n",
		               kConTextColorError, Channel::ResourceSystem);
		Console::Print(
			"エラーメッセージ: " + std::string(importer.GetErrorString()) + "\n",
			kConTextColorError, Channel::ResourceSystem);
		assert(scene); // 読み込み失敗
		return false;
	}

	if (!scene->HasMeshes()) {
		Console::Print("メッシュがありません: " + filePath + "\n", kConTextColorError,
		               Channel::ResourceSystem);
		assert(scene->HasMeshes()); // メッシュがない場合はエラー
	}

	StaticMesh* staticMesh = CreateStaticMesh(filePath);
	ProcessNode(scene->mRootNode, scene, staticMesh);

	Console::Print("メッシュの読み込みに成功しました: " + filePath + "\n",
	               kConTextColorCompleted, Channel::ResourceSystem);
	return true;
}

bool MeshManager::ReloadMeshFromFile(const std::string& filePath) {
	Console::Print("メッシュをリロードしています: " + filePath + "\n",
	               kConTextColorWarning, Channel::ResourceSystem);
	
	// 既存のメッシュを削除
	auto it = staticMeshes_.find(filePath);
	if (it != staticMeshes_.end()) {
		Console::Print("既存のメッシュを削除しました: " + filePath + "\n",
		               kConTextColorWarning, Channel::ResourceSystem);
		staticMeshes_.erase(it);
	}
	
	// 関連するサブメッシュも削除（メッシュ名をキーとして検索）
	for (auto subMeshIt = subMeshes_.begin(); subMeshIt != subMeshes_.end();) {
		if (subMeshIt->first.find(filePath) != std::string::npos) {
			Console::Print("関連するサブメッシュを削除しました: " + subMeshIt->first + "\n",
			               kConTextColorWarning, Channel::ResourceSystem);
			subMeshIt = subMeshes_.erase(subMeshIt);
		} else {
			++subMeshIt;
		}
	}
	
	// 新しいメッシュを読み込み
	bool result = LoadMeshFromFile(filePath);
	
	if (result) {
		Console::Print("メッシュのリロードに成功しました: " + filePath + "\n",
		               kConTextColorCompleted, Channel::ResourceSystem);
	} else {
		Console::Print("メッシュのリロードに失敗しました: " + filePath + "\n",
		               kConTextColorError, Channel::ResourceSystem);
	}
	
	return result;
}

StaticMesh* MeshManager::GetStaticMesh(const std::string& name) const {
	const auto it = staticMeshes_.find(name);
	return it != staticMeshes_.end() ? it->second.get() : nullptr;
}

void MeshManager::ProcessNode(const aiNode* node, const aiScene* scene,
                              StaticMesh*   staticMesh) {
	if (!node || !scene || !scene->mMeshes) {
		Console::Print("無効なノードまたはシーン\n", kConTextColorError,
		               Channel::ResourceSystem);
		return;
	}

	// ノードの変換行列を取得
	aiMatrix4x4 transform = node->mTransformation;

	for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
		if (node->mMeshes[meshIndex] >= scene->mNumMeshes) {
			Console::Print("メッシュインデックスが範囲外です\n", kConTextColorError,
			               Channel::ResourceSystem);
			continue;
		}
		const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
		std::unique_ptr<SubMesh> subMesh = std::unique_ptr<SubMesh>(
			ProcessMesh(mesh, scene, staticMesh, transform));
		staticMesh->AddSubMesh(std::move(subMesh));
	}

	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++
	     childIndex) {
		ProcessNode(node->mChildren[childIndex], scene, staticMesh);
	}
}

SubMesh* MeshManager::ProcessMesh(const aiMesh*      mesh, const aiScene* scene,
                                  StaticMesh*        staticMesh,
                                  const aiMatrix4x4& transform) {
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	// 頂点情報の取得
	for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
		Vertex vertex;

		// 変換行列を頂点に適用
		aiVector3D pos(mesh->mVertices[i].x, mesh->mVertices[i].y,
		               mesh->mVertices[i].z);
		aiVector3D transformedPos = transform * pos;
		vertex.position           = Vec4(transformedPos.x, transformedPos.y,
		                       transformedPos.z, 1.0f);

		//vertex.position = Vec4(Math::HtoM(transformedPos.x), Math::HtoM(transformedPos.y), Math::HtoM(transformedPos.z), 1.0f);

		// 法線にも回転を適用（スケールは除外）
		aiMatrix3x3 normalMatrix(transform);
		aiVector3D  normal(mesh->mNormals[i].x, mesh->mNormals[i].y,
		                  mesh->mNormals[i].z);
		aiVector3D transformedNormal = normalMatrix * normal;
		transformedNormal.Normalize();
		vertex.normal = Vec3(transformedNormal.x, transformedNormal.y,
		                     transformedNormal.z);

		if (mesh->mTextureCoords[0]) {
			vertex.uv = Vec2(mesh->mTextureCoords[0][i].x,
			                 mesh->mTextureCoords[0][i].y);
		} else {
			vertex.uv = Vec2::zero;
		}
		vertices.emplace_back(vertex);
	}

	Console::Print("頂点数: " + std::to_string(mesh->mNumVertices) + "\n",
	               kConTextColorGray, Channel::ResourceSystem);

	// 面情報の取得
	for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
		const aiFace face = mesh->mFaces[faceIndex];
		assert(face.mNumIndices == 3); // 三角形以外はエラー
		for (uint32_t j = 0; j < face.mNumIndices; ++j) {
			indices.emplace_back(face.mIndices[j]);
		}
	}

	Console::Print("三角面数: " + std::to_string(mesh->mNumFaces) + "\n",
	               kConTextColorGray, Channel::ResourceSystem);

	// マテリアルの設定
	Material* material = nullptr;
	if (mesh->mMaterialIndex >= 0) {
		const aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
		const std::string materialName = aiMat->GetName().C_Str();

		// メッシュ名を取得（ファイルパスからファイル名のみを抽出）
		std::string meshName = staticMesh->GetName();
		size_t lastSlash = meshName.find_last_of("/\\");
		if (lastSlash != std::string::npos) {
			meshName = meshName.substr(lastSlash + 1);
		}
		// 拡張子を削除
		size_t lastDot = meshName.find_last_of('.');
		if (lastDot != std::string::npos) {
			meshName = meshName.substr(0, lastDot);
		}

		// マテリアルの作成またはキャッシュから取得（メッシュ名を含む一意キーで管理）
		material = materialManager_->GetOrCreateMaterial(
			materialName, 
			DefaultShader::CreateDefaultShader(shaderManager_),
			meshName
		);

		Console::Print(
			std::format("MeshManager: マテリアル割り当て - メッシュ: {}, マテリアル: {} (フルネーム: {})\n", 
				meshName, materialName, material->GetFullName()),
			kConTextColorCompleted,
			Channel::ResourceSystem
		);

		// テクスチャの設定
		aiString texturePath;
		if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) ==
			AI_SUCCESS) {
			// モデルのディレクトリパスを取得
			std::filesystem::path modelPath(staticMesh->GetName());
			std::filesystem::path modelDir = modelPath.parent_path();

			// texturePath.C_Str() の値からテクスチャパスを作成
			const char* texturePathStr = texturePath.C_Str();
			if (texturePathStr && strlen(texturePathStr) > 0) {
				// テクスチャパスを結合して完全なパスを作成
				std::filesystem::path texPath(texturePathStr);
				std::filesystem::path fullTexturePath = modelDir / texPath;

				// 旧TexManagerを使用してテクスチャを読み込み
				TexManager::GetInstance()->LoadTexture(fullTexturePath.string());
				material->SetTexture("gBaseColorTexture", fullTexturePath.string());
			}
		} else {
			// エラーテクスチャのパスを設定（必要に応じて実装）
			material->SetTexture("gBaseColorTexture", "./Resources/Textures/uvChecker.png");
		}

		// 環境マップテクスチャの設定
		TexManager::GetInstance()->LoadTexture("./Resources/Textures/wave.dds");
		material->SetTexture("gEnvironmentTexture", "./Resources/Textures/wave.dds");
	}

	auto subMesh = std::make_unique<SubMesh>(device_, mesh->mName.C_Str());
	subMesh->SetVertexBuffer(vertices);
	subMesh->SetIndexBuffer(indices);
	if (material) {
		subMesh->SetMaterial(material);
	}
	return subMesh.release();
}
