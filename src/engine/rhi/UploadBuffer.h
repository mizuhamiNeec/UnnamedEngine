#pragma once
#include <cstdint>
#include <d3d12.h>

#include <wrl/client.h>

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Rhi {
	inline uint32_t AlignUp(const uint32_t v, const uint32_t a) {
		return v + (a - 1) & ~(a - 1);
	}

	/// @brief UploadBufferは、永続mapしたD3D12 upload heapを所有し、256 byte strideの要素書き込みを管理します
	template <typename T>
	class UploadBuffer {
	public:
		UploadBuffer() = default;

		void Init(
			ID3D12Device*  device, uint32_t elementCount,
			const wchar_t* debugName
		);

		void Shutdown();

		void Write(uint32_t index, const T& data);

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(
			uint32_t index
		) const;

		[[nodiscard]] ID3D12Resource* GetResource() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
		uint8_t*                               mMapped       = nullptr;
		uint32_t                               mElementCount = 0;
		uint32_t                               mStride       = 0;
		uint64_t                               mBufferSize   = 0;
	};

	template <typename T>
	void UploadBuffer<T>::Init(
		ID3D12Device*  device, const uint32_t elementCount,
		const wchar_t* debugName
	) {
		mElementCount = std::max(1u, elementCount);
		mStride       = AlignUp(static_cast<uint32_t>(sizeof(T)), 256u);
		mBufferSize   = static_cast<uint64_t>(mStride) * mElementCount;

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width               = mBufferSize;
		desc.Height              = 1;
		desc.DepthOrArraySize    = 1;
		desc.MipLevels           = 1;
		desc.SampleDesc.Count    = 1;
		desc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags               = D3D12_RESOURCE_FLAG_NONE;

		// バッファの作成
		HRESULT hr = device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mResource.ReleaseAndGetAddressOf())
		);
		if (FAILED(hr)) {
			Fatal("RHI", "UploadBuffer: バッファの作成に失敗しました (hr={:08x})", hr);
			return;
		}

		// デバッグ名の設定
		if (debugName) {
			mResource->SetName(debugName);
		}

		// 永続マップ
		hr = mResource->Map(0, nullptr, reinterpret_cast<void**>(&mMapped));
		if (FAILED(hr)) {
			Fatal("RHI", "UploadBuffer: バッファのマッピングに失敗しました (hr={:08x})", hr);
		}
	}

	template <typename T>
	void UploadBuffer<T>::Shutdown() {
		if (mResource && mMapped) {
			mResource->Unmap(0, nullptr);
			mMapped = nullptr;
			mResource.Reset();
			mElementCount = 0;
			mStride       = 0;
			mBufferSize   = 0;
		}
	}

	template <typename T>
	void UploadBuffer<T>::Write(const uint32_t index, const T& data) {
		if (!mMapped || index >= mElementCount) {
			return;
		}
		std::memcpy(
			mMapped + static_cast<uint64_t>(index) * mStride, &data,
			sizeof(T)
		);
	}

	template <typename T>
	D3D12_GPU_VIRTUAL_ADDRESS UploadBuffer<T>::GetGpuAddress(
		const uint32_t index
	) const {
		if (!mResource || index >= mElementCount) {
			return 0;
		}
		return mResource->GetGPUVirtualAddress() + static_cast<uint64_t>(
			       index) * mStride;
	}

	template <typename T>
	ID3D12Resource* UploadBuffer<T>::GetResource() const {
		return mResource.Get();
	}
}
