#pragma once
#include <wrl/client.h>

using namespace Microsoft::WRL;

class Swapchain {
public:
	/*Swapchain(ComPtr<IDXGISwapChain4> swapChain, std::shared_ptr<DescriptorManager>& heapRTV, bool useHDR);*/
	~Swapchain();

	//UINT GetCurrentBackBufferIndex() const;
	//HRESULT Present(UINT SyncInterval, UINT Flags);
	//DescriptorHandle GetCurrentRTV(); const;

	//void WaitPreviousFrame(ComPtr<ID3D12CommandQueue> commandQueue, int frameIndex, DWORD timeout);

	//void ResizeBuffers(UINT width, UINT height);

	//D3D12_RESOURCE_BARRIER GetBarrierToRenderTarget();
	//D3D12_RESOURCE_BARRIER GetBarrierToPresent();

	//DXGI_FORMAT GetFormat() const { return desc.Format; }
private:
	void SetMetadata();
};

