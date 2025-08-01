#include "engine/public/ResourceSystem/Mesh/MeshManager.h"

#include <codecvt>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <format>


#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/postprocess.h>

#include "engine/public/OldConsole/Console.h"
#include "engine/public/ResourceSystem/Material/MaterialManager.h"
#include "engine/public/ResourceSystem/Shader/DefaultShader.h"
#include "engine/public/TextureManager/TexManager.h"

void MeshManager::Init(ID3D12Device*    device, ShaderManager* shaderManager,
                       MaterialManager* materialManager) {
	Console::Print(
		"MeshManager を初期化しています...\n",
		kConTextColorGray,
		Channel::ResourceSystem
	);
	mDevice          = device;
	mShaderManager   = shaderManager;
	mMaterialManager = materialManager;
}

void MeshManager::Shutdown() {
	Console::Print(
		"MeshManager を終了しています...\n",
		kConTextColorGray,
		Channel::ResourceSystem
	);

	for (auto& snd : mSubMeshes | std::views::values) {
		Console::Print(
			"MeshMgr: " + snd->GetName() + "\n",
			Vec4::white,
			Channel::ResourceSystem
		);
		snd->ReleaseResource();
		snd.reset();
	}
	mSubMeshes.clear();

	for (auto& snd : mStaticMeshes | std::views::values) {
		if (snd) {
			Console::Print(
				"MeshMgr: Releasing " + snd->GetName() + "\n",
				Vec4::white,
				Channel::ResourceSystem
			);
			snd->ReleaseResource();
		}
	}
	mStaticMeshes.clear();

	mDevice          = nullptr;
	mTexManager      = nullptr;
	mShaderManager   = nullptr;
	mMaterialManager = nullptr;
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
	ProcessStaticMeshNode(scene->mRootNode, scene, staticMesh);

	Console::Print("メッシュの読み込みに成功しました: " + filePath + "\n",
	               kConTextColorCompleted, Channel::ResourceSystem);
	return true;
}

