#include "DebugDumpShader.h"

#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <string>

#include "engine/public/subsystem/console/Log.h"

namespace Unnamed {
	static const char* ResClassName(D3D_SHADER_INPUT_TYPE t) {
		switch (t) {
		case D3D_SIT_CBUFFER: return "CBV";
		case D3D_SIT_TBUFFER: return "TB";
		case D3D_SIT_TEXTURE: return "SRV";
		case D3D_SIT_SAMPLER: return "SMP";
		case D3D_SIT_UAV_RWTYPED: return "UAV";
		case D3D_SIT_STRUCTURED: return "UAV(STR)";
		case D3D_SIT_UAV_RWSTRUCTURED: return "UAV(RWSTR)";
		case D3D_SIT_BYTEADDRESS: return "UAV(BY)";
		case D3D_SIT_UAV_RWBYTEADDRESS: return "UAV(RWBY)";
		case D3D_SIT_UAV_APPEND_STRUCTURED: return "UAV(APP)";
		case D3D_SIT_UAV_CONSUME_STRUCTURED: return "UAV(CNS)";
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: return "UAV(RWSTRC)";
		default: return "?";
		}
	}

	void DumpShaderResources(
		const D3D12_SHADER_BYTECODE& bc, const char* tag
	) {
		ID3D12ShaderReflection* refl = nullptr;
		if (
			FAILED(
				D3DReflect(
					bc.pShaderBytecode,
					bc.BytecodeLength,
					IID_PPV_ARGS(&refl)
				)
			)
		) {
			return;
		}

		D3D12_SHADER_DESC sd{};
		refl->GetDesc(&sd);

		std::string kChannel = "SHDump";

		DevMsg(
			kChannel,
			"--- {} --- Resources={}, CBuffers={}",
			tag ? tag : "",
			sd.BoundResources,
			sd.ConstantBuffers
		);

		for (UINT i = 0; i < sd.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC bd{};
			refl->GetResourceBindingDesc(i, &bd);
			DevMsg(
				kChannel,
				" {} name={} reg={} space={} count={}",
				ResClassName(bd.Type), bd.Name, bd.BindPoint, bd.Space,
				bd.BindCount
			);
		}
		refl->Release();
	}

	static bool RS_HasCBV_For(const D3D12_ROOT_SIGNATURE_DESC& rs, UINT reg,
	                          UINT space, D3D12_SHADER_VISIBILITY stage) {
		for (UINT i = 0; i < rs.NumParameters; ++i) {
			auto& p = rs.pParameters[i];
			if (!(p.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL || p.
				ShaderVisibility == stage))
				continue;
			if (p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV) {
				if (p.Descriptor.ShaderRegister == reg && p.Descriptor.
					RegisterSpace == space)
					return true;
			} else if (p.ParameterType ==
				D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
				for (UINT r = 0; r < p.DescriptorTable.NumDescriptorRanges; ++
				     r) {
					auto& rg = p.DescriptorTable.pDescriptorRanges[r];
					if (rg.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV) {
						const UINT end = (rg.NumDescriptors == UINT(-1)) ?
							                 0xffffffffu :
							                 (rg.BaseShaderRegister + rg.
								                 NumDescriptors);
						if (rg.RegisterSpace == space && reg >= rg.
							BaseShaderRegister && reg < end)
							return true;
					}
				}
			}
		}
		return false;
	}

	bool ValidateRSvsShader(
		const D3D12_ROOT_SIGNATURE_DESC& rs,
		const D3D12_SHADER_BYTECODE&     bc,
		D3D12_SHADER_VISIBILITY          stage,
		const char*                      tag
	) {
		ID3D12ShaderReflection* refl = nullptr;
		if (FAILED(
			D3DReflect(bc.pShaderBytecode, bc.BytecodeLength, IID_PPV_ARGS(&refl
			))))
			return true;
		D3D12_SHADER_DESC sd{};
		refl->GetDesc(&sd);

		bool ok = true;
		for (UINT i = 0; i < sd.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC bd{};
			refl->GetResourceBindingDesc(i, &bd);
			if (bd.Type == D3D_SIT_CBUFFER) {
				if (!RS_HasCBV_For(rs, bd.BindPoint, bd.Space, stage)) {
					DevMsg(
						"VALIDATE",
						"[{}] MISSING CBV b{} space{} (stage={})",
						tag ? tag : "", bd.BindPoint, bd.Space,
						(stage == D3D12_SHADER_VISIBILITY_VERTEX ?
							 "VS" :
							 "PS")
					);

					ok = false;
				}
			}
		}
		refl->Release();
		return ok;
	}
}
