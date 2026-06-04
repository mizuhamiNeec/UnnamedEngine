#pragma once
#include "core/math/Mat4.h"
#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

namespace Unnamed::Rhi {
	struct alignas(16) FrameConstants {
		Mat4  view        = Mat4::identity; // 64 +
		Mat4  proj        = Mat4::identity; // 64 +
		Mat4  viewProj    = Mat4::identity; // 64 +
		Vec3  cameraPos   = Vec3::zero;     // 16 +
		float time        = 0.0f;           // 4 +
		float padding[12] = {};             // 48 = 256
	};

	static_assert(
		sizeof(FrameConstants) % 16 == 0,
		"FrameConstants must be 16-byte aligned"
	);
	static_assert(
		sizeof(FrameConstants) <= 256,
		"FrameConstants must be 256 bytes or less"
	);

	struct alignas(16) ObjectConstants {
		Mat4 world = Mat4::identity; // 64 +
		Mat4 worldInverseTranspose = Mat4::identity; // 64 = 128
		Vec4 skinningInfo = Vec4::zero; // x=paletteOffset, y=useSkinning
	};

	static_assert(
		sizeof(ObjectConstants) % 16 == 0,
		"ObjectConstants must be 16-byte aligned"
	);

	static_assert(
		sizeof(ObjectConstants) <= 256,
		"ObjectConstants must be 256 bytes or less"
	);

	struct alignas(16) SkinningPaletteConstants {
		static constexpr uint32_t kMaxBones        = 512;
		Mat4                      bones[kMaxBones] = {};
	};

	static_assert(
		sizeof(SkinningPaletteConstants) <= 64 * 1024,
		"SkinningPaletteConstants must fit in one constant buffer (64KB)"
	);

	struct alignas(16) MaterialConstants {
		Vec4  baseColor     = Vec4::one;
		Vec4  emissiveColor = Vec4::zero;
		float metallic      = 0.0f;
		float roughness     = 1.0f;
		float opacity       = 1.0f;
		float domainMode    = 1.0f; // 0=Unlit, 1=PBR
		float padding[12]   = {};
	};

	static_assert(
		sizeof(MaterialConstants) % 16 == 0,
		"MaterialConstants must be 16-byte aligned"
	);
	static_assert(
		sizeof(MaterialConstants) <= 256,
		"MaterialConstants must be 256 bytes or less"
	);

	struct alignas(16) ShadowConstants {
		Mat4 lightViewProj = Mat4::identity;
		Vec4 params        = Vec4::zero; // x=depthBias, y=strength, z=texelSize, w=enabled
	};

	static_assert(
		sizeof(ShadowConstants) % 16 == 0,
		"ShadowConstants must be 16-byte aligned"
	);
	static_assert(
		sizeof(ShadowConstants) <= 256,
		"ShadowConstants must be 256 bytes or less"
	);
}
