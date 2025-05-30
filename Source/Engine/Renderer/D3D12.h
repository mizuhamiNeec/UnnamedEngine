#pragma once
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

#include <wrl/client.h>

#include <Renderer/Renderer.h>

#include <Lib/Math/Vector/Vec4.h>

#include <ResourceSystem/SRV/ShaderResourceViewManager.h>

using namespace Microsoft::WRL;

struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker();
};

struct RenderTargetTexture {
	ComPtr<ID3D12Resource>      rtv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	DescriptorHandles           srvHandles;
};

struct DepthStencilTexture {
	ComPtr<ID3D12Resource> dsv;
	DescriptorHandles      handles;
};

struct RenderPassTargets {
	D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs; // 複数可
	UINT                         numRTVs;
	D3D12_CPU_DESCRIPTOR_HANDLE* pDSV; // nullptr でもOK
	Vec4                         clearColor;
	float                        clearDepth;
	uint8_t                      clearStencil;
	bool                         bClearColor; // クリアカラーするか?
	bool                         bClearDepth; // 深度クリアするか?
};

class D3D12 : public Renderer {
public: // メンバ関数
	D3D12(BaseWindow* window);
	~D3D12() override;

	void Init() override;
	void Shutdown() override;

	void SetShaderResourceViewManager(ShaderResourceViewManager* srvManager);

	void ClearColorAndDepth() const;
	void PreRender() override;
	void PostRender() override;

	[[nodiscard]] RenderTargetTexture CreateRenderTargetTexture(
		uint32_t    width, uint32_t height, Vec4 clearColor,
		DXGI_FORMAT format = kBufferFormat);
	[[nodiscard]] DepthStencilTexture CreateDepthStencilTexture(
		uint32_t    width, uint32_t height,
		DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
	void BeginRenderPass(const RenderPassTargets& targets) const;

	void                        BeginSwapChainRenderPass() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSwapChainRenderTargetView() const;
	
	void ResetCommandList();

	static void WriteToUploadHeapMemory(ID3D12Resource* resource, uint32_t size,
	                                    const void*     data);

	void WaitPreviousFrame();

	void Resize(uint32_t width, uint32_t height);

private:
	D3DResourceLeakChecker d3dResourceLeakChecker_;

	// 持ってきたやつ
	BaseWindow*                window_     = nullptr;
	ShaderResourceViewManager* srvManager_ = nullptr;

	// メンバ変数
	// 1. デバイス・ファクトリ・スワップチェーン
	ComPtr<ID3D12Device>    device_;
	ComPtr<IDXGIFactory7>   dxgiFactory_;
	ComPtr<IDXGISwapChain4> swapChain_;

	// 2. コマンドキュー・アロケータ・リスト
	ComPtr<ID3D12CommandQueue>        commandQueue_;
	ComPtr<ID3D12CommandAllocator>    commandAllocator_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;

	// 3. ディスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	// 4. レンダーターゲット・深度ステンシル
	std::vector<ComPtr<ID3D12Resource>>      renderTargets_;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles_;
	ComPtr<ID3D12Resource>                   depthStencilResource_;
	DepthStencilTexture                      defaultDepthStencilTexture_;

	// 5. フェンス等の同期
	ComPtr<ID3D12Fence> fence_;
	uint64_t            fenceValue_ = 0;
	HANDLE              fenceEvent_ = nullptr;
	UINT                frameIndex_ = 0;

	// 6. その他
	D3D12_RESOURCE_BARRIER barrier_ = {};

	D3D12_VIEWPORT viewport_    = {};
	D3D12_RECT     scissorRect_ = {};

	uint32_t descriptorSizeRTV = 0;
	uint32_t descriptorSizeDSV = 0;

	uint32_t currentDSVIndex_ = 0;

	// メンバ関数
	//------------------------------------------------------------------------
	// 初期化関連
	//------------------------------------------------------------------------
	static
	void EnableDebugLayer();
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
	}

	//------------------------------------------------------------------------
	// 汎用関数
	//------------------------------------------------------------------------
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
		bool                       shaderVisible
	) const;

private:
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		ID3D12DescriptorHeap* descriptorHeap,
		uint32_t              descriptorSize, uint32_t index
	);
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
		uint32_t width, uint32_t height, DXGI_FORMAT format) const;
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
		ID3D12DescriptorHeap* descriptorHeap,
		uint32_t              descriptorSize, uint32_t index
	);

	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE AllocateNewRTVHandle();
};
