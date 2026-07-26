#pragma once

#include <cstdint>
#include <d3d12.h>

namespace Unnamed::Render {
	/// @brief IDescriptorResolverは、RenderGraph resource handleからframe有効なSRV・UAV descriptorを解決します
	class IDescriptorResolver {
	public:
		virtual ~IDescriptorResolver() = default;

		[[nodiscard]] virtual
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(uint32_t textureId) const = 0;
		[[nodiscard]] virtual
		D3D12_GPU_DESCRIPTOR_HANDLE GetUav(uint32_t textureId) const = 0;
		[[nodiscard]] virtual
		D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCpu(uint32_t textureId) const = 0;
		[[nodiscard]] virtual
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpu(uint32_t textureId) const = 0;

		[[nodiscard]]
		virtual ID3D12Resource* GetResource(uint32_t textureId) const = 0;
	};
}
