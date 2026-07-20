#pragma once

#include <cstdint>
#include <d3d12.h>
#include <optional>
#include <span>

#include "IDescriptorResolver.h"

namespace Unnamed::Rhi {
	class D3D12CommandContext;
}

namespace Unnamed::Render {
	class RenderGraph;

	/// @brief RenderGraph が確定したパス状態の上で描画コマンドを記録するためのコンテキストです。
	/// @details アタッチメントの選択とクリアは RenderGraph のリソース契約から一度だけ行います。
	///          パスコールバックはパイプライン、ディスクリプタ、定数、ジオメトリ、draw/dispatch だけを記録します。
	class RenderPassContext {
	public:
		/// @brief コマンド記録に必要な D3D12 状態とディスクリプタ解決器を束ねます。
		RenderPassContext(
			Rhi::D3D12CommandContext&  context,
			ID3D12GraphicsCommandList* commandList,
			uint32_t                   backBufferWidth,
			uint32_t                   backBufferHeight,
			ID3D12DescriptorHeap*      srvUavHeap,
			const IDescriptorResolver& descriptorResolver
		);

		/// @brief 現在のバックバッファ全体をビューポートとシザーに設定します。
		void SetViewportToBackBuffer() const;
		/// @brief 指定した論理描画領域をビューポートとシザーに設定します。
		void SetViewportAndScissor(
			float x, float y, float width, float height
		) const;
		/// @brief グラフィックス/コンピュート SRV/UAV テーブルに使うヒープを設定します。
		void SetSrvUavHeap() const;

		void BindComputeUavTable(uint32_t rootIndex, uint32_t textureId) const;
		void BindGraphicsSrvTable(uint32_t rootIndex, uint32_t textureId) const;
		/// @brief 事前構築済みの SRV テーブルをグラフィックスルートへ設定します。
		/// @warning テーブル内の全リソースは、setup コールバックで read 宣言済みでなければなりません。
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

		void SetStencilRef(uint32_t ref) const;

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

		/// @brief 外部描画統合用の D3D12 コマンドリストを返します。
		/// @warning 呼び出し側は RenderGraph が設定したアタッチメントを変更してはいけません。
		[[nodiscard]] ID3D12GraphicsCommandList* GetCommandList() const {
			return mCommandList;
		}

	private:
		friend class RenderGraph;

		// アタッチメントの選択とクリアは、RenderGraph だけが行う。
		void SetBackBufferAsRenderTarget() const;
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
		void SetRenderTargetAndDepth(
			std::span<const uint32_t> colorRtIds,
			std::optional<uint32_t>   depthRtId
		) const;

		Rhi::D3D12CommandContext&  mContext;
		ID3D12GraphicsCommandList* mCommandList = nullptr;

		uint32_t              mBackBufferWidth  = 0;
		uint32_t              mBackBufferHeight = 0;
		ID3D12DescriptorHeap* mSrvUavHeap       = nullptr;

		const IDescriptorResolver& mDescriptorResolver;
	};
}
