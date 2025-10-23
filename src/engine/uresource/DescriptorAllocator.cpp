#include <pch.h>

#include <engine/subsystem/console/Log.h>
#include <engine/uresource/DescriptorAllocator.h>

namespace Unnamed {
	constexpr std::string_view kChannel = "DescriptorAllocator";

	namespace {
		/// @brief シェーダー可視かどうかを判定
		/// @param type デスクリプタヒープタイプ
		/// @return シェーダー可視ならtrueを返す
		bool CanShaderVisible(
			const D3D12_DESCRIPTOR_HEAP_TYPE type
		) {
			return type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
				type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		}
	}

	/// @brief 初期化
	/// @param device デバイス
	/// @param type デスクリプタヒープタイプ
	/// @param numDescriptors デスクリプタ数
	/// @param shaderVisible シェーダー可視フラグ
	void DescriptorAllocator::Init(
		ID3D12Device*                    device,
		const D3D12_DESCRIPTOR_HEAP_TYPE type,
		const uint32_t                   numDescriptors,
		const bool                       shaderVisible
	) {
		UASSERT(device != nullptr);
		mDevice = device;
		mType   = type;

		mShaderVisible = shaderVisible && CanShaderVisible(mType);

		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = numDescriptors;
		desc.Type           = mType;
		desc.Flags          = mShaderVisible ?
			             D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE :
			             D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		UASSERT(
			SUCCEEDED(
				mDevice->CreateDescriptorHeap(
					&desc, IID_PPV_ARGS(&mHeap)
				)
			)
		);

		mDescSize = mDevice->GetDescriptorHandleIncrementSize(mType);
		mCapacity = numDescriptors;

		// LIFO!
		mFreeList.resize(mCapacity);
		for (uint32_t i = 0; i < mCapacity; ++i) {
			mFreeList[i] = mCapacity - 1 - i; // 逆順に初期化
		}

		Msg(
			kChannel,
			"DescriptorAllocator initialized: type={}, capacity={}, shaderVisible={}",
			std::to_string(mType),
			std::to_string(mCapacity),
			mShaderVisible ? "true" : "false"
		);
	}

	/// @brief デスクリプタを割り当てる
	/// @return 割り当てられたデスクリプタのインデックス
	uint32_t DescriptorAllocator::Allocate() {
		if (mFreeList.empty()) {
			UASSERT(
				false && "DescriptorAllocator: No free descriptors available.");
			return UINT32_MAX; // すべてのデスクリプタが使用中
		}
		const uint32_t index = mFreeList.back();
		mFreeList.pop_back();
		return index;
	}

	/// @brief デスクリプタを解放する
	/// @param index 解放するデスクリプタのインデックス
	void DescriptorAllocator::Free(uint32_t index) {
		if (index == UINT32_MAX || index >= mCapacity) {
#ifdef _DEBUG
			if (std::ranges::find(mFreeList, index) != mFreeList.end()) {
				Warning(
					kChannel,
					"あんさん、2重に開放してへん? index: {}, capacity: {}",
					std::to_string(index), std::to_string(mCapacity)
				);
				UASSERT(false && "DescriptorAllocator: あんさん、2重に開放してへん?");
				return;
			}
#endif

			mFreeList.emplace_back(index);
		}
	}

	/// @brief CPUデスクリプタハンドルを取得する
	/// @param index デスクリプタのインデックス
	/// @return CPUデスクリプタハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::CPUHandle(
		const uint32_t index
	) const {
		UASSERT(mHeap);
		UASSERT(index < mCapacity);
		D3D12_CPU_DESCRIPTOR_HANDLE base = mHeap->
			GetCPUDescriptorHandleForHeapStart();
		base.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(mDescSize);
		return base;
	}

	/// @brief GPUデスクリプタハンドルを取得する
	/// @param index デスクリプタのインデックス
	/// @return GPUデスクリプタハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GPUHandle(
		const uint32_t index
	) const {
		UASSERT(
			mShaderVisible &&
			"GPU Handle requested from non-shader-visible heap."
		);
		D3D12_GPU_DESCRIPTOR_HANDLE base = mHeap->
			GetGPUDescriptorHandleForHeapStart();
		base.ptr += static_cast<UINT64>(index) * static_cast<UINT64>(mDescSize);
		return base;
	}

	/// @brief デスクリプタヒープを取得する
	/// @return デスクリプタヒープ
	ID3D12DescriptorHeap* DescriptorAllocator::GetHeap() const {
		return mHeap.Get();
	}

	/// @brief デスクリプタアロケータの容量を取得する
	/// @return デスクリプタアロケータの容量
	uint32_t DescriptorAllocator::Capacity() const { return mCapacity; }

	/// @brief 空きデスクリプタ数を取得する
	/// @return 空きデスクリプタ数
	uint32_t DescriptorAllocator::NumFree() const {
		return static_cast<uint32_t>(mFreeList.size());
	}

	/// @brief シェーダー可視かどうかを取得する
	/// @return シェーダー可視ならtrueを返す
	bool DescriptorAllocator::IsShaderVisible() const { return mShaderVisible; }

	/// @brief デスクリプタヒープタイプを取得する
	/// @return デスクリプタヒープタイプ
	D3D12_DESCRIPTOR_HEAP_TYPE DescriptorAllocator::Type() const {
		return mType;
	}

	/// @brief アロケータをリセットする
	void DescriptorAllocator::Reset() {
		mFreeList.clear();
		mFreeList.resize(mCapacity);
		for (uint32_t i = 0; i < mCapacity; ++i) {
			mFreeList[i] = mCapacity - 1 - i;
		}
	}
}
