#include "D3D12CommandContext.h"

#include "D3D12SwapChain.h"

namespace Unnamed::Rhi {
	D3D12CommandContext::D3D12CommandContext(
		ID3D12GraphicsCommandList* commandList, D3D12SwapChain* swapChain
	) : mCommandList(commandList),
	    mSwapChain(swapChain) {
	}

	void D3D12CommandContext::Begin() {
	}

	void D3D12CommandContext::End() {
	}

	void D3D12CommandContext::TransitionBackBufferToRenderTarget() {
		// 現在のバックバッファを取得
		const uint32_t  index  = mSwapChain->GetCurrentBackBufferIndex();
		ID3D12Resource* buffer = mSwapChain->GetBackBuffer(index);

		// バックバッファをレンダーターゲットに遷移
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource   = buffer;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		mCommandList->ResourceBarrier(1, &barrier);
	}

	void D3D12CommandContext::TransitionBackBufferToPresent() {
		// 現在のバックバッファを取得
		const uint32_t  index  = mSwapChain->GetCurrentBackBufferIndex();
		ID3D12Resource* buffer = mSwapChain->GetBackBuffer(index);

		// バックバッファをプレゼントに遷移
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = buffer;
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;

		mCommandList->ResourceBarrier(1, &barrier);
	}

	void D3D12CommandContext::ClearBackBuffer(const ClearColor& color) {
		// 現在のバックバッファを取得
		const uint32_t index = mSwapChain->GetCurrentBackBufferIndex();
		const auto     rtv   = mSwapChain->GetRtvHandle(index);

		mCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		const std::array clearColor{
			color.r, color.g, color.b, color.a
		};
		mCommandList->ClearRenderTargetView(rtv, clearColor.data(), 0, nullptr);
	}

	void D3D12CommandContext::SetSrvUavHeap(ID3D12DescriptorHeap* heap) {
		ID3D12DescriptorHeap* heaps[] = {heap};
		mCommandList->SetDescriptorHeaps(1, heaps);
	}

	void D3D12CommandContext::TransitionResource(
		ID3D12Resource* resource, const D3D12_RESOURCE_STATES before,
		const D3D12_RESOURCE_STATES after
	) {
		if (before == after) {
			return;
		}

		D3D12_RESOURCE_BARRIER b = {};
		b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Transition.pResource   = resource;
		b.Transition.StateBefore = before;
		b.Transition.StateAfter  = after;
		b.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		mCommandList->ResourceBarrier(1, &b);
	}

	void D3D12CommandContext::SetComputePipeline(
		ID3D12RootSignature* rs, ID3D12PipelineState* pso
	) {
		mCommandList->SetComputeRootSignature(rs);
		mCommandList->SetPipelineState(pso);
	}

	void D3D12CommandContext::SetGraphicsPipeline(
		ID3D12RootSignature* rs, ID3D12PipelineState* pso
	) {
		mCommandList->SetGraphicsRootSignature(rs);
		mCommandList->SetPipelineState(pso);
	}

	void D3D12CommandContext::SetComputeRootUavTable(
		const uint32_t rootIndex, const D3D12_GPU_DESCRIPTOR_HANDLE handle
	) {
		mCommandList->SetComputeRootDescriptorTable(rootIndex, handle);
	}

	void D3D12CommandContext::SetGraphicsRootSrvTable(
		const uint32_t rootIndex, const D3D12_GPU_DESCRIPTOR_HANDLE handle
	) {
		mCommandList->SetGraphicsRootDescriptorTable(rootIndex, handle);
	}

	void D3D12CommandContext::Dispatch(
		const uint32_t x, const uint32_t y, const uint32_t z
	) {
		mCommandList->Dispatch(x, y, z);
	}

	void D3D12CommandContext::DrawFullScreenTriangle() {
		mCommandList->IASetPrimitiveTopology(
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		);
		mCommandList->DrawInstanced(3, 1, 0, 0);
	}

	D3D12SwapChain* D3D12CommandContext::GetSwapChain() const {
		return mSwapChain;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE
	D3D12CommandContext::GetCurrentBackBufferRtv() const {
		const uint32_t index = mSwapChain->GetCurrentBackBufferIndex();
		return mSwapChain->GetRtvHandle(index);
	}

	ID3D12Resource* D3D12CommandContext::GetCurrentBackBufferResource() {
		const uint32_t index = mSwapChain->GetCurrentBackBufferIndex();
		return mSwapChain->GetBackBuffer(index);
	}
}
