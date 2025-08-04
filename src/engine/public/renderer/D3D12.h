#pragma once
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

#include <wrl/client.h>

#include <math/public/MathLib.h>

#include "engine/public/utils/Properties.h"
#include "engine/public/Window/Base/BaseWindow.h"

class SrvManager;

struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker();
};

struct RenderTargetTexture {
	Microsoft::WRL::ComPtr<ID3D12Resource>      rtv;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	uint32_t                    srvIndex;
};

struct DepthStencilTexture {
	Microsoft::WRL::ComPtr<ID3D12Resource>      dsv;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	uint32_t                    srvIndex;
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

class D3D12 {
public: // メンバ関数
	D3D12(BaseWindow* window);
	~D3D12();

	void Init();
	void Shutdown();

	void SetShaderResourceViewManager(SrvManager* srvManager);

	void ClearColorAndDepth();
	void PreRender();
	void PostRender();

	[[nodiscard]] RenderTargetTexture CreateRenderTargetTexture(
		uint32_t    width, uint32_t height, Vec4 clearColor,
		DXGI_FORMAT format = kBufferFormat);
	
	// リサイズ時用：古いSRVインデックスを返却して新しいものを作成
	// 使用例: newRtv = CreateRenderTargetTexture(w, h, clearColor, oldRtv.srvIndex);
	[[nodiscard]] RenderTargetTexture CreateRenderTargetTexture(
		uint32_t    width, uint32_t height, Vec4 clearColor,
		uint32_t    oldSrvIndex, DXGI_FORMAT format = kBufferFormat);
	
	[[nodiscard]] DepthStencilTexture CreateDepthStencilTexture(
		uint32_t    width, uint32_t height,
		DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
	
	// リサイズ時用：古いSRVインデックスを返却して新しいものを作成
	// 使用例: newDsv = CreateDepthStencilTexture(w, h, oldDsv.srvIndex);
	[[nodiscard]] DepthStencilTexture CreateDepthStencilTexture(
		uint32_t    width, uint32_t height, uint32_t oldSrvIndex,
		DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
	void BeginRenderPass(const RenderPassTargets& targets) const;

	void                        BeginSwapChainRenderPass();
	D3D12_CPU_DESCRIPTOR_HANDLE GetSwapChainRenderTargetView() const;

	void ResetCommandList();

	static void WriteToUploadHeapMemory(ID3D12Resource* resource, uint32_t size,
		const void* data);

	void WaitPreviousFrame();
	void Flush();

	void Resize(uint32_t width, uint32_t height);

	void ResetOffscreenRenderTextures();

	void SetViewportAndScissor(uint32_t width, uint32_t height);

private:
	D3DResourceLeakChecker mD3dResourceLeakChecker;

	// 持ってきたやつ
	SrvManager* mSrvManager = nullptr;
	BaseWindow* mWindow = nullptr;

	// メンバ変数
	// 1. デバイス・ファクトリ・スワップチェーン
	Microsoft::WRL::ComPtr<ID3D12Device>    mDevice;
	Microsoft::WRL::ComPtr<IDXGIFactory7>   mDxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> mSwapChain;

	// 2. コマンドキュー・アロケータ・リスト
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    mCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>        mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	// 3. ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvDescriptorHeap;

	// 4. レンダーターゲット・深度ステンシル
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>      mRenderTargets;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> mRtvHandlesSwapChain;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> mRtvHandlesOffscreen;
	Microsoft::WRL::ComPtr<ID3D12Resource>                   mDepthStencilResource;
	DepthStencilTexture                      mDefaultDepthStencilTexture;

	// 5. フェンス等の同期
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	uint64_t            mFenceValue = 0;
	HANDLE              mFenceEvent = nullptr;
	UINT                mFrameIndex = 0;

	// 6. その他
	D3D12_RESOURCE_BARRIER mBarrier = {};

	D3D12_VIEWPORT mViewport = {};
	D3D12_RECT     mScissorRect = {};

	uint32_t mDescriptorSizeRtv = 0;
	uint32_t mDescriptorSizeDsv = 0;

	uint32_t mCurrentDsvIndex = 0;

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
	[[nodiscard]] ID3D12Device* GetDevice() const {
		return mDevice.Get();
	}

	[[nodiscard]] ID3D12GraphicsCommandList* GetCommandList() const {
		return mCommandList.Get();
	}

	[[nodiscard]] ID3D12CommandQueue* GetCommandQueue() const {
		return mCommandQueue.Get();
	}

	[[nodiscard]] size_t GetBackBufferCount() const {
		return mRenderTargets.size();
	}

	[[nodiscard]] IDXGISwapChain4* GetSwapChain() const {
		return mSwapChain.Get();
	}

	[[nodiscard]] ID3D12Fence* GetFence() const {
		return mFence.Get();
	}

	[[nodiscard]] ID3D12CommandAllocator* GetCommandAllocator() const {
		return mCommandAllocator.Get();
	}

	[[nodiscard]] uint64_t GetFenceValue() const {
		return mFenceValue;
	}

	void SetFenceValue(const uint64_t newValue) {
		mFenceValue = newValue;
	}

	//------------------------------------------------------------------------
	// 汎用関数
	//------------------------------------------------------------------------
	[[nodiscard]] Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
		bool                       shaderVisible
	) const;

private:
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		ID3D12DescriptorHeap* descriptorHeap,
		uint32_t              descriptorSize, uint32_t index
	);
	[[nodiscard]] Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
		uint32_t width, uint32_t height, DXGI_FORMAT format) const;
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
		ID3D12DescriptorHeap* descriptorHeap,
		uint32_t              descriptorSize, uint32_t index
	);

	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE AllocateNewRTVHandle();
};
