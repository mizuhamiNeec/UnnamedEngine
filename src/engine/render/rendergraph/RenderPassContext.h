#pragma once

#include <cstdint>
#include <d3d12.h>
#include <optional>
#include <span>

#include "IDescriptorResolver.h"

#include "../../RHI/d3d12/D3D12CommandContext.h"

namespace Unnamed::Render {
	class RenderPassContext {
	public:
		RenderPassContext(
			Rhi::D3D12CommandContext&  context,
			ID3D12GraphicsCommandList* commandList,
			uint32_t                   backBufferWidth,
			uint32_t                   backBufferHeight,
			ID3D12DescriptorHeap*      srvUavHeap,
			const IDescriptorResolver& descriptorResolver
		);

		void SetViewportToBackBuffer() const;
		void SetViewportAndScissor(
			float x, float y, float width, float height
		) const;
		void SetSrvUavHeap() const;

		void SetBackBufferAsRenderTarget() const;

		void BindComputeUavTable(uint32_t rootIndex, uint32_t textureId) const;
		void BindGraphicsSrvTable(uint32_t rootIndex, uint32_t textureId) const;
		void BindGraphicsSrvTable(
			uint32_t rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
		) const;

		void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibv) const;

		void BindGraphicsCbv(
			uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS gpuVa
		) const;
		void BindComputeCbv(
			uint32_t rootIndex, D3D12_GPU_VIRTUAL_ADDRESS gpuVa
		) const;

		void SetRenderTarget(uint32_t textureId) const;
		void ClearRtv(
			uint32_t textureId, float r, float g, float b, float a
		) const;

		void SetRenderTargetById(uint32_t textureId) const;
		void ClearColorById(
			uint32_t textureId, float r, float g, float b, float a
		) const;

		void SetRenderTargets(std::span<const uint32_t> textureIds) const;
		void SetRenderTargetsByIds(std::span<const uint32_t> textureIds) const;

		void SetDepthStencilById(uint32_t textureId) const;
		void ClearDepthById(
			uint32_t textureId, float depth = 1.0f
		) const;
		void ClearDepthStencilById(
			uint32_t textureId, float depth, uint8_t stencil
		) const;
		void SetStencilRef(uint32_t ref) const;

		void SetRenderTargetAndDepth(
			std::span<const uint32_t> colorRtIds,
			std::optional<uint32_t>   depthRtId
		) const;

		void SetComputePipeline(
			ID3D12RootSignature* rootSignature,
			ID3D12PipelineState* pipelineState
		) const;
		void SetGraphicsPipeline(
			ID3D12RootSignature* rootSignature,
			ID3D12PipelineState* pipelineState
		) const;

		void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology) const;
		void SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbv) const;

		void DrawTriangleTest() const;
		void DrawInstanced(
			uint32_t vertexCount, uint32_t instanceCount
		) const;
		void DrawIndexedTest(
			uint32_t indexCount,
			uint32_t startIndexLocation = 0,
			int32_t  baseVertexLocation = 0
		) const;

		void DispatchForBackBuffer(
			uint32_t threadGroupSizeX, uint32_t threadGroupSizeY
		) const;
		void Dispatch(uint32_t x, uint32_t y, uint32_t z) const;
		void DrawFullscreenTriangle() const;

		void ClearBackBuffer(float r, float g, float b, float a) const;

		[[nodiscard]] uint32_t GetBackBufferWidth() const;
		[[nodiscard]] uint32_t GetBackBufferHeight() const;

		[[nodiscard]] ID3D12GraphicsCommandList* GetCommandList() const {
			return mCommandList;
		}

	private:
		Rhi::D3D12CommandContext&  mContext;
		ID3D12GraphicsCommandList* mCommandList = nullptr;

		uint32_t              mBackBufferWidth  = 0;
		uint32_t              mBackBufferHeight = 0;
		ID3D12DescriptorHeap* mSrvUavHeap       = nullptr;

		const IDescriptorResolver& mDescriptorResolver;
	};
}