bool MeshManager::ReloadMeshFromFile(const std::string& filePath) {
	Console::Print("メッシュをリロードしています: " + filePath + "\n",
	               kConTextColorWarning, Channel::ResourceSystem);

	// 既存のメッシュを削除
	auto it = mStaticMeshes.find(filePath);
	if (it != mStaticMeshes.end()) {
		Console::Print("既存のメッシュを削除しました: " + filePath + "\n",
		               kConTextColorWarning, Channel::ResourceSystem);
		mStaticMeshes.erase(it);
	}

	// 関連するサブメッシュも削除（メッシュ名をキーとして検索）
	for (auto subMeshIt = mSubMeshes.begin(); subMeshIt != mSubMeshes.end();) {
		if (subMeshIt->first.find(filePath) != std::string::npos) {
			Console::Print("関連するサブメッシュを削除しました: " + subMeshIt->first + "\n",
			               kConTextColorWarning, Channel::ResourceSystem);
			subMeshIt = mSubMeshes.erase(subMeshIt);
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
	const auto it = mStaticMeshes.find(name);
	return it != mStaticMeshes.end() ? it->second.get() : nullptr;
}

StaticMesh* MeshManager::CreateStaticMesh(const std::string& name) {
	const auto it = mStaticMeshes.find(name);
	if (it != mStaticMeshes.end()) {
		return it->second.get();
	}
	auto       mesh     = std::make_unique<StaticMesh>(name);
	const auto meshPtr  = mesh.get();
	mStaticMeshes[name] = std::move(mesh);
	return meshPtr;
}

bool MeshManager::LoadSkeletalMeshFromFile(const std::string& filePath) {
	Assimp::Importer importer;
	const aiScene*   scene = importer.ReadFile(
		filePath,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_ConvertToLeftHanded |
		aiProcess_LimitBoneWeights
	);

	if (!scene) {
		Console::Print("スケルタルメッシュの読み込みに失敗しました: " + filePath + "\n",
		               kConTextColorError, Channel::ResourceSystem);
		Console::Print(
			"エラーメッセージ: " + std::string(importer.GetErrorString()) + "\n",
			kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	if (!scene->HasMeshes()) {
		Console::Print("メッシュがありません: " + filePath + "\n", kConTextColorError,
		               Channel::ResourceSystem);
		return false;
	}

	SkeletalMesh* skeletalMesh = CreateSkeletalMesh(filePath);

	// スケルトンを読み込み
	if (scene->mNumMeshes > 0 && scene->mMeshes[0]->HasBones()) {
		Skeleton skeleton = LoadSkeleton(scene);
		skeletalMesh->SetSkeleton(skeleton);
	}

	// メッシュを処理
	ProcessSkeletalMeshNode(scene->mRootNode, scene, skeletalMesh);

	// アニメーションを読み込み
	if (scene->HasAnimations()) {
		LoadAnimations(scene, skeletalMesh);
	}

	Console::Print("スケルタルメッシュの読み込みに成功しました: " + filePath + "\n",
	               kConTextColorCompleted, Channel::ResourceSystem);
	return true;
}

SkeletalMesh* MeshManager::GetSkeletalMesh(const std::string& name) const {
	return mSkeletalMeshes.contains(name)
		       ? mSkeletalMeshes.at(name).get()
		       : nullptr;
}

SkeletalMesh* MeshManager::CreateSkeletalMesh(const std::string& name) {
	const auto it = mSkeletalMeshes.find(name);
	if (it != mSkeletalMeshes.end()) {
		return it->second.get();
	}
	auto mesh             = std::make_unique<SkeletalMesh>(name);
	auto meshPtr          = mesh.get();
	mSkeletalMeshes[name] = std::move(mesh);
	return meshPtr;
}


SubMesh* MeshManager::CreateSubMesh(const std::string& name) {
	const auto it = mSubMeshes.find(name);
	if (it != mSubMeshes.end()) {
		return it->second.get();
	}
	auto subMesh     = std::make_unique<SubMesh>(mDevice, name);
	auto subMeshPtr  = subMesh.get();
	mSubMeshes[name] = std::move(subMesh);
	return subMeshPtr;
}

void MeshManager::ProcessStaticMeshNode(const aiNode*  node,
                                        const aiScene* scene,
                                        StaticMesh*    staticMesh) {
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
		ProcessStaticMeshNode(node->mChildren[childIndex], scene, staticMesh);
	}
}

void MeshManager::ProcessSkeletalMeshNode(
	aiNode*        node,
	const aiScene* scene,
	SkeletalMesh*  skeletalMesh
) {
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
			ProcessSkeletalMesh(mesh, scene, skeletalMesh, transform));
		skeletalMesh->AddSubMesh(std::move(subMesh));
	}

	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++
	     childIndex) {
		ProcessSkeletalMeshNode(node->mChildren[childIndex], scene,
		                        skeletalMesh);
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
		std::string meshName  = staticMesh->GetName();
		size_t      lastSlash = meshName.find_last_of("/\\");
		if (lastSlash != std::string::npos) {
			meshName = meshName.substr(lastSlash + 1);
		}
		// 拡張子を削除
		size_t lastDot = meshName.find_last_of('.');
		if (lastDot != std::string::npos) {
			meshName = meshName.substr(0, lastDot);
		}

		// マテリアルの作成またはキャッシュから取得（メッシュ名を含む一意キーで管理）
		material = mMaterialManager->GetOrCreateMaterial(
			materialName,
			DefaultShader::CreateDefaultShader(mShaderManager),
			meshName
		);

		Console::Print(
			std::format(
				"MeshManager: マテリアル割り当て - メッシュ: {}, マテリアル: {} (フルネーム: {})\n",
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
				TexManager::GetInstance()->
					LoadTexture(fullTexturePath.string());
				material->SetTexture("gBaseColorTexture",
				                     fullTexturePath.string());
			}
		} else {
			// エラーテクスチャのパスを設定（必要に応じて実装）
			material->SetTexture("gBaseColorTexture",
			                     "./resources/textures/uvChecker.png");
		}

		// 環境マップテクスチャの設定
		// シェーダーがTexture2Dを期待している場合は、forceCubeMapをfalseにする
		TexManager::GetInstance()->LoadTexture("./resources/textures/wave.dds",
		                                       false);
		material->SetTexture("gEnvironmentTexture",
		                     "./resources/textures/wave.dds");
	}

	auto subMesh = std::make_unique<SubMesh>(mDevice, mesh->mName.C_Str());
	subMesh->SetVertexBuffer(vertices);
	subMesh->SetIndexBuffer(indices);
	if (material) {
		subMesh->SetMaterial(material);
	}
	return subMesh.release();
}

