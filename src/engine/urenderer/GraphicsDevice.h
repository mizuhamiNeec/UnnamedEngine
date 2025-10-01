#pragma once

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

#include <engine/uresource/DescriptorAllocator.h>
#include <runtime/core/Properties.h>

#include <runtime/core/math/Math.h>

#include <wrl/client.h>

namespace Unnamed {
	struct BufferHandle {
		uint32_t id = 0;
	};

	struct DescriptorHandle {
		uint32_t                    index = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu   = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu   = {};
	};

	struct FrameContext {
		ID3D12GraphicsCommandList* cmd       = nullptr;
		ID3D12CommandAllocator*    alloc     = nullptr;
		uint32_t                   backIndex = 0;
	};

	struct GraphicsDeviceInfo {
		void*    hWnd;
		uint32_t width;
		uint32_t height;
		bool     bEnableDebug;
	};

	class GraphicsDevice {
		struct PerFrame {
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    commandAllocator;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
			Microsoft::WRL::ComPtr<ID3D12Fence>               fence;
			HANDLE                                            event      = {};
			UINT64                                            fenceValue = 0;
		};

	public:
		GraphicsDevice() = default;

		bool Init(GraphicsDeviceInfo info);
		void Shutdown();

		[[nodiscard]] FrameContext BeginFrame(
			Vec4 clearColor = Vec4(0.2f, 0.2f, 0.2f, 1.0f)
		) const;

		void EndFrame(const FrameContext& ctx);

		[[nodiscard]] PerFrame GetFrameBuffer(uint32_t frameIndex) const;

		[[nodiscard]] FrameContext BeginImmediateFrame() const;

		BufferHandle CreateVertexBuffer(const void* data, size_t bytes);

		BufferHandle CreateIndexBuffer(
			const void* data, size_t bytes, DXGI_FORMAT format
		);

		void BindVertexBuffer(
			ID3D12GraphicsCommandList* commandList,
			BufferHandle               bufferHandle,
			UINT                       stride,
			UINT                       offset = 0
		) const;

		void BindIndexBuffer(
			ID3D12GraphicsCommandList* commandList,
			BufferHandle               bufferHandle,
			DXGI_FORMAT                format,
			UINT                       offset = 0
		) const;

		static void DrawIndexed(
			ID3D12GraphicsCommandList* commandList,
			UINT                       indexCount,
			UINT                       start              = 0,
			INT                        baseVertexLocation = 0
		);

		[[nodiscard]] ID3D12Device*    Device() const noexcept;
		[[nodiscard]] IDXGISwapChain4* SwapChain() const noexcept;

		[[nodiscard]] DescriptorAllocator* GetSrvAllocator() const;
		[[nodiscard]] DescriptorAllocator* GetSamplerAllocator() const;
		[[nodiscard]] DescriptorAllocator* GetRtvAllocator() const;
		[[nodiscard]] DescriptorAllocator* GetDsvAllocator() const;

	private:
		void WaitGPU(uint32_t frameIndex) const;

		BufferHandle CreateStaticBuffer(
			const void* data, size_t bytes, D3D12_RESOURCE_STATES initState
		);

		void CreateDepthBuffers(UINT width, UINT height);
		void DestroyDepthBuffers();

	private:
		struct DefferedFree {
			uint32_t index;
			uint64_t fence;
		};

		std::vector<DefferedFree> mSrvTrash;

		GraphicsDeviceInfo mInfo;

		struct GPUBuffer {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			UINT64                                 size = 0;
		};

		std::vector<GPUBuffer> mBuffers;

		HANDLE mAdapterChangeEvent;       // アダプター変更イベント
		DWORD  mAdapterChangeEventCookie; // アダプター変更イベントのコール

		Microsoft::WRL::ComPtr<ID3D12Device>       mDevice;
		Microsoft::WRL::ComPtr<IDXGISwapChain4>    mSwapChain;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;

		// フレームバッファ
		struct BackBuffer {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			D3D12_CPU_DESCRIPTOR_HANDLE            rtv = {};
		};

		std::array<BackBuffer, kFrameBufferCount> mBackBuffers;

		// 深度バッファ
		static constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

		struct DepthBuffer {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			D3D12_CPU_DESCRIPTOR_HANDLE            dsv = {};
		};

		std::array<DepthBuffer, kFrameBufferCount> mDepthBuffers;

		// デスクリプタアロケータ
		std::unique_ptr<DescriptorAllocator> mSrvAllocator;
		std::unique_ptr<DescriptorAllocator> mSamplerAllocator;
		std::unique_ptr<DescriptorAllocator> mRtvAllocator;
		std::unique_ptr<DescriptorAllocator> mDsvAllocator;

		std::array<PerFrame, kFrameBufferCount> mFrameContexts;

		uint32_t mBackBufferIndex = 0;
	};
}
