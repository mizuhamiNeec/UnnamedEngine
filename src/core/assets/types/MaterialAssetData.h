#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief マテリアルのドメインを表す列挙型
	enum class MATERIAL_DOMAIN : uint8_t {
		UNLIT           = 0,
		PBR_METAL_ROUGH = 1,
	};

	/// @brief マテリアルのシェーディングモデル。
	enum class MATERIAL_SHADING_MODEL : uint8_t {
		LIT_PBR = 0,
		TOON    = 1,
		UNLIT   = 2,
	};

	/// @brief ShadowMap caster で使用するカリングモード。
	enum class MATERIAL_SHADOW_CULL_MODE : uint8_t {
		FOLLOW_MATERIAL = 0,
		BACK            = 1,
		FRONT           = 2,
		NONE            = 3,
	};

	/// @brief 文字列からマテリアルドメインを解析します。
	/// @param text 解析する文字列
	/// @return 解析結果。未定義文字列はPBR_METAL_ROUGHを返します。
	MATERIAL_DOMAIN ParseMaterialDomain(std::string_view text);

	/// @brief 文字列からマテリアルシェーディングモデルを解析します。
	/// @param text 解析する文字列
	/// @return 解析結果。未定義文字列はLIT_PBRを返します。
	MATERIAL_SHADING_MODEL ParseMaterialShadingModel(std::string_view text);

	/// @brief 文字列からShadowMap caster用カリングモードを解析します。
	/// @param text 解析する文字列
	/// @return 解析結果。未定義文字列はFOLLOW_MATERIALを返します。
	MATERIAL_SHADOW_CULL_MODE ParseMaterialShadowCullMode(
		std::string_view text
	);

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

		AssetID                shaderProgramId = kInvalidAssetID;
		VirtualPath            shaderProgramPath;
		MATERIAL_DOMAIN        domain       = MATERIAL_DOMAIN::PBR_METAL_ROUGH;
		MATERIAL_SHADING_MODEL shadingModel =
			MATERIAL_SHADING_MODEL::LIT_PBR;

		MaterialRenderStateData renderState = {};

		std::unordered_map<std::string, float> scalarParams;
		std::unordered_map<std::string, Vec4>  vectorParams;
	};
}
