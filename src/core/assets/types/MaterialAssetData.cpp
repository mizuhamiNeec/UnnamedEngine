#include "MaterialAssetData.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	MATERIAL_DOMAIN ParseMaterialDomain(const std::string_view text) {
		const auto value = StrUtil::ToLowerCase(text);
		if (value == "unlit") {
			return MATERIAL_DOMAIN::UNLIT;
		}
		return MATERIAL_DOMAIN::PBR_METAL_ROUGH;
	}

	MATERIAL_SHADING_MODEL ParseMaterialShadingModel(
		const std::string_view text
	) {
		const auto value = StrUtil::ToLowerCase(text);
		if (value == "toon" || value == "npbr") {
			return MATERIAL_SHADING_MODEL::TOON;
		}
		if (value == "unlit") {
			return MATERIAL_SHADING_MODEL::UNLIT;
		}
		if (value == "pbr" || value == "lit" || value == "litpbr") {
			return MATERIAL_SHADING_MODEL::LIT_PBR;
		}
		if (value == "lit_pbr" || value == "lit-pbr") {
			return MATERIAL_SHADING_MODEL::LIT_PBR;
		}
		return MATERIAL_SHADING_MODEL::LIT_PBR;
	}

	MATERIAL_SHADOW_CULL_MODE ParseMaterialShadowCullMode(
		const std::string_view text
	) {
		const auto value = StrUtil::ToLowerCase(text);
		if (value == "back" || value == "backface" || value == "cullback") {
			return MATERIAL_SHADOW_CULL_MODE::BACK;
		}
		if (value == "front" || value == "frontface" || value == "cullfront") {
			return MATERIAL_SHADOW_CULL_MODE::FRONT;
		}
		if (value == "none" || value == "off" || value == "double_sided") {
			return MATERIAL_SHADOW_CULL_MODE::NONE;
		}
		if (value == "doublesided" || value == "double-sided") {
			return MATERIAL_SHADOW_CULL_MODE::NONE;
		}
		if (value == "follow" || value == "follow_material") {
			return MATERIAL_SHADOW_CULL_MODE::FOLLOW_MATERIAL;
		}
		return MATERIAL_SHADOW_CULL_MODE::FOLLOW_MATERIAL;
	}
}
