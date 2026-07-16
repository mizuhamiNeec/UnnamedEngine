#pragma once
#include <cstdint>
#include <vector>

#include "core/math/Mat4.h"

#include "engine/unnamed/primitive/Primitives.h"

namespace Unnamed::Render {
	class RenderGraph;

	enum class RT_BUILD_MODE : uint8_t {
		NONE,
		SOFTWARE_FALLBACK,
		HARDWARE_INLINE,
		HARDWARE_PIPELINE,
	};

	struct RtInstanceDesc {
		uint32_t meshIndex    = 0;
		uint32_t instanceMask = 0xFF;
		Mat4     world        = Mat4::identity;
		AABB     worldBounds  = {};
	};

	struct RtFrameState {
		RT_BUILD_MODE               buildMode = RT_BUILD_MODE::NONE;
		std::vector<RtInstanceDesc> visibleInstances;
		bool                        needsTlasRebuild = false;
	};

	struct GiFrameState {
		uint32_t irradianceHistoryTextureId = 0;
		uint32_t momentsHistoryTextureId    = 0;
		bool     historyValid               = false;
	};

	struct VirtualGeometryConfig {
		uint32_t pageSize          = 128;
		uint32_t maxResidentPages  = 65'536;
		uint32_t requestBufferSize = 16'384;
	};

	struct VirtualGeometryFrameState {
		uint32_t              feedbackTextureId = 0;
		std::vector<uint32_t> feedbackRequests;
	};

	class AdvancedRenderFoundation {
	public:
		void Initialize(
			const RenderGraph& graph, uint32_t width, uint32_t height
		);
		void OnResize(uint32_t width, uint32_t height);
		void BeginFrame();

		[[nodiscard]] RtFrameState&              GetRtState();
		[[nodiscard]] GiFrameState&              GetGiState();
		[[nodiscard]] VirtualGeometryFrameState& GetVirtualGeometryState();
		[[nodiscard]] const VirtualGeometryConfig&
		GetVirtualGeometryConfig() const;

	private:
		bool mInitialized = false;

		RtFrameState              mRtFrameState              = {};
		GiFrameState              mGiFrameState              = {};
		VirtualGeometryConfig     mVirtualGeometryConfig     = {};
		VirtualGeometryFrameState mVirtualGeometryFrameState = {};
	};
}
