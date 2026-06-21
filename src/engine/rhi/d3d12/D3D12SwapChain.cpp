#include "D3D12SwapChain.h"

#include <chrono>
#include <string_view>

#include "D3D12Util.h"

#include "core/UnnamedMacro.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Rhi {
	static constexpr std::string_view kChannel = "SwpCin";

	using namespace Microsoft::WRL;

	namespace {
		bool CheckTearingSupport(const ComPtr<IDXGIFactory4>& factory) {
			ComPtr<IDXGIFactory5> factory5;
			if (FAILED(factory.As(&factory5))) {
				return false;
			}

			BOOL          allowTearing = FALSE;
			const HRESULT hr           = factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing,
				sizeof(allowTearing)
			);
			return SUCCEEDED(hr) && allowTearing == TRUE;
		}
	}

	D3D12SwapChain::D3D12SwapChain(
		ComPtr<IDXGIFactory4>      factory,
		ComPtr<ID3D12Device>       device,
		ComPtr<ID3D12CommandQueue> graphicsQueue,
		const HWND                 hwnd,
		const SwapChainDesc&       desc
	) : mFactory(std::move(factory)),
	    mDevice(std::move(device)),
	    mGraphicsQueue(std::move(graphicsQueue)) {
		mWidth        = desc.width;
		mHeight       = desc.height;
		mBufferCount  = desc.bufferCount;
		mAllowTearing = CheckTearingSupport(mFactory);

		D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
		rtvDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDesc.NumDescriptors             = mBufferCount;
		rtvDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		Throw(
			mDevice->CreateDescriptorHeap(
				&rtvDesc, IID_PPV_ARGS(mRtvHeap.ReleaseAndGetAddressOf())
			)
		);
		mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV
		);

		CreateSwapChainInternal(hwnd, desc);
		CreateBackBuffersAndRtv();
	}

	uint32_t D3D12SwapChain::GetWidth() const {
		return mWidth;
	}

	uint32_t D3D12SwapChain::GetHeight() const {
		return mHeight;
	}

	uint32_t D3D12SwapChain::GetBufferCount() const {
		return mBufferCount;
	}

	uint32_t D3D12SwapChain::GetCurrentBackBufferIndex() const {
		return mSwapChain->GetCurrentBackBufferIndex();
	}

	TEXTURE_FORMAT D3D12SwapChain::GetFormat() const {
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		Throw(mSwapChain->GetDesc1(&desc));
		return ToTextureFormat(desc.Format);
	}

	void D3D12SwapChain::Resize(const uint32_t width, const uint32_t height) {
		// サイズが0の場合は何もしない
		if (width == 0 || height == 0) {
			return;
		}
		// サイズが変わっていなければ何もしない
		if (width == mWidth && height == mHeight) {
			return;
		}

		const auto     resizeStart = std::chrono::steady_clock::now();
		const uint32_t oldWidth    = mWidth;
		const uint32_t oldHeight   = mHeight;
		DevMsg(
			kChannel,
			"Resize start: {}x{} -> {}x{}",
			oldWidth,
			oldHeight,
			width,
			height
		);

		for (uint32_t i = 0; i < mBufferCount; i++) {
			mBackBuffers[i].Reset();
		}

		mWidth  = width;
		mHeight = height;

		DXGI_SWAP_CHAIN_DESC1 desc = {};
		Throw(mSwapChain->GetDesc1(&desc));

		Throw(
			mSwapChain->ResizeBuffers(
				mBufferCount, mWidth, mHeight, desc.Format, desc.Flags
			)
		);

		CreateBackBuffersAndRtv();
		const double elapsedMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - resizeStart
		).count();
		Msg(
			kChannel,
			"Resize done: {}x{} (elapsedMs={:.3f})",
			mWidth,
			mHeight,
			elapsedMs
		);
	}

	void D3D12SwapChain::Present(const bool vSync) {
		const UINT syncInterval = vSync ? 1u : 0u;
		UINT       flags        = 0u;
		if (!vSync && mAllowTearing) {
			flags |= DXGI_PRESENT_ALLOW_TEARING;
		}
		Throw(mSwapChain->Present(syncInterval, flags));
	}

	IDXGISwapChain3* D3D12SwapChain::GetSwapChain() const {
		return mSwapChain.Get();
	}

	ID3D12Resource* D3D12SwapChain::GetBackBuffer(const uint32_t index) const {
		return mBackBuffers[index].Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12SwapChain::GetRtvHandle(
		const uint32_t index
	) const {
		UASSERT(index < mBufferCount);
		auto [ptr] = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		ptr        +=
			static_cast<SIZE_T>(index) *
			static_cast<SIZE_T>(mRtvDescriptorSize);

		return {ptr};
	}

	void D3D12SwapChain::CreateSwapChainInternal(
		const HWND hwnd, const SwapChainDesc& desc
	) {
		DXGI_SWAP_CHAIN_DESC1 scDesc;
		scDesc.Width              = desc.width;
		scDesc.Height             = desc.height;
		scDesc.Format             = ToDxgiFormat(desc.format);
		scDesc.Stereo             = FALSE;
		scDesc.SampleDesc.Count   = 1;
		scDesc.SampleDesc.Quality = 0;
		scDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scDesc.BufferCount        = desc.bufferCount;
		scDesc.Scaling            = DXGI_SCALING_STRETCH;
		scDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		scDesc.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
		scDesc.Flags              = !desc.vSync && mAllowTearing ?
			               DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING :
			               0;

		ComPtr<IDXGISwapChain1> swapChain1;
		Throw(
			mFactory->CreateSwapChainForHwnd(
				mGraphicsQueue.Get(),
				hwnd,
				&scDesc,
				nullptr,
				nullptr,
				swapChain1.ReleaseAndGetAddressOf()
			)
		);

		// Alt+Enter 無効化
		Throw(mFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

		// IDXGISwapChain4 を取得
		Throw(swapChain1.As(&mSwapChain));
	}

	void D3D12SwapChain::CreateBackBuffersAndRtv() {
		for (uint32_t i = 0; i < mBufferCount; ++i) {
			// バックバッファを取得して RTV を作成
			Throw(
				mSwapChain->GetBuffer(
					i, IID_PPV_ARGS(mBackBuffers[i].ReleaseAndGetAddressOf())
				)
			);
			mDevice->CreateRenderTargetView(
				mBackBuffers[i].Get(), nullptr, GetRtvHandle(i)
			);

			auto name = std::format(L"BackBuffer{}", i);
			mBackBuffers[i]->SetName(name.c_str());
		}
	}
}
