#include "RegistryDescriptorResolver.h"

#include "RgResourceRegistry.h"

namespace Unnamed::Render {
	RegistryDescriptorResolver::RegistryDescriptorResolver(
		const RgResourceRegistry& reg
	) : mReg(reg) {
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RegistryDescriptorResolver::GetSrv(
		const uint32_t textureId
	) const {
		return mReg.GetSrv(textureId);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RegistryDescriptorResolver::GetUav(
		const uint32_t textureId
	) const {
		return mReg.GetUav(textureId);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RegistryDescriptorResolver::GetRtvCpu(
		const uint32_t textureId
	) const {
		return mReg.GetRtvCpu(textureId);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RegistryDescriptorResolver::GetDsvCpu(
		const uint32_t textureId
	) const {
		return mReg.GetDsvCpu(textureId);
	}

	ID3D12Resource* RegistryDescriptorResolver::GetResource(
		const uint32_t textureId
	) const {
		return mReg.GetResource(textureId);
	}
}
