#pragma once
#include <cstdint>
#include <vector>

#include <engine/uprimitive/UPrimitives.h>

namespace Unnamed {
	struct MeshSubmesh {
		uint32_t indexOffset   = 0;
		uint32_t indexCount    = 0;
		uint32_t materialIndex = 0;
		AABB     aabb;
	};

	struct MeshSkin {
		std::vector<std::string> jointNames;
		std::vector<Mat4>        invBind;

		std::vector<std::array<uint16_t, 4>> joints;
		std::vector<std::array<float, 4>>    weights;
	};

	struct MeshMorphTarget {
		std::string name;

		std::vector<Vec3> positions;
		std::vector<Vec3> normals;
	};

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
