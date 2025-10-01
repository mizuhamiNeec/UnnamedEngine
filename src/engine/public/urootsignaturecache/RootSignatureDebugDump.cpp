#include "RootSignatureDebugDump.h"

#include <engine/public/subsystem/console/Log.h>

namespace Unnamed {
	static constexpr std::string_view VisName(const D3D12_SHADER_VISIBILITY v) {
		switch (v) {
		case D3D12_SHADER_VISIBILITY_ALL: return "ALL";
		case D3D12_SHADER_VISIBILITY_VERTEX: return "VS";
		case D3D12_SHADER_VISIBILITY_PIXEL: return "PS";
		case D3D12_SHADER_VISIBILITY_GEOMETRY: return "GS";
		case D3D12_SHADER_VISIBILITY_HULL: return "HS";
		case D3D12_SHADER_VISIBILITY_DOMAIN: return "DS";
		default: return "?";
		}
	}

	static bool ValidateDescAndLog(const D3D12_ROOT_SIGNATURE_DESC& d,
	                               std::string_view                 tag) {
		constexpr std::string_view kChannel = "RSDump";
		if (d.NumParameters && d.pParameters == nullptr) {
			DevMsg(kChannel,
			       "[{}] Invalid desc: NumParameters={} but pParameters=null",
			       tag, d.NumParameters);
			return false;
		}
		if (d.NumStaticSamplers && d.pStaticSamplers == nullptr) {
			DevMsg(kChannel,
			       "[{}] Invalid desc: NumStaticSamplers={} but pStaticSamplers=null",
			       tag, d.NumStaticSamplers);
			// サンプラは今回見ないので続行
		}
		for (UINT i = 0; i < d.NumParameters; ++i) {
			const auto& p = d.pParameters[i];
			if (p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
				const auto& t = p.DescriptorTable;
				if (t.NumDescriptorRanges && t.pDescriptorRanges == nullptr) {
					DevMsg(kChannel,
					       "[{}] Invalid table param#{}: ranges={} but ptr=null",
					       tag, i, t.NumDescriptorRanges);
					return false;
				}
			}
		}
		return true;
	}

	void DumpRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC& d,
	                           std::string_view                 tag) {
		constexpr std::string_view kChannel = "RSDump";

		if (!ValidateDescAndLog(d, tag)) {
			DevMsg(kChannel, "[{}] Abort dump due to invalid desc.", tag);
			return;
		}

		DevMsg(kChannel, "--- {} --- Params={}, Samplers={}, Flags=0x{:08x}",
		       tag, d.NumParameters, d.NumStaticSamplers,
		       static_cast<unsigned int>(d.Flags));

		for (UINT i = 0; i < d.NumParameters; ++i) {
			const auto& p = d.pParameters[i];
			switch (p.ParameterType) {
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
				DevMsg(kChannel, " #{}: CBV  b{} space{} vis={}", i,
				       p.Descriptor.ShaderRegister, p.Descriptor.RegisterSpace,
				       VisName(p.ShaderVisibility));
				break;
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
				DevMsg(kChannel, " #{}: SRV  t{} space{} vis={}", i,
				       p.Descriptor.ShaderRegister, p.Descriptor.RegisterSpace,
				       VisName(p.ShaderVisibility));
				break;
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				DevMsg(kChannel, " #{}: UAV  u{} space{} vis={}", i,
				       p.Descriptor.ShaderRegister, p.Descriptor.RegisterSpace,
				       VisName(p.ShaderVisibility));
				break;
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
				const auto& t = p.DescriptorTable;
				DevMsg(kChannel, " #{}: TABLE vis={} (ranges={})", i,
				       VisName(p.ShaderVisibility), t.NumDescriptorRanges);

				if (t.NumDescriptorRanges == 0 || t.pDescriptorRanges ==
					nullptr) {
					DevMsg(kChannel, "          - (no ranges)");
					break;
				}

				for (UINT r = 0; r < t.NumDescriptorRanges; ++r) {
#if defined(_MSC_VER) && !defined(NDEBUG)
					__try {
#endif
						const auto&      rg = t.pDescriptorRanges[r];
						std::string_view rt;
						switch (rg.RangeType) {
						case D3D12_DESCRIPTOR_RANGE_TYPE_SRV: rt = "SRV";
							break;
						case D3D12_DESCRIPTOR_RANGE_TYPE_CBV: rt = "CBV";
							break;
						case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: rt = "UAV";
							break;
						case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER: rt = "SMP";
							break;
						default: rt = "?";
							break;
						}
						DevMsg(kChannel,
						       "          - {} base={} count={} space={}", rt,
						       rg.BaseShaderRegister, rg.NumDescriptors,
						       rg.RegisterSpace);
#if defined(_MSC_VER) && !defined(NDEBUG)
					} __except (EXCEPTION_EXECUTE_HANDLER) {
						DevMsg(kChannel,
						       "          - [AV while reading range #{}] ptr={} count={}",
						       r, static_cast<const void*>(t.pDescriptorRanges),
						       t.NumDescriptorRanges);
						break;
					}
#endif
				}
				break;
			}
			default:
				DevMsg(kChannel, " #{}: (Unknown root parameter type {})", i,
				       static_cast<int>(p.ParameterType));
				break;
			}
		}
	}
}
