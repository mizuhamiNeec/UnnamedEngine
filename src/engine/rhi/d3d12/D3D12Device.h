#pragma once
#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <wrl/client.h>

#include "D3D12FrameUploadAllocator.h"

#include "engine/Properties.h"
#include "engine/rhi/DxcShaderCompiler.h"
#include "engine/rhi/RhiTypes.h"
#include "engine/rhi/interface/IRhiDevice.h"

namespace Unnamed::Render {
	class PipelineCache;
	class ShaderLibrary;
}

namespace Unnamed::Rhi {
	class DxcShaderCompiler;
	static constexpr uint32_t kMaxFramesInFlight = 3; // 最大同時フレーム数

	class D3D12Device final : public IRhiDevice {
	public:
		D3D12Device(
			HWND                 hwnd, const DeviceDesc& deviceDesc,
			const SwapChainDesc& swapChainDesc
		);
		~D3D12Device() override;

		void BeginFrame() override;
		void EndFrame() override;
		void OnResize(uint32_t width, uint32_t height) override;

		[[nodiscard]] BACKEND_TYPE GetBackendType() const override;
		IRhiSwapChain&             GetSwapChain() override;

		[[nodiscard]] ID3D12GraphicsCommandList* GetCommandList() const;
		[[nodiscard]] ID3D12CommandQueue*        GetGraphicsQueue() const;
		[[nodiscard]] class D3D12SwapChain*      GetD3D12SwapChain() const;

		[[nodiscard]] ID3D12DescriptorHeap* GetSrvUavHeap() const;
		[[nodiscard]] uint32_t GetSrvUavDescriptorSize() const;
		[[nodiscard]] uint32_t GetSrvUavHeapCapacity() const;
		[[nodiscard]] uint32_t GetSrvUavTotalHeapCapacity() const;
		[[nodiscard]] uint32_t GetImGuiSrvHeapBase() const;
		[[nodiscard]] uint32_t GetImGuiSrvHeapCapacity() const;
		[[nodiscard]] uint32_t AllocateImGuiSrvSlot();
		[[nodiscard]] uint32_t GetCurrentFrameIndex() const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSrvUavCpuHandle(
			uint32_t absoluteSlot
		) const;
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrvUavGpuHandle(
			uint32_t absoluteSlot
		) const;

		[[nodiscard]] ID3D12DescriptorHeap* GetRtvHeap() const;
		[[nodiscard]] uint32_t              GetRtvDescriptorSize() const;
		[[nodiscard]] uint32_t              GetRtvHeapCapacity() const;

		[[nodiscard]] ID3D12DescriptorHeap* GetDsvHeap() const;
		[[nodiscard]] uint32_t              GetDsvDescriptorSize() const;
		[[nodiscard]] uint32_t              GetDsvHeapCapacity() const;

		[[nodiscard]] ID3D12RootSignature* GetCsRootSignature() const;
		[[nodiscard]] ID3D12RootSignature* GetFsRootSignature() const;
		[[nodiscard]] ID3D12RootSignature* GetGeomRootSignature() const;
		[[nodiscard]] ID3D12RootSignature* GetGeomRootSignatureLinearClamp()
			const;
		[[nodiscard]] ID3D12RootSignature* GetGeomRootSignaturePointClamp()
			const;

		[[nodiscard]] ID3D12Device* GetDevice() const;
		DxcShaderCompiler&          GetDxcCompiler();

		D3D12FrameUploadAllocator& GetFrameUploadAllocator();
		[[nodiscard]] uint32_t     GetFramesInFlight() const;

		class UploadContext {
		public:
			explicit UploadContext(D3D12Device& device);
			~UploadContext();

			UploadContext(const UploadContext&)            = delete;
			UploadContext& operator=(const UploadContext&) = delete;
			UploadContext(UploadContext&&)                 = default;
			UploadContext& operator=(UploadContext&&)      = default;

			void     Begin();
			uint64_t EndAndSubmitAndWait();

