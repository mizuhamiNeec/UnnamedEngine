#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/assets/AssetID.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief マテリアルのドメインを表す列挙型
	enum class MATERIAL_DOMAIN : uint8_t {
		UNLIT           = 0,
		PBR_METAL_ROUGH = 1,
	};

	/// @brief ShadowMap caster で使用するカリングモード。
	enum class MATERIAL_SHADOW_CULL_MODE : uint8_t {
		FOLLOW_MATERIAL = 0,
		BACK            = 1,
		FRONT           = 2,
		NONE            = 3,
	};

	/// @brief マテリアルの描画状態を表す構造体
	struct MaterialRenderStateData {
		bool                      depthEnable    = true;
		bool                      depthWrite     = true;
		bool                      cullBackFace   = true;
		bool                      blendEnable    = false;
		bool                      castsShadow    = true;
		MATERIAL_SHADOW_CULL_MODE shadowCullMode =
			MATERIAL_SHADOW_CULL_MODE::FOLLOW_MATERIAL;

		bool    stencilEnable    = false;
		uint8_t stencilReadMask  = 0xFF;
		uint8_t stencilWriteMask = 0xFF;
	};

	/// @brief マテリアルアセットのデータ構造体
	struct MaterialAssetData {
		std::string name;

		AssetID         shaderProgramId = kInvalidAssetID;
		std::string     shaderProgramPath;
		MATERIAL_DOMAIN domain = MATERIAL_DOMAIN::PBR_METAL_ROUGH;

		MaterialRenderStateData renderState = {};

		std::unordered_map<std::string, float> scalarParams;
		std::unordered_map<std::string, Vec4>  vectorParams;
	};
}
