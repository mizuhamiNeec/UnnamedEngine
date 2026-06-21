#pragma once
#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "engine/rhi/RhiTypes.h"
#include "engine/rhi/interface/IRhiSwapChain.h"

namespace Unnamed::Rhi {
	class D3D12SwapChain final : public IRhiSwapChain {
	public:
		D3D12SwapChain(
			Microsoft::WRL::ComPtr<IDXGIFactory4>      factory,
			Microsoft::WRL::ComPtr<ID3D12Device>       device,
			Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphicsQueue,
			HWND                                       hwnd,
			const SwapChainDesc&                       desc
		);

		[[nodiscard]] uint32_t       GetWidth() const override;
		[[nodiscard]] uint32_t       GetHeight() const override;
		[[nodiscard]] uint32_t       GetBufferCount() const override;
		[[nodiscard]] uint32_t       GetCurrentBackBufferIndex() const override;
		[[nodiscard]] TEXTURE_FORMAT GetFormat() const override;

		void Resize(uint32_t width, uint32_t height) override;
		void Present(bool vSync) override;

		[[nodiscard]] IDXGISwapChain3* GetSwapChain() const;

		[[nodiscard]] ID3D12Resource* GetBackBuffer(uint32_t index) const;

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(
			uint32_t index
		) const;

	private:
		void CreateSwapChainInternal(HWND hwnd, const SwapChainDesc& desc);
		void CreateBackBuffersAndRtv();

		Microsoft::WRL::ComPtr<IDXGIFactory4>      mFactory;
		Microsoft::WRL::ComPtr<ID3D12Device>       mDevice;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mGraphicsQueue;

		Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		uint32_t                                     mRtvDescriptorSize = 0;

		uint32_t mWidth        = 0;
		uint32_t mHeight       = 0;
		uint32_t mBufferCount  = 2;
		bool     mAllowTearing = false;

		// 最大トリプルバッファ対応
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 3> mBackBuffers = {};
	};
}