SubMesh* MeshManager::ProcessSkeletalMesh(
	const aiMesh*      mesh,
	const aiScene*     scene,
	SkeletalMesh*      skeletalMesh,
	const aiMatrix4x4& transform
) {
	std::vector<SkinnedVertex> vertices;
	std::vector<uint32_t>      indices;

	// 頂点ボーンデータの初期化
	struct VertexBoneData {
		float    weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		uint32_t indices[4] = {0, 0, 0, 0};
		int      count      = 0;
	};
	std::vector<VertexBoneData> vertexBoneData(mesh->mNumVertices);

	// ボーン情報の処理
	if (mesh->HasBones()) {
		const Skeleton& skeleton = skeletalMesh->GetSkeleton();

		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			const aiBone* bone     = mesh->mBones[boneIndex];
			std::string   boneName = bone->mName.C_Str();

			// スケルトンからボーンIDを取得
			auto it = skeleton.boneMap.find(boneName);
			if (it == skeleton.boneMap.end()) {
				Console::Print("ボーンが見つかりません: " + boneName + "\n",
				               kConTextColorWarning, Channel::ResourceSystem);
				continue;
			}

			uint32_t globalBoneIndex = it->second;

			// ボーンインデックスが範囲内かチェック
			if (globalBoneIndex >= 256) {
				Console::Print(
					"ボーンインデックスが範囲外です: " + std::to_string(globalBoneIndex) +
					" (最大255)\n",
					kConTextColorError, Channel::ResourceSystem);
				continue;
			}

			// このボーンが影響する頂点のウェイトを設定
			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++
			     weightIndex) {
				const aiVertexWeight& weight   = bone->mWeights[weightIndex];
				uint32_t              vertexId = weight.mVertexId;

				if (vertexId >= mesh->mNumVertices) {
					Console::Print("頂点インデックスが範囲外です\n",
					               kConTextColorError, Channel::ResourceSystem);
					continue;
				}

				VertexBoneData& boneData = vertexBoneData[vertexId];
				if (boneData.count < 4) {
					boneData.weights[boneData.count] = weight.mWeight;
					boneData.indices[boneData.count] = globalBoneIndex;
					boneData.count++;
				}
			}
		}
	}

	// 頂点情報の取得
	for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
		SkinnedVertex vertex;

		// 変換行列を頂点に適用
		aiVector3D pos(mesh->mVertices[i].x, mesh->mVertices[i].y,
		               mesh->mVertices[i].z);
		aiVector3D transformedPos = transform * pos;
		vertex.position           = Vec4(transformedPos.x, transformedPos.y,
		                                 transformedPos.z, 1.0f);

		// 法線にも回転を適用（スケールは除外）
		aiMatrix3x3 normalMatrix(transform);
		if (mesh->mNormals) {
			aiVector3D normal(mesh->mNormals[i].x, mesh->mNormals[i].y,
			                  mesh->mNormals[i].z);
			aiVector3D transformedNormal = normalMatrix * normal;
			transformedNormal.Normalize();
			vertex.normal = Vec3(transformedNormal.x, transformedNormal.y,
			                     transformedNormal.z);
		} else {
			vertex.normal = Vec3(0.0f, 1.0f, 0.0f);
		}

		if (mesh->mTextureCoords[0]) {
			vertex.uv = Vec2(mesh->mTextureCoords[0][i].x,
			                 mesh->mTextureCoords[0][i].y);
		} else {
			vertex.uv = Vec2::zero;
		}

		// ボーンウェイトとインデックスを設定
		const VertexBoneData& boneData = vertexBoneData[i];
		vertex.boneWeights = Vec4(boneData.weights[0], boneData.weights[1],
		                          boneData.weights[2], boneData.weights[3]);
		vertex.boneIndices[0] = boneData.indices[0];
		vertex.boneIndices[1] = boneData.indices[1];
		vertex.boneIndices[2] = boneData.indices[2];
		vertex.boneIndices[3] = boneData.indices[3];

		// ウェイトの正規化
		float totalWeight = vertex.boneWeights.x + vertex.boneWeights.y +
			vertex.boneWeights.z + vertex.boneWeights.w;
		if (totalWeight > 0.0f) {
			vertex.boneWeights = vertex.boneWeights / totalWeight;
		} else {
			// ボーンの影響がない頂点はルートボーン（インデックス0）に完全に紐づけ
			vertex.boneWeights    = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
			vertex.boneIndices[0] = 0;
			vertex.boneIndices[1] = 0;
			vertex.boneIndices[2] = 0;
			vertex.boneIndices[3] = 0;
		}

		vertices.emplace_back(vertex);

		// 最初の数頂点のボーン情報をデバッグ出力
		if (i < 5) {
			Console::Print(
				std::format(
					"頂点{}: ウェイト[{:.3f}, {:.3f}, {:.3f}, {:.3f}] インデックス[{}, {}, {}, {}]\n",
					i, vertex.boneWeights.x, vertex.boneWeights.y,
					vertex.boneWeights.z, vertex.boneWeights.w,
					vertex.boneIndices[0], vertex.boneIndices[1],
					vertex.boneIndices[2], vertex.boneIndices[3]),
				kConTextColorGray,
				Channel::ResourceSystem
			);
		}
	}

	Console::Print("スケルタルメッシュ頂点数: " + std::to_string(mesh->mNumVertices) + "\n",
	               kConTextColorGray, Channel::ResourceSystem);

	// 面情報の取得
	for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
		const aiFace face = mesh->mFaces[faceIndex];
		if (face.mNumIndices == 3) {
			for (uint32_t j = 0; j < face.mNumIndices; ++j) {
				indices.emplace_back(face.mIndices[j]);
			}
		}
	}

	Console::Print("スケルタルメッシュ三角面数: " + std::to_string(mesh->mNumFaces) + "\n",
	               kConTextColorGray, Channel::ResourceSystem);

	// マテリアルの設定
	Material*         material     = nullptr;
	const aiMaterial* aiMat        = scene->mMaterials[mesh->mMaterialIndex];
	const std::string materialName = aiMat->GetName().C_Str();

	// メッシュ名を取得（ファイルパスからファイル名のみを抽出）
	std::string meshName  = skeletalMesh->GetName();
	size_t      lastSlash = meshName.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		meshName = meshName.substr(lastSlash + 1);
	}
	// 拡張子を削除
	size_t lastDot = meshName.find_last_of('.');
	if (lastDot != std::string::npos) {
		meshName = meshName.substr(0, lastDot);
	}

	// マテリアルの作成またはキャッシュから取得（メッシュ名を含む一意キーで管理）
	material = mMaterialManager->GetOrCreateMaterial(
		materialName,
		DefaultShader::CreateDefaultSkinnedShader(mShaderManager),
		meshName
	);

	Console::Print(
		std::format(
			"MeshManager: スケルタルメッシュマテリアル割り当て - メッシュ: {}, マテリアル: {} (フルネーム: {})\n",
			meshName, materialName, material->GetFullName()),
		kConTextColorCompleted,
		Channel::ResourceSystem
	);

	// テクスチャの設定
	aiString texturePath;
	if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) ==
		AI_SUCCESS) {
		// モデルのディレクトリパスを取得
		std::filesystem::path modelPath(skeletalMesh->GetName());
		std::filesystem::path modelDir = modelPath.parent_path();

		// texturePath.C_Str() の値からテクスチャパスを作成
		const char* texturePathStr = texturePath.C_Str();
		if (texturePathStr && strlen(texturePathStr) > 0) {
			// テクスチャパスを結合して完全なパスを作成
			std::filesystem::path texPath(texturePathStr);
			std::filesystem::path fullTexturePath = modelDir / texPath;

			TexManager::GetInstance()->
				LoadTexture(fullTexturePath.string());
			material->SetTexture("gBaseColorTexture",
			                     fullTexturePath.string());
		}
	} else {
		// エラーテクスチャのパスを設定（必要に応じて実装）
		material->SetTexture("gBaseColorTexture",
		                     "./resources/textures/uvChecker.png");
	}

	// 環境マップテクスチャの設定
	TexManager::GetInstance()->LoadTexture("./resources/textures/wave.dds",
	                                       true);
	material->SetTexture(
		"gEnvironmentTexture",
		"./resources/textures/wave.dds"
	);

	auto subMesh = std::make_unique<SubMesh>(mDevice, mesh->mName.C_Str());
	subMesh->SetSkinnedVertexBuffer(vertices);
	subMesh->SetIndexBuffer(indices);
	if (material) {
		subMesh->SetMaterial(material);
	}
	return subMesh.release();
}

