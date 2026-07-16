#include "AdvancedRenderFoundation.h"

#include "engine/render/rendergraph/RenderGraph.h"
#include "engine/render/rendergraph/RgResourceRegistry.h"

namespace Unnamed::Render {
	void AdvancedRenderFoundation::Initialize(
		const RenderGraph& graph, const uint32_t width, const uint32_t height
	) {
		if (mInitialized) {
			return;
		}
		mInitialized = true;

		// RT出力の最小土台
		mRtFrameState.buildMode = RT_BUILD_MODE::SOFTWARE_FALLBACK;

		// GI履歴バッファ
		mGiFrameState.irradianceHistoryTextureId = graph.CreateTexture(
			{
				.width          = width,
				.height         = height,
				.resourceFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
				.allowUav       = true,
				.allowRtv       = true,
				.debugName      = "GI_IrradianceHistory",
				.extentMode     = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
			}
		);
		mGiFrameState.momentsHistoryTextureId = graph.CreateTexture(
			{
				.width          = width,
				.height         = height,
				.resourceFormat = DXGI_FORMAT_R16G16_FLOAT,
				.allowUav       = true,
				.allowRtv       = true,
				.debugName      = "GI_MomentsHistory",
				.extentMode     = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
			}
		);

		// Virtual Geometry フィードバック
		mVirtualGeometryFrameState.feedbackTextureId = graph.CreateTexture(
			{
				.width          = width,
				.height         = height,
				.resourceFormat = DXGI_FORMAT_R32_UINT,
				.allowUav       = true,
				.allowRtv       = false,
				.debugName      = "VG_PageFeedback",
				.extentMode     = RG_EXTENT_MODE::MATCH_BACK_BUFFER,
			}
		);
	}

	void AdvancedRenderFoundation::OnResize(const uint32_t, const uint32_t) {
		// リサイズ時は履歴を無効化してリプロジェクション破綻を防ぐ
		mGiFrameState.historyValid = false;
	}

	void AdvancedRenderFoundation::BeginFrame() {
		mRtFrameState.visibleInstances.clear();
		mRtFrameState.needsTlasRebuild = false;
		mVirtualGeometryFrameState.feedbackRequests.clear();
	}

	RtFrameState& AdvancedRenderFoundation::GetRtState() {
		return mRtFrameState;
	}

	GiFrameState& AdvancedRenderFoundation::GetGiState() {
		return mGiFrameState;
	}

	VirtualGeometryFrameState&
	AdvancedRenderFoundation::GetVirtualGeometryState(
	) {
		return mVirtualGeometryFrameState;
	}

	const VirtualGeometryConfig& AdvancedRenderFoundation::
	GetVirtualGeometryConfig() const {
		return mVirtualGeometryConfig;
	}
}
