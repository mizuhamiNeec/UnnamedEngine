#include "MeshAssetLoader.h"
#include "core/assets/FileStamp.h"
#include "core/filesystem/Path.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "core/assets/types/MeshAssetData.h"
#include "core/hash/HashBuilder.h"
#include "core/hash/StableHashBuilder.h"
#include "core/io/binary/BinaryReader.h"
#include "core/io/binary/BinaryWriter.h"

#include "core/string/StrUtil.h"

#include "engine/profiler/Profiler.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel          = "MeshAssetLdr";
		constexpr uint32_t         kMeshCacheMagic   = 0x48534D55; // UMSH
		constexpr uint32_t         kMeshCacheVersion = 5;

		constexpr std::array kSupportedExtensions = {
			".obj",
			".gltf",
			".glb",
			// FBXはAssimpビルドに含めると重たいので非対応! Gitに上げられない!
		};

		/// @brief バイナリなメッシュキャッシュファイルのヘッダ構造体。
		struct MeshCacheHeader {
			uint32_t magic                = kMeshCacheMagic;
			uint32_t version              = kMeshCacheVersion;
			uint64_t sourcePathHash       = 0;
			int64_t  sourceLastWriteTicks = 0;
			uint64_t sourceSizeBytes      = 0;
			uint64_t importerConfigHash   = 0;
			uint32_t vertexCount          = 0;
			uint32_t indexCount           = 0;
			uint32_t submeshCount         = 0;
			uint32_t skeletonBoneCount    = 0;
			uint32_t animationClipCount   = 0;
			uint32_t flags                = 0;
		};

		/// @brief Assimpのインポート設定を表すハッシュ値を生成する。スキンニングの有無によって、頂点処理のフラット化（PreTransformVertices）を切り替えるため、スキンニングの有無も考慮する。
		/// @param hasSkinning スキンニングがあるか?
		/// @return インポート設定を表すハッシュ値
		uint64_t BuildImporterConfigHash(const bool hasSkinning) {
			constexpr uint64_t kBaseFlags =
				aiProcess_JoinIdenticalVertices | // 重複頂点の結合
				aiProcess_ImproveCacheLocality | // 頂点キャッシュ最適化
				aiProcess_CalcTangentSpace | // NormalMap 用 tangent/bitangent 生成
				aiProcess_ConvertToLeftHanded; // このエンジンは左手座標系

			StableHashBuilder hashBuilder(kBaseFlags);

			hashBuilder.AddUInt64(kMeshCacheVersion);

			if (!hasSkinning) {
				hashBuilder.AddUInt64(
					aiProcess_PreTransformVertices
				);
			}
			return hashBuilder.Value();
		}

		/// @brief Assimpの4x4行列をエンジンのMat4に変換する。Assimpは行優先、エンジンは列優先なので、要素の入れ替えに注意。
		/// @param m 変換するAssimpの行列
		/// @return 変換されたMat4
		Mat4 ToMat4(const aiMatrix4x4& m) {
			Mat4 out = Mat4::identity;
			out      = {
				{m.a1, m.b1, m.c1, m.d1}, // 1列目				
				{m.a2, m.b2, m.c2, m.d2}, // 2列目
				{m.a3, m.b3, m.c3, m.d3}, // 3列目
				{m.a4, m.b4, m.c4, m.d4}  // 4列目
			};
			return out;
		}

		Vec3 ToVec3(const aiVector3D& v) {
			return {v.x, v.y, v.z};
		}

		[[nodiscard]] bool IsFiniteVec3(const Vec3& v) {
			return std::isfinite(v.x) && std::isfinite(v.y) &&
			       std::isfinite(v.z);
		}

		[[nodiscard]] Vec3 SafeNormalizeOr(
			const Vec3& value, const Vec3& fallback
		) {
			if (!IsFiniteVec3(value) || value.SqrLength() <= 1.0e-8f) {
				return fallback;
			}
			return value.Normalized();
		}

		[[nodiscard]] Vec3 OrthonormalizeTangent(
			const Vec3& tangent, const Vec3& normal
		) {
			const Vec3 n = SafeNormalizeOr(normal, Vec3::up);
			Vec3       t = tangent - n * tangent.Dot(n);
			if (!IsFiniteVec3(t) || t.SqrLength() <= 1.0e-8f) {
				const Vec3 fallbackAxis = std::abs(n.y) < 0.999f ?
					                          Vec3::up :
					                          Vec3::right;
				t = fallbackAxis - n * fallbackAxis.Dot(n);
			}
			return SafeNormalizeOr(t, Vec3::right);
		}

		[[nodiscard]] Vec4 BuildTangentWithHandedness(
			const Vec3& normal, const Vec3& tangent, const Vec3& bitangent
		) {
			const Vec3  n    = SafeNormalizeOr(normal, Vec3::up);
			const Vec3  t    = OrthonormalizeTangent(tangent, n);
			const Vec3  b    = SafeNormalizeOr(bitangent, n.Cross(t));
			const float sign = n.Cross(t).Dot(b) < 0.0f ? -1.0f : 1.0f;
			return {t, sign};
		}

		Quaternion ToQuaternion(const aiQuaternion& q) {
			return Quaternion(q.x, q.y, q.z, q.w).Normalized();
		}

		void BuildNodeLookup(
			const aiNode*                                   node,
			std::unordered_map<std::string, const aiNode*>& outLookup
		) {
			if (!node) {
				return;
			}

			outLookup.emplace(node->mName.C_Str(), node);
			for (uint32_t i = 0; i < node->mNumChildren; ++i) {
				BuildNodeLookup(node->mChildren[i], outLookup);
			}
		}

		void BuildSkeletonHierarchy(
			const aiScene*                                   scene,
			const std::unordered_map<std::string, uint16_t>& boneNameToIndex,
			MeshAssetData&                                   out
		) {
			if (!scene || !scene->mRootNode || out.skeleton.empty()) {
				return;
			}

			std::unordered_map<std::string, const aiNode*> nodeLookup;
			nodeLookup.reserve(boneNameToIndex.size() * 2);
			BuildNodeLookup(scene->mRootNode, nodeLookup);

			for (auto& bone : out.skeleton) {
				const auto nodeIt = nodeLookup.find(bone.name);
				if (nodeIt == nodeLookup.end() || !nodeIt->second) {
					continue;
				}

				const aiNode* node = nodeIt->second;
				aiVector3D    scaling(1.0f, 1.0f, 1.0f);
				aiQuaternion  rotation;
				aiVector3D    translation(0.0f, 0.0f, 0.0f);
				node->mTransformation.Decompose(scaling, rotation, translation);

				bone.bindLocalTranslation = ToVec3(translation);
				bone.bindLocalRotation    = ToQuaternion(rotation);
				bone.bindLocalScale       = ToVec3(scaling);

				bone.parentIndex     = -1;
				const aiNode* parent = node->mParent;
				while (parent) {
					const auto parentIt = boneNameToIndex.find(
						parent->mName.C_Str()
					);
					if (parentIt != boneNameToIndex.end()) {
						bone.parentIndex = parentIt->second;
						break;
					}
					parent = parent->mParent;
				}
			}
		}

		template <typename T>
		float LastKeyTimeSeconds(const std::vector<T>& keys) {
			if (keys.empty()) {
				return 0.0f;
			}
			return keys.back().timeSeconds;
		}

		void SortTrackKeys(SkeletonBoneTrackAssetData& track) {
			std::ranges::sort(
				track.translationKeys,
				[](
				const AnimationKeyVec3AssetData& lhs,
				const AnimationKeyVec3AssetData& rhs
			) {
					return lhs.timeSeconds < rhs.timeSeconds;
				}
			);
			std::ranges::sort(
				track.rotationKeys,
				[](
				const AnimationKeyQuatAssetData& lhs,
				const AnimationKeyQuatAssetData& rhs
			) {
					return lhs.timeSeconds < rhs.timeSeconds;
				}
			);
			std::ranges::sort(
				track.scaleKeys,
				[](
				const AnimationKeyVec3AssetData& lhs,
				const AnimationKeyVec3AssetData& rhs
			) {
					return lhs.timeSeconds < rhs.timeSeconds;
				}
			);
		}

		void ExtractAnimationClips(
			const aiScene*                                   scene,
			const std::unordered_map<std::string, uint16_t>& boneNameToIndex,
			MeshAssetData&                                   out
		) {
			if (!scene || scene->mNumAnimations == 0) {
				return;
			}

			out.animationClips.reserve(scene->mNumAnimations);
			for (uint32_t animIndex = 0; animIndex < scene->mNumAnimations;
			     ++animIndex) {
				const aiAnimation* anim = scene->mAnimations[animIndex];
				if (!anim) {
					continue;
				}

				const double ticksPerSecond = anim->mTicksPerSecond > 0.0 ?
					                              anim->mTicksPerSecond :
					                              25.0;
				AnimationClipAssetData clip = {};
				clip.name                   = anim->mName.length > 0 ?
					            std::string(
						            anim->mName.C_Str()) :
					            "Anim" + std::to_string(
						            animIndex);
				clip.durationSeconds = anim->mDuration > 0.0 ?
					                       static_cast<float>(
						                       anim->mDuration /
						                       ticksPerSecond
					                       ) :
					                       0.0f;

				for (uint32_t ch = 0; ch < anim->mNumChannels; ++ch) {
					const aiNodeAnim* channel = anim->mChannels[ch];
					if (!channel) {
						continue;
					}

					const auto boneIt = boneNameToIndex.find(
						channel->mNodeName.C_Str()
					);
					if (boneIt == boneNameToIndex.end()) {
						continue;
					}

					SkeletonBoneTrackAssetData track = {};
					track.boneIndex                  = boneIt->second;
					track.translationKeys.reserve(channel->mNumPositionKeys);
					track.rotationKeys.reserve(channel->mNumRotationKeys);
					track.scaleKeys.reserve(channel->mNumScalingKeys);

					for (uint32_t i = 0; i < channel->mNumPositionKeys; ++i) {
						const aiVectorKey& key = channel->mPositionKeys[i];
						track.translationKeys.push_back(
							{
								.timeSeconds = static_cast<float>(
									key.mTime / ticksPerSecond
								),
								.value = ToVec3(key.mValue)
							}
						);
					}

					for (uint32_t i = 0; i < channel->mNumRotationKeys; ++i) {
						const aiQuatKey& key = channel->mRotationKeys[i];
						track.rotationKeys.push_back(
							{
								.timeSeconds = static_cast<float>(
									key.mTime / ticksPerSecond
								),
								.value = ToQuaternion(key.mValue)
							}
						);
					}

					for (uint32_t i = 0; i < channel->mNumScalingKeys; ++i) {
						const aiVectorKey& key = channel->mScalingKeys[i];
						track.scaleKeys.push_back(
							{
								.timeSeconds = static_cast<float>(
									key.mTime / ticksPerSecond
								),
								.value = ToVec3(key.mValue)
							}
						);
					}

					SortTrackKeys(track);
					clip.durationSeconds = std::max(
						clip.durationSeconds,
						std::max(
							LastKeyTimeSeconds(track.translationKeys),
							std::max(
								LastKeyTimeSeconds(track.rotationKeys),
								LastKeyTimeSeconds(track.scaleKeys)
							)
						)
					);
					clip.boneTracks.emplace_back(std::move(track));
				}

				if (!clip.boneTracks.empty()) {
					out.animationClips.emplace_back(std::move(clip));
				}
			}
		}

		/// @brief 頂点にボーンの影響を追加する。最大4本までのボーンをサポートし、5本以上の場合は最小重みのものを置き換える。
		/// @param v 影響を追加する頂点
		/// @param boneIndex 追加するボーンのインデックス
		/// @param weight 追加するボーンの重み
		void AddBoneWeight(
			MeshVertex& v, const uint16_t boneIndex, const float weight
		) {
			for (int i = 0; i < 4; ++i) {
				if (v.boneWeights[i] == 0.0f) {
					v.boneIndices[i] = boneIndex;
					v.boneWeights[i] = weight;
					return;
				}
			}

			// 4本超えた場合は最小重みを置き換える
			int minIndex = 0;
			for (int i = 1; i < 4; ++i) {
				if (v.boneWeights[i] < v.boneWeights[minIndex]) {
					minIndex = i;
				}
			}
			if (weight > v.boneWeights[minIndex]) {
				v.boneIndices[minIndex] = boneIndex;
				v.boneWeights[minIndex] = weight;
			}
		}

		/// @brief 頂点のボーンウェイトを正規化する。ウェイト和が0の場合は安全な既定値にフォールバックする。
		/// @param v 正規化する頂点
		void NormalizeBoneWeights(MeshVertex& v) {
			float weightSum = 0.0f;
			for (const float weight : v.boneWeights) {
				weightSum += weight;
			}

			if (weightSum <= 1e-6f) {
				v.boneIndices = {0, 0, 0, 0};
				v.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
				return;
			}

			const float invSum = 1.0f / weightSum;
			for (float& weight : v.boneWeights) {
				weight *= invSum;
			}
		}

		/// @brief スキン無しメッシュの頂点に、階層トランスフォームを焼き込む
		/// @param node 現在のノード
		/// @param parentTransform 親ノードからの累積トランスフォーム
		/// @param scene Assimpのシーンデータ
		void BakeNodeTransformsIntoMesh(
			const aiNode*      node,
			const aiMatrix4x4& parentTransform,
			const aiScene*     scene
		) {
			if (!node || !scene) {
				return;
			}

			const aiMatrix4x4 global = parentTransform * node->mTransformation;

			for (uint32_t mi = 0; mi < node->mNumMeshes; ++mi) {
				const uint32_t meshIndex = node->mMeshes[mi];
				if (meshIndex >= scene->mNumMeshes) {
					continue;
				}
				const aiMesh* mesh = scene->mMeshes[meshIndex];
				if (!mesh) {
					continue;
				}

				// スキン無しだけで呼ばれる想定だが、混在ファイルの安全のためボーン付きは触らない
				if (mesh->HasBones()) {
					continue;
				}

				// 位置
				for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
					mesh->mVertices[v] = global * mesh->mVertices[v];
				}

				// 法線（方向ベクトルとして変換）
				if (mesh->HasNormals()) {
					aiMatrix4x4 rotScale = global;
					rotScale.a4          = rotScale.b4 = rotScale.c4 = 0.0f;
					// translation 제거
					for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
						mesh->mNormals[v] = rotScale * mesh->mNormals[v];
						mesh->mNormals[v].Normalize();
					}
				}

				if (mesh->HasTangentsAndBitangents()) {
					aiMatrix4x4 rotScale = global;
					rotScale.a4          = rotScale.b4 = rotScale.c4 = 0.0f;
					for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
						mesh->mTangents[v] = rotScale * mesh->mTangents[v];
						mesh->mTangents[v].Normalize();
						mesh->mBitangents[v] = rotScale * mesh->mBitangents[v];
						mesh->mBitangents[v].Normalize();

						if (mesh->HasNormals()) {
							const Vec3 n = SafeNormalizeOr(
								ToVec3(mesh->mNormals[v]), Vec3::up
							);
							const Vec3 t = OrthonormalizeTangent(
								ToVec3(mesh->mTangents[v]), n
							);
							mesh->mTangents[v] = aiVector3D(t.x, t.y, t.z);
						}
					}
				}
			}

			for (uint32_t c = 0; c < node->mNumChildren; ++c) {
				BakeNodeTransformsIntoMesh(node->mChildren[c], global, scene);
			}
		}
	}

	bool MeshAssetLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		// 拡張子ベースで判定。厳密なファイル存在チェックはLoad()に任せる。
		const std::string ext =
			StrUtil::ToLowerCase(path.Extension().ToGenericUtf8());
		bool ok = false;

		for (const auto& supportedExt : kSupportedExtensions) {
			if (ext == supportedExt) {
				ok = true;
				break;
			}
		}

		if (outType) {
			*outType = ok ? ASSET_TYPE::MESH : ASSET_TYPE::UNKNOWN;
		}

		return ok;
	}

	LoadResult MeshAssetLoader::Load(const Path& path) {
		LoadResult r = {};
		if (TryLoadDerivedCache(path, r)) {
			return r;
		}

		Profiler*            profiler = ServiceLocator::Get<Profiler>();
		Profiler::ScopeTimer assimpScope(profiler, "MeshImport.Assimp");

		Assimp::Importer importer;

		// スキン無しメッシュは階層トランスフォームを焼き込んで頂点をフラット化することで、ランタイムの頂点処理を軽量化する。
		constexpr uint32_t kBaseFlags =
			aiProcess_ImproveCacheLocality |
			aiProcess_CalcTangentSpace |
			aiProcess_ConvertToLeftHanded;

		const aiScene* scene = importer.ReadFile(
			path.ToGenericUtf8(), kBaseFlags
		);

		if (!scene) {
			Error(
				kChannel, "メッシュの読み込みに失敗しました: path={}, err={}",
				path, importer.GetErrorString()
			);
			return r;
		}

		if (!scene->HasMeshes()) {
			Error(kChannel, "メッシュが含まれていません: {}", path);
			return r;
		}

		bool hasBones = false;
		for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
			if (scene->mMeshes[i] && scene->mMeshes[i]->HasBones()) {
				hasBones = true;
				break;
			}
		}

		if (!hasBones && scene->mRootNode) {
			BakeNodeTransformsIntoMesh(scene->mRootNode, aiMatrix4x4(), scene);
		}

		MeshAssetData out = {};
		out.sourcePath    = path.LexicallyNormal();
		out.hasSkinning   = hasBones;
		std::unordered_map<std::string, uint16_t> boneNameToIndex;

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++
		     meshIndex) {
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh || !(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) {
				continue;
			}

			const uint32_t baseVertex = static_cast<uint32_t>(out.vertices.
				size());
			const uint32_t submeshIndexStart = static_cast<uint32_t>(out.indices
				.
				size());
			out.vertices.reserve(
				out.vertices.size() + static_cast<size_t>(mesh->mNumVertices)
			);

			for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
				MeshVertex v{};
				v.position = Vec3(
					mesh->mVertices[i].x,
					mesh->mVertices[i].y,
					mesh->mVertices[i].z
				);
				v.normal = mesh->HasNormals() ?
					           Vec3(
						           mesh->mNormals[i].x,
						           mesh->mNormals[i].y,
						           mesh->mNormals[i].z
					           ) :
					           Vec3::up;
				v.uv = mesh->HasTextureCoords(0) ?
					       Vec2(
						       mesh->mTextureCoords[0][i].x,
						       mesh->mTextureCoords[0][i].y
					       ) :
					       Vec2::zero;
				if (mesh->HasTangentsAndBitangents()) {
					v.tangent = BuildTangentWithHandedness(
						v.normal,
						ToVec3(mesh->mTangents[i]),
						ToVec3(mesh->mBitangents[i])
					);
				} else {
					v.tangent = Vec4(
						OrthonormalizeTangent(Vec3::right, v.normal),
						1.0f
					);
				}

				out.localBoundsMin = Vec3::Min(out.localBoundsMin, v.position);
				out.localBoundsMax = Vec3::Max(out.localBoundsMax, v.position);

				out.vertices.emplace_back(v);
			}

			if (mesh->HasBones()) {
				for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; ++
				     boneIdx) {
					const aiBone* bone = mesh->mBones[boneIdx];
					if (!bone) {
						continue;
					}

					const std::string boneName        = bone->mName.C_Str();
					uint16_t          globalBoneIndex = 0;

					const auto it = boneNameToIndex.find(boneName);
					if (it == boneNameToIndex.end()) {
						globalBoneIndex = static_cast<uint16_t>(out.skeleton.
							size());
						boneNameToIndex.emplace(boneName, globalBoneIndex);

						SkeletonBoneAssetData sb = {};
						sb.name                  = boneName;
						sb.parentIndex           = -1;
						sb.inverseBindPose       = ToMat4(bone->mOffsetMatrix);
						out.skeleton.emplace_back(std::move(sb));
					} else {
						globalBoneIndex = it->second;
					}

					for (uint32_t w = 0; w < bone->mNumWeights; ++w) {
						const uint32_t localVertex = bone->mWeights[w].
							mVertexId;
						if (localVertex >= mesh->mNumVertices) {
							continue;
						}
						const uint32_t globalVertex = baseVertex + localVertex;
						if (globalVertex >= out.vertices.size()) {
							continue;
						}
						AddBoneWeight(
							out.vertices[globalVertex],
							globalBoneIndex,
							bone->mWeights[w].mWeight
						);
					}
				}

				for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
					const uint32_t globalVertex = baseVertex + i;
					if (globalVertex >= out.vertices.size()) {
						continue;
					}
					NormalizeBoneWeights(out.vertices[globalVertex]);
				}
			}

			out.indices.reserve(
				out.indices.size() + static_cast<size_t>(mesh->mNumFaces) * 3u
			);
			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++
			     faceIndex) {
				const aiFace& face = mesh->mFaces[faceIndex];
				if (face.mNumIndices < 3) {
					continue;
				}

				for (uint32_t i = 1; i + 1 < face.mNumIndices; ++i) {
					out.indices.emplace_back(baseVertex + face.mIndices[0]);
					out.indices.emplace_back(baseVertex + face.mIndices[i]);
					out.indices.emplace_back(baseVertex + face.mIndices[i + 1]);
				}
			}

			const uint32_t submeshIndexCount = static_cast<uint32_t>(
				out.indices.size() - submeshIndexStart
			);
			if (submeshIndexCount > 0) {
				SubMeshAssetData submesh = {};
				submesh.indexStart       = submeshIndexStart;
				submesh.indexCount       = submeshIndexCount;
				submesh.materialIndex    = mesh->mMaterialIndex;
				out.submeshes.emplace_back(submesh);
			}
		}

		if (!out.skeleton.empty()) {
			BuildSkeletonHierarchy(scene, boneNameToIndex, out);
		}
		if (out.hasSkinning) {
			ExtractAnimationClips(scene, boneNameToIndex, out);
		}

		if (out.vertices.empty() || out.indices.empty()) {
			Error(kChannel, "有効な頂点/インデックスがありません: {}", path);
			return r;
		}

		r.payload     = std::move(out);
		r.resolveName = Path::ToUtf8String(path.FileName());
		r.stamp       = ReadCurrentFileStamp(path);
		WriteDerivedCache(path, r);

		return r;
	}

	Path MeshAssetLoader::GetDerivedCachePath(const Path& sourcePath) {
		const std::string normalized =
			sourcePath.LexicallyNormal().ToGenericUtf8();
		const uint64_t hash = std::hash<std::string>{}(normalized);
		return Path("./bin/cache/assets/meshes") /
		       Path(std::to_string(hash) + ".umeshbin");
	}

	bool MeshAssetLoader::TryLoadDerivedCache(
		const Path& path, LoadResult& out
	) {
		const Path cachePath = GetDerivedCachePath(path);
		if (!std::filesystem::exists(cachePath.Native())) {
			return false;
		}

		BinaryReader reader(cachePath);
		if (!reader.IsOpen()) {
			return false;
		}

		MeshCacheHeader header = {};
		if (!reader.ReadPod(header)) {
			return false;
		}
		if (header.magic != kMeshCacheMagic || header.version !=
		    kMeshCacheVersion) {
			return false;
		}

		const std::string normalized  = path.LexicallyNormal().ToGenericUtf8();
		const FileStamp   sourceStamp = ReadCurrentFileStamp(path);
		if (header.sourcePathHash != std::hash<std::string>{}(normalized)) {
			return false;
		}
		if (
			header.sourceLastWriteTicks !=
			sourceStamp.lastWriteTicks ||
			header.sourceSizeBytes != sourceStamp.sizeInBytes
		) {
			return false;
		}

		const bool hasSkinning = (header.flags & 0x1u) != 0u;
		if (header.importerConfigHash != BuildImporterConfigHash(hasSkinning)) {
			return false;
		}

		MeshAssetData mesh = {};
		mesh.sourcePath    = path.LexicallyNormal();
		mesh.hasSkinning   = hasSkinning;
		mesh.vertices.resize(header.vertexCount);
		mesh.indices.resize(header.indexCount);
		mesh.submeshes.resize(header.submeshCount);
		mesh.skeleton.resize(header.skeletonBoneCount);

		if (!reader.ReadArray(mesh.vertices.data(), mesh.vertices.size()) ||
		    !reader.ReadArray(mesh.indices.data(), mesh.indices.size()) ||
		    !reader.ReadArray(mesh.submeshes.data(), mesh.submeshes.size())) {
			return false;
		}

		for (uint32_t i = 0; i < header.skeletonBoneCount; ++i) {
			if (!reader.ReadString(mesh.skeleton[i].name) ||
			    !reader.ReadPod(mesh.skeleton[i].parentIndex)) {
				return false;
			}
			if (!reader.ReadPod(mesh.skeleton[i].inverseBindPose)) {
				return false;
			}
			if (!reader.ReadPod(mesh.skeleton[i].bindLocalTranslation)) {
				return false;
			}
			if (!reader.ReadPod(mesh.skeleton[i].bindLocalRotation)) {
				return false;
			}
			if (!reader.ReadPod(mesh.skeleton[i].bindLocalScale)) {
				return false;
			}
		}

		mesh.animationClips.resize(header.animationClipCount);
		for (uint32_t i = 0; i < header.animationClipCount; ++i) {
			AnimationClipAssetData& clip = mesh.animationClips[i];

			if (!reader.ReadString(clip.name) ||
			    !reader.ReadPod(clip.durationSeconds)) {
				return false;
			}

			uint32_t trackCount = 0;
			if (!reader.ReadPod(trackCount)) {
				return false;
			}
			clip.boneTracks.resize(trackCount);
			for (uint32_t t = 0; t < trackCount; ++t) {
				SkeletonBoneTrackAssetData& track = clip.boneTracks[t];
				if (!reader.ReadPod(track.boneIndex)) {
					return false;
				}

				uint32_t translationCount = 0;
				uint32_t rotationCount    = 0;
				uint32_t scaleCount       = 0;
				if (!reader.ReadPod(translationCount) ||
				    !reader.ReadPod(rotationCount) ||
				    !reader.ReadPod(scaleCount)) {
					return false;
				}

				track.translationKeys.resize(translationCount);
				track.rotationKeys.resize(rotationCount);
				track.scaleKeys.resize(scaleCount);

				if (!reader.ReadArray(
					    track.translationKeys.data(),
					    track.translationKeys.size()
				    ) ||
				    !reader.ReadArray(
					    track.rotationKeys.data(),
					    track.rotationKeys.size()
				    ) ||
				    !reader.ReadArray(
					    track.scaleKeys.data(),
					    track.scaleKeys.size()
				    )) {
					return false;
				}
			}
		}

		for (const auto& vertex : mesh.vertices) {
			mesh.localBoundsMin = Vec3::Min(
				mesh.localBoundsMin, vertex.position
			);
			mesh.localBoundsMax = Vec3::Max(
				mesh.localBoundsMax, vertex.position
			);
		}

		out.payload     = std::move(mesh);
		out.resolveName = Path::ToUtf8String(path.FileName());
		out.stamp       = sourceStamp;

		if (Profiler* profiler = ServiceLocator::Get<Profiler>()) {
			profiler->AddSample("MeshImport.CacheRead", 1.0f);
		}
		return true;
	}

	bool MeshAssetLoader::WriteDerivedCache(
		const Path& path, const LoadResult& in
	) {
		const auto* mesh = std::get_if<MeshAssetData>(
			&const_cast<AssetPayload&>(in.payload)
		);
		if (!mesh) {
			return false;
		}

		const Path      cachePath = GetDerivedCachePath(path);
		std::error_code ec;
		std::filesystem::create_directories(
			cachePath.ParentPath().Native(), ec
		);

		BinaryWriter writer(cachePath);
		if (!writer.IsOpen()) {
			return false;
		}

		MeshCacheHeader header = {};
		header.sourcePathHash  = std::hash<std::string>{}(
			path.LexicallyNormal().ToGenericUtf8()
		);
		header.sourceLastWriteTicks = in.stamp.lastWriteTicks;
		header.sourceSizeBytes = in.stamp.sizeInBytes;
		header.importerConfigHash = BuildImporterConfigHash(mesh->hasSkinning);
		header.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
		header.indexCount = static_cast<uint32_t>(mesh->indices.size());
		header.submeshCount = static_cast<uint32_t>(mesh->submeshes.size());
		header.skeletonBoneCount = static_cast<uint32_t>(mesh->skeleton.size());
		header.animationClipCount = static_cast<uint32_t>(
			mesh->animationClips.size()
		);
		header.flags = mesh->hasSkinning ? 0x1u : 0u;

		if (!writer.WritePod(header)) {
			return false;
		}
		if (!writer.WriteArray(mesh->vertices.data(), mesh->vertices.size()) ||
		    !writer.WriteArray(mesh->indices.data(), mesh->indices.size()) ||
		    !writer.WriteArray(mesh->submeshes.data(),
		                       mesh->submeshes.size())) {
			return false;
		}

		for (const auto& bone : mesh->skeleton) {
			if (!writer.WriteString(bone.name) ||
			    !writer.WritePod(bone.parentIndex)) {
				return false;
			}
			if (!writer.WritePod(bone.inverseBindPose)) {
				return false;
			}
			if (!writer.WritePod(bone.bindLocalTranslation)) {
				return false;
			}
			if (!writer.WritePod(bone.bindLocalRotation)) {
				return false;
			}
			if (!writer.WritePod(bone.bindLocalScale)) {
				return false;
			}
		}

		for (const auto& clip : mesh->animationClips) {
			if (!writer.WriteString(clip.name) ||
			    !writer.WritePod(clip.durationSeconds)) {
				return false;
			}

			const uint32_t trackCount = static_cast<uint32_t>(
				clip.boneTracks.size()
			);
			if (!writer.WritePod(trackCount)) {
				return false;
			}

			for (const auto& track : clip.boneTracks) {
				if (!writer.WritePod(track.boneIndex)) {
					return false;
				}

				const uint32_t translationCount = static_cast<uint32_t>(
					track.translationKeys.size()
				);
				const uint32_t rotationCount = static_cast<uint32_t>(
					track.rotationKeys.size()
				);
				const uint32_t scaleCount = static_cast<uint32_t>(
					track.scaleKeys.size()
				);
				if (
					!writer.WritePod(translationCount) ||
					!writer.WritePod(rotationCount) ||
					!writer.WritePod(scaleCount)
				) {
					return false;
				}

				if (!writer.WriteArray(
					    track.translationKeys.data(),
					    track.translationKeys.size()
				    ) ||
				    !writer.WriteArray(
					    track.rotationKeys.data(),
					    track.rotationKeys.size()
				    ) ||
				    !writer.WriteArray(
					    track.scaleKeys.data(),
					    track.scaleKeys.size()
				    )) {
					return false;
				}
			}
		}

		if (Profiler* profiler = ServiceLocator::Get<Profiler>()) {
			profiler->AddSample("MeshImport.CacheWrite", 1.0f);
		}
		return writer.Flush();
	}
}