Skeleton MeshManager::LoadSkeleton(const aiScene* scene) {
	Skeleton skeleton;

	if (!scene->HasMeshes()) {
		Console::Print("メッシュがありません\n", kConTextColorError,
		               Channel::ResourceSystem);
		return skeleton;
	}

	// すべてのメッシュからボーン情報を収集
	std::map<std::string, int> boneIndexMap;
	int                        boneIndex = 0;

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		const aiMesh* mesh = scene->mMeshes[meshIndex];

		if (!mesh->HasBones()) {
			continue;
		}

		for (uint32_t i = 0; i < mesh->mNumBones; ++i) {
			const aiBone* bone     = mesh->mBones[i];
			std::string   boneName = bone->mName.C_Str();

			// 既に処理されたボーンかチェック
			if (!boneIndexMap.contains(boneName)) {
				Bone newBone;
				newBone.name = boneName;
				newBone.id   = boneIndex;

				// Assimpの行列は列優先なので、転置して行優先に変換する
				const aiMatrix4x4& offsetMatrix = bone->mOffsetMatrix;

				// TODO: Transpose関数で良くね?
				for (int row = 0; row < 4; ++row) {
					for (int col = 0; col < 4; ++col) {
						// 転置して格納（列優先→行優先）
						newBone.offsetMatrix.m[row][col] =
							offsetMatrix[col][row];
					}
				}

				skeleton.bones.emplace_back(newBone);
				skeleton.boneMap[boneName] = boneIndex;
				boneIndexMap[boneName]     = boneIndex;

				// 最初の数ボーンの情報をデバッグ出力
				if (boneIndex < 3) {
					Console::Print(
						std::format("ボーン{}: {} (ID: {})\n", boneIndex, boneName,
						            newBone.id),
						kConTextColorGray,
						Channel::ResourceSystem
					);
				}

				boneIndex++;
			}
		}
	}

	// ボーン変換行列の初期化
	skeleton.boneMatrices.resize(skeleton.bones.size(), Mat4::identity);

	// ルートノードを読み込み
	if (scene->mRootNode) {
		skeleton.rootNode = LoadNode(scene->mRootNode);
	}

	Console::Print(
		"スケルトン読み込み完了: " + std::to_string(skeleton.bones.size()) + " ボーン\n",
		kConTextColorCompleted, Channel::ResourceSystem);

	return skeleton;
}

