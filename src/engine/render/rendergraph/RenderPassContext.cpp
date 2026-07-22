#include "RenderPassContext.h"

#include <algorithm>
#include <unordered_set>

#include "RenderGraph.h"

#include "engine/RHI/d3d12/D3D12CommandContext.h"
#include "engine/rhi/d3d12/D3D12SwapChain.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	RenderPassContext::RenderPassContext(
		Rhi::D3D12CommandContext& context,
		ID3D12GraphicsCommandList* commandList,
		const uint32_t backBufferWidth, const uint32_t backBufferHeight,
		ID3D12DescriptorHeap* srvUavHeap,
		const IDescriptorResolver& descriptorResolver
	) : mContext(context),
	    mCommandList(commandList),
	    mBackBufferWidth(backBufferWidth),
	    mBackBufferHeight(backBufferHeight),
	    mSrvUavHeap(srvUavHeap),
	    mDescriptorResolver(descriptorResolver) {
	}

	void RenderPassContext::SetViewportToBackBuffer() const {
		SetViewportAndScissor(
			0.0f,
			0.0f,
			static_cast<float>(mBackBufferWidth),
			static_cast<float>(mBackBufferHeight)
		);
	}

	void RenderPassContext::SetViewportAndScissor(
		const float x, const float y, const float width, const float height
	) const {
		const float safeWidth  = std::max(1.0f, width);
		const float safeHeight = std::max(1.0f, height);

		const D3D12_VIEWPORT viewport = {
			.TopLeftX = x,
			.TopLeftY = y,
			.Width    = safeWidth,
			.Height   = safeHeight,
			.MinDepth = 0.0f,
			.MaxDepth = 1.0f,
		};

		const D3D12_RECT scissorRect = {
			.left   = static_cast<LONG>(x),
			.top    = static_cast<LONG>(y),
			.right  = static_cast<LONG>(x + safeWidth),
			.bottom = static_cast<LONG>(y + safeHeight),
		};

		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);
	}

	void RenderPassContext::SetSrvUavHeap() const {
		mContext.SetSrvUavHeap(mSrvUavHeap);
	}

	void RenderPassContext::BindComputeUavTable(
		const uint32_t rootIndex, const uint32_t textureId
	) const {
		const auto gpu = mDescriptorResolver.GetUav(textureId);
		mCommandList->SetComputeRootDescriptorTable(rootIndex, gpu);
	}

	void RenderPassContext::BindGraphicsSrvTable(
		const uint32_t rootIndex, const uint32_t textureId
	) const {
		const auto gpu = mDescriptorResolver.GetSrv(textureId);
		mCommandList->SetGraphicsRootDescriptorTable(rootIndex, gpu);
	}

	void RenderPassContext::BindGraphicsSrvTable(
		const uint32_t rootIndex, const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
	) const {
		mCommandList->SetGraphicsRootDescriptorTable(rootIndex, gpuHandle);
	}

	void RenderPassContext::SetIndexBuffer(
		const D3D12_INDEX_BUFFER_VIEW& ibv
	) const {
		mCommandList->IASetIndexBuffer(&ibv);
	}

	void RenderPassContext::BindGraphicsCbv(
		const uint32_t rootIndex, const D3D12_GPU_VIRTUAL_ADDRESS gpuVa
	) const {
		if (gpuVa == 0) {
			Fatal(
				"RDG", "無効なGPU仮想アドレスをセットしようとしました: rootIndex={}, gpuVa={:#x}",
				rootIndex, gpuVa
			);
			return;
		}
		mCommandList->SetGraphicsRootConstantBufferView(rootIndex, gpuVa);
	}

	void RenderPassContext::BindComputeCbv(
		uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS gpuVa
	) const {
		if (gpuVa == 0) {
			Fatal(
				"RDG", "無効なGPU仮想アドレスをセットしようとしました: rootIndex={}, gpuVa={:#x}",
				rootIndex, gpuVa
			);
			return;
		}
		mCommandList->SetComputeRootConstantBufferView(rootIndex, gpuVa);
	}

	void RenderPassContext::SetStencilRef(const uint32_t ref) const {
		mCommandList->OMSetStencilRef(ref);
	}

	void RenderPassContext::SetComputePipeline(
		ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState
	) const {
		mContext.SetComputePipeline(rootSignature, pipelineState);
	}

	void RenderPassContext::SetGraphicsPipeline(
		ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState
	) const {
		mContext.SetGraphicsPipeline(rootSignature, pipelineState);
	}

	void RenderPassContext::SetPrimitiveTopology(
		const D3D_PRIMITIVE_TOPOLOGY topology
	) const {
		mCommandList->IASetPrimitiveTopology(topology);
	}

	void RenderPassContext::SetVertexBuffer(
		const D3D12_VERTEX_BUFFER_VIEW& vbv
	) const {
		mCommandList->IASetPrimitiveTopology(
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->IASetVertexBuffers(0, 1, &vbv);
	}

	void RenderPassContext::DrawTriangleTest() const {
		mCommandList->DrawInstanced(3, 1, 0, 0);
	}

	void RenderPassContext::DrawInstanced(
		const uint32_t vertexCount, const uint32_t instanceCount
	) const {
		mCommandList->DrawInstanced(vertexCount, instanceCount, 0, 0);
	}

	void RenderPassContext::DrawIndexedTest(
		const uint32_t indexCount,
		const uint32_t startIndexLocation,
		const int32_t  baseVertexLocation
	) const {
		mCommandList->DrawIndexedInstanced(
			indexCount,
			1,
			startIndexLocation,
			baseVertexLocation,
			0
		);
	}

	void RenderPassContext::DispatchForBackBuffer(
		const uint32_t threadGroupSizeX, const uint32_t threadGroupSizeY
	) const {
		const uint32_t gx = (mBackBufferWidth + threadGroupSizeX - 1) /
		                    threadGroupSizeX;
		const uint32_t gy = (mBackBufferHeight + threadGroupSizeY - 1) /
		                    threadGroupSizeY;
		mContext.Dispatch(gx, gy, 1);
	}

	void RenderPassContext::Dispatch(
		const uint32_t x, const uint32_t y, const uint32_t z
	) const {
		mContext.Dispatch(x, y, z);
	}

	void RenderPassContext::DrawFullscreenTriangle() const {
		mContext.DrawFullScreenTriangle();
	}

	void RenderPassContext::ClearBackBuffer(
		const float r, const float g, const float b, const float a
	) const {
		const Rhi::ClearColor color = {.r = r, .g = g, .b = b, .a = a};
		mContext.ClearBackBuffer(color);
	}

	uint32_t RenderPassContext::GetBackBufferWidth() const {
		return mBackBufferWidth;
	}

	uint32_t RenderPassContext::GetBackBufferHeight() const {
		return mBackBufferHeight;
	}

	void RenderPassContext::SetBackBufferAsRenderTarget() const {
		const uint32_t index = mContext.GetSwapChain()->
		                                GetCurrentBackBufferIndex();
		const auto rtv = mContext.GetSwapChain()->GetRtvHandle(index);
		mCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	void RenderPassContext::SetRenderTarget(uint32_t textureId) const {
		const auto rtv = mDescriptorResolver.GetRtvCpu(textureId);
		if (rtv.ptr == 0) {
			Fatal("RDG", "無効なRTVをセットしようとしました: textureId={}", textureId);
			return;
		}
		mCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	void RenderPassContext::ClearRtv(
		uint32_t    textureId, const float r, const float g, const float b,
		const float a
	) const {
		const auto rtv = mDescriptorResolver.GetRtvCpu(textureId);
		if (rtv.ptr == 0) {
			Fatal("RDG", "無効なRTVをクリアしようとしました: textureId={}", textureId);
			return;
		}
		const float color[4] = {r, g, b, a};
		mCommandList->ClearRenderTargetView(rtv, color, 0, nullptr);
	}

	void RenderPassContext::SetRenderTargetById(
		const uint32_t textureId
	) const {
		if (textureId == RenderGraph::kBackBufferId) {
			SetBackBufferAsRenderTarget();
			return;
		}
		SetRenderTarget(textureId);
	}

	void RenderPassContext::ClearColorById(
		const uint32_t textureId, const float r, const float g, const float b,
		const float    a
	) const {
		if (textureId == RenderGraph::kBackBufferId) {
			ClearBackBuffer(r, g, b, a);
			return;
		}
		ClearRtv(textureId, r, g, b, a);
	}

	void RenderPassContext::SetRenderTargets(
		const std::span<const uint32_t> textureIds
	) const {
		if (textureIds.empty()) {
			mCommandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
			return;
		}

		std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 8> rtvs = {}; // 最大8つまで

		constexpr auto size = static_cast<const unsigned&>(rtvs.size());

		const uint32_t count = std::min<uint32_t>(
			static_cast<uint32_t>(textureIds.size()),
			size
		);

		for (uint32_t i = 0; i < count; ++i) {
			const auto rtv = mDescriptorResolver.GetRtvCpu(textureIds[i]);
			if (rtv.ptr == 0) {
				Fatal(
					"RDG", "無効なRTVをセットしようとしました: textureId={}", textureIds[i]
				);
				rtvs[i] = {};
			} else {
				rtvs[i] = rtv;
			}
		}

		mCommandList->OMSetRenderTargets(count, rtvs.data(), FALSE, nullptr);
	}

	void RenderPassContext::SetRenderTargetsByIds(
		const std::span<const uint32_t> textureIds
	) const {
		if (textureIds.empty()) {
			return;
		}

		constexpr uint32_t maxRtvs = 8;
		const uint32_t     count   = std::min<uint32_t>(
			static_cast<uint32_t>(textureIds.size()), maxRtvs
		);

		std::array<D3D12_CPU_DESCRIPTOR_HANDLE, maxRtvs> rtvs      = {};
		std::array<ID3D12Resource*, maxRtvs>             resources = {};
		uint32_t                                         outCount  = 0;

		std::unordered_set<uint64_t>              seenRtvPtr;
		std::unordered_set<const ID3D12Resource*> seenRes;

		for (uint32_t i = 0; i < count; ++i) {
			const uint32_t id = textureIds[i];

			D3D12_CPU_DESCRIPTOR_HANDLE rtv;
			ID3D12Resource*             res;

			if (id == RenderGraph::kBackBufferId) {
				rtv = mContext.GetCurrentBackBufferRtv();
				res = mContext.GetCurrentBackBufferResource();
			} else {
				rtv = mDescriptorResolver.GetRtvCpu(id);
				res = mDescriptorResolver.GetResource(id);
			}

			if (rtv.ptr == 0) {
				Fatal("RDG", "無効なRTVをセットしようとしました: textureId={}", id);
				return;
			}

			for (uint32_t j = 0; j < outCount; ++j) {
				if (rtvs[j].ptr == rtv.ptr) {
					Error(
						"RDG", "同じRTVが複数回指定されています: textureId={}, rtvPtr={:#x}",
						id, rtv.ptr
					);
					return;
				}

				if (res != nullptr && resources[j] == res) {
					Error(
						"RDG",
						"同じリソースが複数回RTとして指定されています: textureId={}, resPtr={:#x}",
						id, reinterpret_cast<uint64_t>(res)
					);
					return;
				}
			}

			rtvs[outCount]      = rtv;
			resources[outCount] = res;
			++outCount;
		}

		mCommandList->OMSetRenderTargets(
			outCount,
			rtvs.data(),
			FALSE,
			nullptr
		);
	}

	void RenderPassContext::SetDepthStencilById(uint32_t textureId) const {
		const auto dsv = mDescriptorResolver.GetDsvCpu(textureId);
		if (dsv.ptr == 0) {
			Fatal("RDG", "無効なDSVをセットしようとしました: textureId={}", textureId);
			return;
		}

		mCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
	}

	void RenderPassContext::ClearDepthById(
		const uint32_t textureId, const float depth
	) const {
		ClearDepthStencilById(textureId, depth, 0);
	}

	void RenderPassContext::ClearDepthStencilById(
		uint32_t textureId, const float depth, const uint8_t stencil
	) const {
		if (textureId == RenderGraph::kBackBufferId) {
			Fatal("RDG", "バックバッファは深度ステンシルを持ちません。");
			return;
		}

		const auto dsv = mDescriptorResolver.GetDsvCpu(textureId);
		if (dsv.ptr == 0) {
			Fatal("RDG", "無効なDSVをクリアしようとしました: textureId={}", textureId);
			return;
		}

		mCommandList->ClearDepthStencilView(
			dsv,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			depth,
			stencil,
			0,
			nullptr
		);
	}

	void RenderPassContext::SetRenderTargetAndDepth(
		const std::span<const uint32_t> colorRtIds,
		std::optional<uint32_t>         depthRtId
	) const {
		if (colorRtIds.empty() && !depthRtId.has_value()) {
			mCommandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
			return;
		}

		std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 8> rtvs = {}; // 最大8つまで

		constexpr auto size = static_cast<const unsigned&>(rtvs.size());

		const uint32_t count = std::min<uint32_t>(
			static_cast<uint32_t>(colorRtIds.size()),
			size
		);

		for (uint32_t i = 0; i < count; ++i) {
			const uint32_t id = colorRtIds[i];

			D3D12_CPU_DESCRIPTOR_HANDLE rtv;
			if (id == RenderGraph::kBackBufferId) {
				rtv = mContext.GetCurrentBackBufferRtv();
			} else {
				rtv = mDescriptorResolver.GetRtvCpu(id);
			}

			if (rtv.ptr == 0) {
				Fatal(
					"RDG", "無効なRTVをセットしようとしました: textureId={}", colorRtIds[i]
				);
				return;
			}

			rtvs[i] = rtv;
		}

		const D3D12_CPU_DESCRIPTOR_HANDLE* dsvPtr = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE        dsv;
		if (depthRtId.has_value()) {
			dsv = mDescriptorResolver.GetDsvCpu(depthRtId.value());
			if (dsv.ptr == 0) {
				Fatal(
					"RDG", "無効なDSVをセットしようとしました: textureId={}", depthRtId.value()
				);
				return;
			}
			dsvPtr = &dsv;
		}

		mCommandList->OMSetRenderTargets(count, rtvs.data(), FALSE, dsvPtr);
	}
}
