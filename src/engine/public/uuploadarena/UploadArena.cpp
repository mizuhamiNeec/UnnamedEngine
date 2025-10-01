#include <pch.h>

#include "UploadArena.h"

#include <core/memory/MemUtil.h>
#include <engine/public/subsystem/console/Log.h>
#include <runtime/core/Properties.h>

namespace Unnamed {
	constexpr std::string_view kChannel = "UploadArena";

	bool UploadArena::Init(ID3D12Device* device, const uint64_t sizePerFrame) {
		mFrames = std::max<uint32_t>(1, kFrameBufferCount);

		// 最低64KB
		mSizePerFrame = std::max<uint64_t>(sizePerFrame, 64ull * 1024);

		const uint64_t              total = mSizePerFrame * mFrames;
		const D3D12_HEAP_PROPERTIES hp    = {D3D12_HEAP_TYPE_UPLOAD};
		D3D12_RESOURCE_DESC         rd    = {};
		rd.Dimension                      = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width                          = total;
		rd.Height                         = 1;
		rd.DepthOrArraySize               = 1;
		rd.MipLevels                      = 1;
		rd.Format                         = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc                     = {.Count = 1, .Quality = 0};
		rd.Layout                         = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		if (
			FAILED(
				device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
					D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&
						mBuffer)
				)
			)
		) {
			return false;
		}

		constexpr D3D12_RANGE noRead = {0, 0};
		if (
			FAILED(
				mBuffer->Map(0, &noRead, reinterpret_cast<void**>(&mMapped)))
		) {
			mBuffer.Reset(); // TODO: いる?
			return false;
		}

		mBaseGPUVirtualAddress = mBuffer->GetGPUVirtualAddress();

		mRetireFenceValues.assign(mFrames, 0);
		mRetireFences.assign(mFrames, nullptr);
		mFrameOffsets.assign(mFrames, 0);
		mCurrentFrame = 0;
		return true;
	}

	void UploadArena::Shutdown() {
		if (mBuffer) {
			mBuffer->Unmap(0, nullptr);
			mMapped = nullptr;
			mBuffer.Reset();
			mRetireFences.clear();
			mRetireFenceValues.clear();
			mFrameOffsets.clear();
			mFrames                = 0;
			mSizePerFrame          = 0;
			mBaseGPUVirtualAddress = 0;
			mCurrentFrame          = 0;
		}
	}

	bool UploadArena::BeginFrame(const uint32_t backIndex) {
		mCurrentFrame = backIndex % mFrames;

		// フェンスが設定されていない場合はそのまま使う
		auto* f = mRetireFences[mCurrentFrame].Get();
		if (f) {
			if (f->GetCompletedValue() < mRetireFenceValues[mCurrentFrame]) {
				return false;
			}
		}
		mFrameOffsets[mCurrentFrame] = 0;
		return true;
	}

	void UploadArena::OnFrameSubmitted(
		const uint32_t backIndex, ID3D12Fence* fence, const uint64_t value
	) {
		const uint32_t frameIndex      = backIndex % mFrames;
		mRetireFences[frameIndex]      = fence;
		mRetireFenceValues[frameIndex] = value;
	}

	UploadArena::Slice UploadArena::Allocate(
		const uint64_t size, const uint64_t alignment
	) {
		const uint32_t frameIndex = mCurrentFrame;
		const uint64_t base       = mSizePerFrame * frameIndex;
		const uint64_t offset     = MemUtil::AlignUp(
			mFrameOffsets[frameIndex], alignment
		);
		if (offset + size > mSizePerFrame) {
			// 容量不足
			Warning(
				kChannel,
				"アリーナの容量が足りません (size={}, alignment={}, frameIndex={}, used={}, capacity={})",
				std::to_string(size), std::to_string(alignment),
				std::to_string(frameIndex),
				std::to_string(offset), std::to_string(mSizePerFrame)
			);
			return {};
		}
		mFrameOffsets[frameIndex] = offset + size;
		Slice slice;
		slice.cpu               = mMapped + base + offset;
		slice.gpuVirtualAddress = mBaseGPUVirtualAddress + base + offset;
		slice.offset            = base + offset;
		return slice;
	}
}
