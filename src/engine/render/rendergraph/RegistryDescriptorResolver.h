#pragma once

#include "IDescriptorResolver.h"

namespace Unnamed::Render {
	class RgResourceRegistry;

	/// @brief RegistryDescriptorResolverは、RgResourceRegistryを参照してtexture handleのdescriptorを解決します
	class RegistryDescriptorResolver final : public IDescriptorResolver {
	public:
		explicit RegistryDescriptorResolver(const RgResourceRegistry& reg);

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(
			uint32_t textureId
		) const override;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetUav(
			uint32_t textureId
		) const override;

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCpu(
			uint32_t textureId
		) const override;

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpu(
			uint32_t textureId
		) const override;

		[[nodiscard]] ID3D12Resource*
		GetResource(uint32_t textureId) const override;

	private:
		const RgResourceRegistry& mReg;
	};
}
