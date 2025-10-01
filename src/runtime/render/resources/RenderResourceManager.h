#pragma once
#include <cstdint>
#include <d3d12.h>
#include <deque>
#include <unordered_map>

#include <engine/urenderer/GraphicsDevice.h>

#include <runtime/assets/core/UAssetID.h>

#include <wrl/client.h>

namespace Unnamed {
	class GraphicsDevice;
	class UAssetManager;
	class UploadArena;

	struct TextureHandle {
		uint32_t           id  = UINT32_MAX;
		uint32_t           gen = 0; // テクスチャの世代
		[[nodiscard]] bool IsValid() const { return id != UINT32_MAX; }
	};

	class RenderResourceManager {
	public:
		explicit RenderResourceManager(
			GraphicsDevice* gd, UAssetManager* am, UploadArena* arena
		);

		TextureHandle AcquireTexture(
			AssetID                    asset,
			ID3D12GraphicsCommandList* commandList
		);

		void ProcessDeferredMipUploads(ID3D12GraphicsCommandList* commandList);

		void ReleaseTexture(TextureHandle handle, ID3D12Fence* fence,
		                    uint64_t      value);

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGPU(
			TextureHandle handle
		) const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCPU(
			TextureHandle handle
		) const;

		UAssetManager* GetAssetManager() const;

		void TrackUpload(
			const Microsoft::WRL::ComPtr<ID3D12Resource>& resource);

		void FlushUploads(ID3D12Fence* fence, uint64_t value);
		void GarbageCollect();

		[[nodiscard]] uint32_t GpuRefCount(TextureHandle handle) const;
		[[nodiscard]] size_t   VramUsageBytes() const;

		struct GpuVB {
			BufferHandle handle;
			UINT         stride = 0;
		};

		struct GpuIB {
			BufferHandle handle;
			DXGI_FORMAT  format = DXGI_FORMAT_R32_UINT;
		};

		bool CreateStaticVertexBuffer(
			const void* data, size_t bytes, UINT stride, GpuVB& out
		) const;

		bool CreateStaticIndexBuffer(
			const void* data, size_t bytes, DXGI_FORMAT fmt, GpuIB& out
		) const;

		void BindVertexBuffer(ID3D12GraphicsCommandList* cmd,
		                      const GpuVB&               vb) const;
		void BindIndexBuffer(ID3D12GraphicsCommandList* cmd,
		                     const GpuIB&               ib) const;

	private:
		struct GpuTexture {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			uint32_t                               srvIndex = UINT32_MAX;
			uint32_t                               refs     = 0;
			Microsoft::WRL::ComPtr<ID3D12Fence>    retireFence;
			uint64_t                               retireValue = 0;
			bool                                   alive       = false;
			uint32_t                               gen         = 1;

			AssetID     sourceAsset = kInvalidAssetID;
			uint32_t    w           = 0;
			uint32_t    h           = 0;
			DXGI_FORMAT format      = DXGI_FORMAT_R8G8B8A8_UNORM;
			size_t      vramBytes   = 0;
		};

	private:
		bool UploadRGBA8_1Mip(
			GpuTexture&                texture,
			const void*                pixels,
			size_t                     rowPitchBytes,
			uint32_t                   w, uint32_t h,
			ID3D12GraphicsCommandList* commandList
		) const;

	private:
		GraphicsDevice*                            mGd           = nullptr;
		UAssetManager*                             mAssetManager = nullptr;
		UploadArena*                               mArena        = nullptr;
		mutable std::mutex                         mMutex;
		std::vector<GpuTexture>                    mTextures;
		std::vector<uint32_t>                      mFreeTextureList;
		std::unordered_map<AssetID, TextureHandle> mAssetToTexture;

		// フレームで発生したアップロード
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mUploadsThisFrame;

		// フェンスの後に開放する待ち
		struct PendingUpload {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			Microsoft::WRL::ComPtr<ID3D12Fence>    fence;
			uint64_t                               value = 0;
		};

		std::vector<PendingUpload> mPendingUploads;

		struct PendingMipUpload {
			TextureHandle                         tex;
			uint32_t                              mip = 0; // 上げるミップレベル
			std::shared_ptr<std::vector<uint8_t>> data;    // ミップのコピー
			uint32_t                              width    = 0;
			uint32_t                              height   = 0;
			size_t                                rowPitch = 0;
		};

		std::deque<PendingMipUpload> mDeferredMipUploads;
	};
}
