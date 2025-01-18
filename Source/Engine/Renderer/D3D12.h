#pragma once
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <wrl/client.h>

#include "Renderer.h"

using namespace Microsoft::WRL;

class D3D12 : public Renderer {
public: // メンバ関数
	D3D12();
	~D3D12() override;

	void Init() override;

	void ClearColorAndDepth() const;
	void PreRender() override;
	void PostRender() override;

	static void WriteToUploadHeapMemory(ID3D12Resource* resource, uint32_t size, const void* data);

	void WaitPreviousFrame();

	void Resize(uint32_t width, uint32_t height);

private:
	//　メンバ変数
	ComPtr<ID3D12Device> device_;
	ComPtr<IDXGIFactory7> dxgiFactory_;
	ComPtr<ID3D12CommandQueue> commandQueue_;
	ComPtr<IDXGISwapChain4> swapChain_;


	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	std::vector<ComPtr<ID3D12Resource>> renderTargets_;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles_;

	ComPtr<ID3D12Resource> depthStencilResource_;

	ComPtr<ID3D12CommandAllocator> commandAllocator_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;

	ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
	UINT frameIndex_ = 0;

	D3D12_RESOURCE_BARRIER barrier_ = {};

	D3D12_VIEWPORT viewport_ = {};
	D3D12_RECT scissorRect_ = {};

	uint32_t descriptorSizeRTV = 0;
	uint32_t descriptorSizeDSV = 0;

	std::chrono::steady_clock::time_point reference_;

	// メンバ関数
	//------------------------------------------------------------------------
	// 初期化関連
	//------------------------------------------------------------------------
	static void EnableDebugLayer();
	void CreateDevice();
	void SetInfoQueueBreakOnSeverity() const;
	void CreateCommandQueue();
	void CreateSwapChain();
	void CreateDescriptorHeaps();
	void CreateRTV();
	void CreateDSV();
	void CreateCommandAllocator();
	void CreateCommandList();
	void CreateFence();

	void SetViewportAndScissor();

	void HandleDeviceLost();

	void PrepareForShutdown() const;

public:
	// -----------------------------------------------------------------------
	// Accessor
	// -----------------------------------------------------------------------
	ID3D12Device* GetDevice() const {
		return device_.Get();
	}

	ID3D12GraphicsCommandList* GetCommandList() const {
		return commandList_.Get();
	}

	ID3D12CommandQueue* GetCommandQueue() const {
		return commandQueue_.Get();
	}

	size_t GetBackBufferCount() const {
		return renderTargets_.size();
	}

	IDXGISwapChain4* GetSwapChain() const {
		return swapChain_.Get();
	}

	ID3D12Fence* GetFence() const {
		return fence_.Get();
	}

	ID3D12CommandAllocator* GetCommandAllocator() const {
		return commandAllocator_.Get();
	}

	uint64_t GetFenceValue() const {
		return fenceValue_;
	}

	void SetFenceValue(const uint64_t newValue) {
		fenceValue_ = newValue;
	};

	//------------------------------------------------------------------------
	// 汎用関数
	//------------------------------------------------------------------------
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
		bool shaderVisible
	) const;

private:
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		ID3D12DescriptorHeap* descriptorHeap,
		uint32_t descriptorSize, uint32_t index
	);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
		ID3D12DescriptorHeap* descriptorHeap,
		uint32_t descriptorSize, uint32_t index
	);
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource() const;
};

struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker();
};
