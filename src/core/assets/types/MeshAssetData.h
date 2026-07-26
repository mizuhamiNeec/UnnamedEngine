#pragma once
#include <array>
#include <cfloat>
#include <cstdint>
#include <string>
#include <vector>

#include "core/filesystem/Path.h"
#include "core/math/Mat4.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief メッシュの頂点データ構造体
	struct MeshVertex {
		Vec3                    position    = Vec3::zero;
		Vec3                    normal      = Vec3::up;
		Vec2                    uv          = Vec2::zero;
		Vec4                    tangent     = Vec4(Vec3::right, 1.0f);
		std::array<uint16_t, 4> boneIndices = {0, 0, 0, 0};
		std::array<float, 4>    boneWeights = {0.0f, 0.0f, 0.0f, 0.0f};
	};

	/// @brief スケルトンのボーンデータ構造体
	struct SkeletonBoneAssetData {
		std::string name;
		int32_t     parentIndex          = -1;
		Mat4        inverseBindPose      = Mat4::identity;
		Vec3        bindLocalTranslation = Vec3::zero;
		Quaternion  bindLocalRotation    = Quaternion::identity;
		Vec3        bindLocalScale       = Vec3::one;
	};

	/// @brief AnimationKeyVec3AssetDataは、AnimationKeyVec3Asset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します
	struct AnimationKeyVec3AssetData {
		float timeSeconds = 0.0f;
		Vec3  value       = Vec3::zero;
	};

	/// @brief AnimationKeyQuatAssetDataは、AnimationKeyQuatAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します
	struct AnimationKeyQuatAssetData {
		float      timeSeconds = 0.0f;
		Quaternion value       = Quaternion::identity;
	};

	/// @brief SkeletonBoneTrackAssetDataは、SkeletonBoneTrackAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します
	struct SkeletonBoneTrackAssetData {
		int32_t                                boneIndex = -1;
		std::vector<AnimationKeyVec3AssetData> translationKeys;
		std::vector<AnimationKeyQuatAssetData> rotationKeys;
		std::vector<AnimationKeyVec3AssetData> scaleKeys;
	};

	/// @brief AnimationClipAssetDataは、AnimationClipAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します
	struct AnimationClipAssetData {
		std::string                             name;
		float                                   durationSeconds = 0.0f;
		std::vector<SkeletonBoneTrackAssetData> boneTracks;
	};

	/// @brief メッシュのサブメッシュデータ構造体
	struct SubMeshAssetData {
		uint32_t indexStart    = 0; // インデックスバッファ内の開始インデックス
		uint32_t indexCount    = 0; // このサブメッシュのインデックス数
		uint32_t materialIndex = 0; // Assimp のマテリアルインデックス
	};

	/// @brief メッシュアセットのデータ構造体
	struct MeshAssetData {
		std::vector<MeshVertex>             vertices;
		std::vector<uint32_t>               indices;
		std::vector<SubMeshAssetData>       submeshes; // サブメッシュ情報
		std::vector<SkeletonBoneAssetData>  skeleton;
		std::vector<AnimationClipAssetData> animationClips;
		bool                                hasSkinning = false;

		Vec3 localBoundsMin = Vec3(FLT_MAX);
		Vec3 localBoundsMax = Vec3(-FLT_MAX);

		Path sourcePath;
	};

	/// @brief メッシュに必要なマテリアルスロット数を返します。
	/// @param meshAsset 対象メッシュ
	/// @return サブメッシュの最大materialIndexから求めたスロット数
	uint32_t ComputeRequiredMaterialSlotCount(const MeshAssetData& meshAsset);
}
