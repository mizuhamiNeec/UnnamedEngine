#pragma once
#include <cstdint>
#include <vector>

#include <engine/uprimitive/UPrimitives.h>

namespace Unnamed {
	/// @brief メッシュのサブメッシュ情報
	struct MeshSubmesh {
		uint32_t indexOffset   = 0;
		uint32_t indexCount    = 0;
		uint32_t materialIndex = 0;
		AABB     aabb;
	};

	/// @brief メッシュのスキニング情報
	struct MeshSkin {
		std::vector<std::string> jointNames;
		std::vector<Mat4>        invBind;

		std::vector<std::array<uint16_t, 4>> joints;
		std::vector<std::array<float, 4>>    weights;
	};

	/// @brief メッシュのモーフターゲット情報
	struct MeshMorphTarget {
		std::string name;

		std::vector<Vec3> positions;
		std::vector<Vec3> normals;
	};

	/// @brief メッシュアセットのデータ構造体
	struct MeshAssetData {
		std::vector<uint8_t> bytes;

		std::vector<Vec3> positions;
		std::vector<Vec3> normals;
		std::vector<Vec4> tangents;
		std::vector<Vec2> uv0;
		std::vector<Vec4> color0;

		std::vector<uint32_t>    indices;
		std::vector<MeshSubmesh> submeshes;
		AABB                     meshBounds;

		bool     hasSkin = false;
		MeshSkin skin;

		bool                         hasMorphTarget = false;
		std::vector<MeshMorphTarget> morphTargets;

		std::string sourcePath;
	};
}
