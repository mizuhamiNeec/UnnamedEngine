#include <pch.h>

//-----------------------------------------------------------------------------

#include "MeshLoader.h"

#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <runtime/assets/types/MeshAsset.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "MeshLoader";

	namespace {
		uint32_t AppendVertexStreams(
			MeshAssetData& out, const aiMesh* m, const bool filipV
		) {
			const uint32_t base = static_cast<uint32_t>(out.positions.size());

			out.positions.reserve(base + m->mNumVertices);

			if (m->HasNormals()) {
				out.normals.reserve(base + m->mNumVertices);
			}
			if (m->HasTangentsAndBitangents()) {
				out.tangents.reserve(base + m->mNumVertices);
			}
			if (m->HasTextureCoords(0)) {
				out.uv0.reserve(base + m->mNumVertices);
			}
			if (m->HasVertexColors(0)) {
				out.color0.reserve(base + m->mNumVertices);
			}

			for (unsigned v = 0; v < m->mNumVertices; ++v) {
				const auto& p = m->mVertices[v];
				out.positions.emplace_back(p.x, p.y, p.z);
				out.meshBounds.Expand(out.positions.back());

				if (m->HasNormals()) {
					const auto& n = m->mNormals[v];
					out.normals.emplace_back(n.x, n.y, n.z);
				}
				if (m->HasTangentsAndBitangents()) {
					const auto& t = m->mTangents[v];
					out.tangents.emplace_back(t.x, t.y, t.z);
				}
				if (m->HasTextureCoords(0)) {
					const auto& uv = m->mTextureCoords[0][v];
					out.uv0.emplace_back(uv.x, filipV ? 1.0f - uv.y : uv.y);
				}
				if (m->HasVertexColors(0)) {
					const auto& c = m->mColors[0][v];
					out.color0.emplace_back(c.r, c.g, c.b);
				}
			}
			return base;
		}

		void AppendIndices(MeshAssetData& out, const aiMesh* m,
		                   uint32_t       baseVertex) {
			const uint32_t baseIndex = static_cast<uint32_t>(out.indices.
				size());
			uint32_t triCount = 0;

			out.indices.reserve(baseIndex + m->mNumFaces * 3);

			for (unsigned f = 0; f < m->mNumFaces; ++f) {
				const auto& face = m->mFaces[f];
				if (face.mNumIndices != 3) {
					UASSERT(false && "please triangulate the mesh.");
					continue;
				}
				out.indices.emplace_back(baseVertex + face.mIndices[0]);
				out.indices.emplace_back(baseVertex + face.mIndices[1]);
				out.indices.emplace_back(baseVertex + face.mIndices[2]);
				triCount += 1;
			}

			MeshSubmesh submesh   = {};
			submesh.indexOffset   = baseIndex;
			submesh.indexCount    = triCount * 3;
			submesh.materialIndex = m->mMaterialIndex;
			for (unsigned v = 0; v < m->mNumVertices; ++v) {
				submesh.aabb.Expand(out.positions[baseVertex + v]);
			}
			out.submeshes.emplace_back(submesh);
		}

		static void AccumulateSkinning(
			const aiMesh*                              m,
			std::unordered_map<std::string, uint32_t>& jointMap,
			MeshAssetData&                             out,
			uint32_t                                   baseVertex,
			int                                        maxWeights
		) {
			if (m->mNumBones == 0) { return; }

			out.hasSkin = true;

			const uint32_t vCount = m->mNumVertices;
			std::vector<std::vector<std::pair<uint32_t, float>>> accum(vCount);

			for (unsigned b = 0; b < m->mNumBones; ++b) {
				const aiBone*     bone = m->mBones[b];
				const std::string name = bone->mName.C_Str();
				uint32_t          jix;
				auto              it = jointMap.find(name);
				if (it == jointMap.end()) {
					jix            = static_cast<uint32_t>(jointMap.size());
					jointMap[name] = jix;
					out.skin.jointNames.emplace_back(name);

					// 行列を突っ込む
					auto& mat          = bone->mOffsetMatrix;
					Mat4  convertedMat = {
						{
							mat[0][0], mat[0][1], mat[0][2], mat[0][3],
							mat[1][0], mat[1][1], mat[1][2], mat[1][3],
							mat[2][0], mat[2][1], mat[2][2], mat[2][3],
							mat[3][0], mat[3][1], mat[3][2], mat[3][3]
						}
					};

					out.skin.invBind.emplace_back(
						convertedMat
					);
				} else {
					jix = it->second;
				}

				for (unsigned w = 0; w < bone->mNumWeights; ++w) {
					const aiVertexWeight& weight = bone->mWeights[w];
					if (static_cast<uint32_t>(weight.mWeight) >= vCount) {
						continue;
					}
					accum[weight.mVertexId].emplace_back(jix, weight.mWeight);
				}
			}

			// 配列確保
			if (out.skin.joints.size() < out.positions.size()) {
				out.skin.joints.resize(out.positions.size(), {0, 0, 0, 0});
				out.skin.weights.resize(out.positions.size(), {0, 0, 0, 0});
			}

			for (uint32_t v = 0; v < vCount; ++v) {
				auto& list = accum[v];
				if (list.empty()) { continue; }

				const int K = std::min(maxWeights,
				                       static_cast<int>(list.size()));
				std::ranges::partial_sort(
					list, list.begin() + K,
					[](auto& a, auto& b) {
						return a.second > b.second;
					}
				);

				float                   sum = 0.0f;
				std::array<uint16_t, 4> jix{0, 0, 0, 0};
				std::array<float, 4>    wgt{0.0f, 0.0f, 0.0f, 0.0f};
				for (int k = 0; k < K; ++k) {
					jix[k] = static_cast<unsigned short>(list[k].first);
					wgt[k] = list[k].second;
					sum += wgt[k];
				}
				if (sum > 0.0f) {
					for (int k = 0; k < K; ++k) {
						wgt[k] /= sum;
					}
				}

				out.skin.joints[baseVertex + v]  = jix;
				out.skin.weights[baseVertex + v] = wgt;
			}
		}

		static void AppendMorphTargets(
			const aiMesh*  m,
			MeshAssetData& out,
			uint32_t       baseVertex,
			float          eps
		) {
			if (m->mNumAnimMeshes == 0) { return; }

			out.hasMorphTarget = true;

			const uint32_t totalVerts = static_cast<uint32_t>(out.positions.
				size());

			for (unsigned mi = 0; mi < m->mNumAnimMeshes; ++mi) {
				const aiAnimMesh* am = m->mAnimMeshes[mi];

				MeshMorphTarget target = {};
				target.name            =
					am->mName.length > 0 ?
						am->mName.C_Str() :
						"Morph_" + std::to_string(mi);

				target.positions.resize(totalVerts, Vec3::zero);
				target.normals.resize(totalVerts, Vec3::zero);

				for (unsigned v = 0; v < m->mNumVertices; ++v) {
					const aiVector3D& bp       = m->mVertices[v];
					const aiVector3D& tp       = am->mVertices[v];
					Vec3              deltaPos = {
						tp.x - bp.x, tp.y - bp.y, tp.z - bp.z
					};
					if (
						abs(deltaPos.x) > eps ||
						abs(deltaPos.y) > eps ||
						abs(deltaPos.z) > eps
					) {
						target.positions[baseVertex + v] = deltaPos;
					}

					if (am->HasNormals() && m->HasNormals()) {
						const aiVector3D& bn        = m->mNormals[v];
						const aiVector3D& tn        = am->mNormals[v];
						Vec3              deltaNorm = {
							tn.x - bn.x, tn.y - bn.y, tn.z - bn.z
						};
						if (
							abs(deltaNorm.x) > eps ||
							abs(deltaNorm.y) > eps ||
							abs(deltaNorm.z) > eps
						) {
							target.normals[baseVertex + v] = deltaNorm;
						}
					}
				}
				out.morphTargets.emplace_back(std::move(target));
			}
		}
	}

	bool MeshLoader::CanLoad(
		const std::string_view path, UASSET_TYPE* outType
	) const {
		const std::string ext = StrUtil::ToLowerExt(path);
		const bool        ok  =
			ext == ".fbx" ||
			ext == ".obj" ||
			ext == ".gltf" ||
			ext == ".glb";

		if (outType) {
			*outType = ok ? UASSET_TYPE::MESH : UASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MeshLoader::Load(const std::string& path) {
		LoadResult    r   = {};
		MeshAssetData out = {};
		out.sourcePath    = path;

		unsigned flags = 0;
		flags |= aiProcess_Triangulate;              // 三角形化
		flags |= aiProcess_JoinIdenticalVertices;    // 重複頂点の結合
		flags |= aiProcess_ImproveCacheLocality;     // 頂点キャッシュの最適化
		flags |= aiProcess_SplitLargeMeshes;         // 大きなメッシュの分割
		flags |= aiProcess_SortByPType;              // プリミティブタイプでソート
		flags |= aiProcess_RemoveRedundantMaterials; // 冗長なマテリアルの削除
		flags |= aiProcess_GenSmoothNormals;         // スムースシェーディング用の法線生成
		flags |= aiProcess_CalcTangentSpace;         // 接線空間の計算
		flags |= aiProcess_ConvertToLeftHanded;      // 左手系に変換

		Assimp::Importer imp;
		const aiScene*   scene = imp.ReadFile(path, flags);
		if (!scene || scene->mNumMeshes == 0) {
			Error(
				kChannel,
				"Assimp failed: {}",
				imp.GetErrorString() ?
					imp.GetErrorString() :
					"unknown error."
			);
			return r;
		}

		std::unordered_map<std::string, uint32_t> jointDict;
		constexpr bool                            kFlipV      = false;
		constexpr int                             kMaxWeights = 4;
		constexpr float                           kMorphEps   = FLT_EPSILON;

		for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi) {
			const aiMesh*  m    = scene->mMeshes[mi];
			const uint32_t base = AppendVertexStreams(out, m, kFlipV);
			AppendIndices(out, m, base);
			AccumulateSkinning(m, jointDict, out, base, kMaxWeights);
			AppendMorphTargets(m, out, base, kMorphEps);
		}

		if (out.tangents.empty() && !out.normals.empty()) {
			out.tangents.resize(out.positions.size(), Vec4(1, 0, 0, 1));
		}
		if (out.uv0.size() < out.positions.size()) {
			out.uv0.resize(out.positions.size(), Vec2(0, 0));
		}
		if (out.color0.size() < out.positions.size()) {
			out.color0.resize(out.positions.size(), Vec4(1, 1, 1, 1));
		}
		if (out.hasSkin) {
			if (out.skin.joints.size() < out.positions.size()) {
				out.skin.joints.resize(out.positions.size(), {0, 0, 0, 0});
			}
			if (out.skin.weights.size() < out.positions.size()) {
				out.skin.weights.resize(out.positions.size(), {0, 0, 0, 0});
			}
		}

		r.payload     = std::move(out);
		r.resolveName = std::filesystem::path(path).filename().string();
		if (std::error_code ec; std::filesystem::exists(path, ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}
		return r;
	}
}