Node MeshManager::LoadNode(const aiNode* aiNode) {
	Node node;
	node.name = aiNode->mName.C_Str();

	// 変換行列を設定
	const aiMatrix4x4& transform = aiNode->mTransformation;

	// TODO: Transpose関数で良くね?
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			// オフセット行列と同様に転置して格納（列優先→行優先）
			node.localMat.m[row][col] = transform[col][row];
		}
	}

	// node.localMat = node.localMat.Transpose();

	aiVector3D   translation, scaling;
	aiQuaternion rotation;
	transform.Decompose(scaling, rotation, translation);

	node.transform.translate =
		Vec3(translation.x, translation.y, translation.z);
	node.transform.rotate = Quaternion(
		rotation.x,
		rotation.y,
		rotation.z,
		rotation.w
	);
	node.transform.scale = Vec3(scaling.x, scaling.y, scaling.z);

	// 再起
	for (uint32_t i = 0; i < aiNode->mNumChildren; ++i) {
		node.children.emplace_back(LoadNode(aiNode->mChildren[i]));
	}

	return node;
}

void MeshManager::LoadAnimations(const aiScene* scene,
                                 SkeletalMesh*  skeletalMesh) {
	for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
		const aiAnimation* aiAnim    = scene->mAnimations[i];
		Animation          animation = LoadAnimation(aiAnim);

		std::string animName = aiAnim->mName.C_Str();
		if (animName.empty()) {
			animName = "Animation_" + std::to_string(i);
		}

		skeletalMesh->AddAnimation(animName, animation);

		Console::Print("アニメーション読み込み: " + animName +
		               " (時間: " + std::to_string(animation.duration) + "秒)\n",
		               kConTextColorCompleted, Channel::ResourceSystem);
	}
}

