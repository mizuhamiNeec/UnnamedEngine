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

	/// @brief RtInstanceDescは、TLAS instanceのtransform、BLAS address、mask、instance IDを指定します
	struct RtInstanceDesc {
		uint32_t meshIndex    = 0;
		uint32_t instanceMask = 0xFF;
		Mat4     world        = Mat4::identity;
		AABB     worldBounds  = {};
	};

	/// @brief RtFrameStateは、ray tracing機能のframe有効状態と診断値を保持します
	struct RtFrameState {
		RT_BUILD_MODE               buildMode = RT_BUILD_MODE::NONE;
		std::vector<RtInstanceDesc> visibleInstances;
		bool                        needsTlasRebuild = false;
	};

	/// @brief GiFrameStateは、global illumination機能のframe有効状態と診断値を保持します
	struct GiFrameState {
		uint32_t irradianceHistoryTextureId = 0;
		uint32_t momentsHistoryTextureId    = 0;
		bool     historyValid               = false;
	};

	/// @brief VirtualGeometryConfigは、VirtualGeometry機能の生成時に適用する有効化条件と調整値を保持します
	struct VirtualGeometryConfig {
		uint32_t pageSize          = 128;
		uint32_t maxResidentPages  = 65'536;
		uint32_t requestBufferSize = 16'384;
	};

	/// @brief VirtualGeometryFrameStateは、virtual geometry機能のframe有効状態と診断値を保持します
	struct VirtualGeometryFrameState {
		uint32_t              feedbackTextureId = 0;
		std::vector<uint32_t> feedbackRequests;
	};

	/// @brief AdvancedRenderFoundationは、レイトレーシング、GI、仮想ジオメトリ機能の利用可否とフレーム状態を集約します
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
