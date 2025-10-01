#pragma once
#include <cstdint>
#include <d3d12.h>
#include <vector>

#include <wrl/client.h>

namespace Unnamed {
	class UploadArena {
	public:
		struct Slice {
			void*                     cpu               = nullptr; // 書き込み先
			D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress = 0;
			uint64_t                  offset            = 0; // リソース先頭からのオフセット
		};

		bool Init(ID3D12Device* device, uint64_t sizePerFrame);
		void Shutdown();
		bool BeginFrame(uint32_t backIndex);
		void OnFrameSubmitted(
			uint32_t backIndex, ID3D12Fence* fence,
			uint64_t value
		);
		Slice Allocate(uint64_t size, uint64_t alignment);
		[[nodiscard]] ID3D12Resource* Resource() const { return mBuffer.Get(); }
		[[nodiscard]] uint64_t SizePerFrame() const { return mSizePerFrame; }
		[[nodiscard]] uint32_t Frames() const { return mFrames; }

	private:

		Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
		uint8_t*                               mMapped                = nullptr;
		uint64_t                               mBaseGPUVirtualAddress = 0;

		uint64_t mSizePerFrame = 0;
		uint32_t mFrames       = 0;
		uint32_t mCurrentFrame = 0;

		std::vector<uint64_t> mFrameOffsets; // フレームごとのオフセット
		// フレームごとのフェンス
		std::vector<Microsoft::WRL::ComPtr<ID3D12Fence>> mRetireFences;
		std::vector<uint64_t>                            mRetireFenceValues;
	};
}
