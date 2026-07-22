#include "RenderGraphBuilder.h"

#include "RenderGraph.h"

namespace Unnamed::Render {
	namespace {
		bool HasUse(
			const std::vector<RgUse>& uses,
			const uint32_t            texId,
			const RG_ACCESS           access
		) {
			return std::ranges::any_of(
				uses,
				[&](const RgUse& u) {
					return u.textureId == texId && u.access == access;
				}
			);
		}
	}

	RenderGraphBuilder::RenderGraphBuilder(
		RgResourceRegistry& registry
	) : mRegistry(registry) {
	}

	uint32_t RenderGraphBuilder::CreateTexture(
		const RgTextureDesc& desc
	) const {
		return mRegistry.CreateTexture(desc);
	}

	void RenderGraphBuilder::WriteUav(const uint32_t texId) {
		MarkUse(texId, RG_ACCESS::UAV_WRITE);
	}

	void RenderGraphBuilder::ReadSrvPs(const uint32_t texId) {
		MarkUse(texId, RG_ACCESS::SRV_READ_PS);
	}

	void RenderGraphBuilder::ReadSrvCs(const uint32_t texId) {
		MarkUse(texId, RG_ACCESS::SRV_READ_CS);
	}

	void RenderGraphBuilder::WriteBackBufferRt() {
		SetColorRt(RenderGraph::kBackBufferId);
	}

	void RenderGraphBuilder::WriteRt(const uint32_t texId) {
		SetColorRt(texId);
	}

	void RenderGraphBuilder::ClearColor(
		const uint32_t texId,
		const float    r, const float g, const float b, const float a
	) {
		SetColorRt(texId);

		const auto it = std::ranges::find_if(
			mClearCommands,
			[&](const RgClearCmd& c) {
				return c.textureId == texId;
			}
		);

		if (it != mClearCommands.end()) {
			// すでにクリアコマンドがある場合は上書きする
			it->color = {r, g, b, a};
		} else {
			mClearCommands.emplace_back(
				RgClearCmd{
					.textureId = texId,
					.color     = {.r = r, .g = g, .b = b, .a = a}
				}
			);
		}
	}

	void RenderGraphBuilder::WriteDepth(uint32_t texId) {
		SetDepthRt(texId);
	}

	void RenderGraphBuilder::ClearDepth(
		uint32_t texId, float depth, uint8_t stencil
	) {
		SetDepthRt(texId);

		const auto it = std::ranges::find_if(
			mClearDepthCommands,
			[&](const RgDepthClearCmd& c) {
				return c.textureId == texId;
			}
		);

		if (it != mClearDepthCommands.end()) {
			it->depth   = depth;
			it->stencil = stencil;
		} else {
			mClearDepthCommands.emplace_back(
				RgDepthClearCmd{
					.textureId = texId,
					.depth     = depth,
					.stencil   = stencil,
				}
			);
		}
	}

	const std::vector<RgUse>& RenderGraphBuilder::GetUses() {
		return mUses;
	}

	const std::vector<RgClearCmd>& RenderGraphBuilder::GetClearColors() const {
		return mClearCommands;
	}

	const std::vector<RgDepthClearCmd>& RenderGraphBuilder::
	GetClearDepths() const {
		return mClearDepthCommands;
	}

	const std::vector<uint32_t>& RenderGraphBuilder::GetColorRts() const {
		return mColorRts;
	}

	const std::optional<uint32_t>& RenderGraphBuilder::GetDepthRt() const {
		return mDepthRt;
	}

	void RenderGraphBuilder::SetColorRt(uint32_t texId) {
		if (!std::ranges::contains(mColorRts, texId)) {
			mColorRts.emplace_back(texId);
		}
		MarkUse(texId, RG_ACCESS::RENDER_TARGET);
	}

	void RenderGraphBuilder::SetDepthRt(uint32_t texId) {
		mDepthRt = texId;
		MarkUse(texId, RG_ACCESS::DEPTH_WRITE);
	}

	void RenderGraphBuilder::MarkUse(uint32_t texId, RG_ACCESS access) {
		if (HasUse(mUses, texId, access)) {
			return;
		}

		mUses.emplace_back(texId, access);
	}
}
