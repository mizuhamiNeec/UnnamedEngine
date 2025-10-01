#pragma once

#include <d3d12.h>

namespace Unnamed {
	void DumpShaderResources(const D3D12_SHADER_BYTECODE& bc, const char* tag);
	bool ValidateRSvsShader(
		const D3D12_ROOT_SIGNATURE_DESC& rs,
		const D3D12_SHADER_BYTECODE&     bc,
		D3D12_SHADER_VISIBILITY          stage,
		const char*                      tag
	);
}
