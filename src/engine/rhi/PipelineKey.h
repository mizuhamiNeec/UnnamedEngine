#pragma once
#include <cstdint>
#include <vector>

namespace Unnamed::Rhi {
	enum class VertexSemantic : uint8_t {
		POSITION,
		NORMAL,
		TANGENT,
		TEXCOORD,
		COLOR
	};

	enum class VertexFormat : uint8_t {
		FLOAT1,
		FLOAT2,
		FLOAT3,
		FLOAT4,
		U_BYTE4_N,
	};

	/// @brief VertexElementDescは、1頂点属性のsemantic、format、byte offset、input slotを指定します
	struct VertexElementDesc {
		VertexSemantic semantic      = VertexSemantic::POSITION;
		uint8_t        semanticIndex = 0;
		VertexFormat   format        = VertexFormat::FLOAT3;
		uint16_t       offset        = 0;
		uint16_t       inputSlot     = 0;

		bool     perInstance      = false;
		uint16_t instanceStepRate = 0;
	};

	/// @brief VertexLayoutDescは、PSO作成へ渡す頂点属性列とstrideを指定します
	struct VertexLayoutDesc {
		uint32_t                       stride = 0;
		std::vector<VertexElementDesc> elements;

		bool operator ==(const VertexLayoutDesc& rhs) const;
	};
}