			ID3D12GraphicsCommandList* GetCommandList() const {
				return mCommandList.Get();
			}

		private:
			D3D12Device& mDevice;

			Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    mAllocator;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
			Microsoft::WRL::ComPtr<ID3D12Fence>               mFence;

			uint64_t mFenceValue = 0;
			HANDLE   mFenceEvent = nullptr;
		};

		UploadContext& GetUploadContext();

	private:
		static void EnableDebugLayer(bool gpuValidation);
		void        CreateDevice();
		void        EnableValidationLayer() const;
		void        CreateQueue();
		void        CreateSrvUavHeap();
		void        CreateRtvHeap();
		void        CreateDsvHeap();
		/// @brief 使用するルートシグネチャ群を作成します。
		void        CreatePipelines();
		/// @brief コンピュート用ルートシグネチャを作成します。
		void        CreateComputeRootSignature();
		/// @brief フルスクリーン描画用ルートシグネチャを作成します。
		void        CreateFullscreenRootSignature();
		/// @brief ジオメトリ描画用ルートシグネチャを作成します。
		void        CreateGeometryRootSignature();
		void        CreateCommandObjects();
		void        CreateFenceObjects();
		void        WaitForFrame(uint32_t frameIndex) const;
		void        SignalFrame(uint32_t frameIndex);
		void        WaitForGpuIdle();

	public:
		[[nodiscard]] uint64_t GetCompletedFenceValue() const;
		[[nodiscard]] uint64_t GetCurrentFrameFenceValue() const;
		[[nodiscard]] uint64_t GetLastSubmittedFenceValue(
			uint32_t frameIndex
		) const;
		[[nodiscard]] uint64_t GetNextSignalFenceValue() const;

	private:
		// D3D12
		Microsoft::WRL::ComPtr<IDXGIFactory6>      mFactory;
		Microsoft::WRL::ComPtr<ID3D12Device>       mDevice;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mGraphicsQueue;

		std::unique_ptr<UploadContext> mUploadContext;

		// スワップチェーン
		std::unique_ptr<D3D12SwapChain> mSwapChain;

		// コマンドリスト
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

		// フレームコンテキスト
		struct FrameContext {
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
			uint64_t                                       fenceValue = 0;

			D3D12FrameUploadAllocator upload;
		};

		std::array<FrameContext, kMaxFramesInFlight> mFrames = {};
		uint32_t                                     mFramesInFlight;

		// フェンス
		uint64_t                            mNextFenceValue = 1;
		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		HANDLE                              mFenceEvent = nullptr;

		HWND mHwnd = nullptr;

		// アダプタ変更イベント
		HANDLE                                mAdapterChangeEvent = nullptr;
		Microsoft::WRL::ComPtr<IDXGIFactory7> mFactory7;
		DWORD                                 mAdapterChangeEventCookie = 0;

		bool mEnableVsync = false;

		DxcShaderCompiler mDxcCompiler;

		// CBV/SRV/UAV のヒープ
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvUavHeap;
		uint32_t                                     mSrvUavDescriptorSize = 0;
		uint32_t                                     mSrvUavHeapCapacity   = 0;
		uint32_t                                     mSrvUavTotalCapacity  = 0;
		uint32_t                                     mImguiSrvHeapBase     = 0;
		uint32_t                                     mImguiSrvHeapCapacity = 0;
		uint32_t                                     mImguiNextSlot        = 0;

		// RTVのヒープ
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		uint32_t                                     mRtvDescriptorSize = 0;
		uint32_t                                     mRtvHeapCapacity   = 0;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;
		uint32_t                                     mDsvDescriptorSize = 0;
		uint32_t                                     mDsvHeapCapacity   = 0;

		// パイプライン
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mCsRootSignature;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mFsRootSignature;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mGeomRootSignature;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mGeomRootSignatureLinearClamp;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mGeomRootSignaturePointClamp;
	};
}