Animation MeshManager::LoadAnimation(const aiAnimation* aiAnim) {
	Animation animation;
	animation.duration = static_cast<float>(aiAnim->mDuration / aiAnim->
		mTicksPerSecond);

	// 各ノードのアニメーションチャンネルを処理
	for (uint32_t i = 0; i < aiAnim->mNumChannels; ++i) {
		const aiNodeAnim* nodeAnim = aiAnim->mChannels[i];
		std::string       nodeName = nodeAnim->mNodeName.C_Str();

		NodeAnimation nodeAnimation;

		// 位置
		for (uint32_t j = 0; j < nodeAnim->mNumPositionKeys; ++j) {
			const aiVectorKey& key = nodeAnim->mPositionKeys[j];
			KeyframeVec3       keyframe;
			keyframe.time = static_cast<float>(key.mTime / aiAnim->
				mTicksPerSecond);
			keyframe.value = Vec3(key.mValue.x, key.mValue.y, key.mValue.z);
			nodeAnimation.translate.keyFrames.emplace_back(keyframe);
		}

		// 回転
		for (uint32_t j = 0; j < nodeAnim->mNumRotationKeys; ++j) {
			const aiQuatKey&   key = nodeAnim->mRotationKeys[j];
			KeyframeQuaternion keyframe;
			keyframe.time = static_cast<float>(key.mTime / aiAnim->
				mTicksPerSecond);
			// 回転キーフレーム（デバッグ表示と同じ変換）
			keyframe.value = Quaternion(
				key.mValue.x,
				key.mValue.y,
				key.mValue.z,
				key.mValue.w
			);
			nodeAnimation.rotate.keyFrames.emplace_back(keyframe);
		}

		// スケール
		for (uint32_t j = 0; j < nodeAnim->mNumScalingKeys; ++j) {
			const aiVectorKey& key = nodeAnim->mScalingKeys[j];
			KeyframeVec3       keyframe;
			keyframe.time = static_cast<float>(key.mTime / aiAnim->
				mTicksPerSecond);
			keyframe.value = Vec3(key.mValue.x, key.mValue.y, key.mValue.z);
			nodeAnimation.scale.keyFrames.emplace_back(keyframe);
		}

		animation.nodeAnimations[nodeName] = nodeAnimation;
		animation.nodeNames.emplace_back(nodeName);
	}

	return animation;
}
