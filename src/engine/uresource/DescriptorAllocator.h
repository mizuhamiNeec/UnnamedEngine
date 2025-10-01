#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

#include <wrl/client.h>

namespace Unnamed {
	class DescriptorAllocator {
	public:
		~DescriptorAllocator() = default;

		void Init(
			ID3D12Device*              device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			uint32_t                   numDescriptors,
			bool                       shaderVisible = false
		);

		uint32_t Allocate();
		void     Free(uint32_t index);

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(
			uint32_t index) const;
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(
			uint32_t index) const;

		[[nodiscard]] ID3D12DescriptorHeap* GetHeap() const;

		[[nodiscard]] uint32_t Capacity() const;

		[[nodiscard]] uint32_t NumFree() const;

		[[nodiscard]] bool IsShaderVisible() const;

		[[nodiscard]] D3D12_DESCRIPTOR_HEAP_TYPE Type() const;

		void Reset();

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
		ID3D12Device*                                mDevice = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE                   mType   =
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		bool mShaderVisible = false;

		uint32_t mCapacity = 0;
		uint32_t mDescSize = 0;

		std::vector<uint32_t> mFreeList;
	};
}
