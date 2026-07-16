#include "D3D12Util.h"

#include <stdexcept>

#include "engine/platform/WindowsUtils.h"

namespace Unnamed::Rhi {
	void Throw(const HRESULT hr) {
		if (FAILED(hr)) {
			throw std::runtime_error(
				"D3D12 HRESULT failed: " +
				WindowsUtils::GetHresultMessage(hr)
			);
		}
	}

	const char* ToD3D12SemanticName(const VertexSemantic semantic) {
		switch (semantic) {
			case VertexSemantic::POSITION: return "POSITION";
			case VertexSemantic::NORMAL: return "NORMAL";
			case VertexSemantic::TANGENT: return "TANGENT";
			case VertexSemantic::COLOR: return "COLOR";
			case VertexSemantic::TEXCOORD: return "TEXCOORD";
			default: return "UNKNOWN_SEMANTIC";
		}
	}

	DXGI_FORMAT ToDxgiFormat(const VertexFormat format) {
		switch (format) {
			case VertexFormat::FLOAT1: return DXGI_FORMAT_R32_FLOAT;
			case VertexFormat::FLOAT2: return DXGI_FORMAT_R32G32_FLOAT;
			case VertexFormat::FLOAT3: return DXGI_FORMAT_R32G32B32_FLOAT;
			case VertexFormat::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case VertexFormat::U_BYTE4_N: return DXGI_FORMAT_R8G8B8A8_UNORM;
			default: return DXGI_FORMAT_R32G32B32_FLOAT;
		}
	}

	DXGI_FORMAT ToDxgiFormat(const TEXTURE_FORMAT format) {
		switch (format) {
			case TEXTURE_FORMAT::R8G8B8A8_UNORM: return
					DXGI_FORMAT_R8G8B8A8_UNORM;

			case TEXTURE_FORMAT::R10G10B10A2_UNORM: return
					DXGI_FORMAT_R10G10B10A2_UNORM;
			default: return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
	}

	TEXTURE_FORMAT ToTextureFormat(const DXGI_FORMAT format) {
		switch (format) {
			case DXGI_FORMAT_R8G8B8A8_UNORM: return
					TEXTURE_FORMAT::R8G8B8A8_UNORM;

			case DXGI_FORMAT_R10G10B10A2_UNORM: return
					TEXTURE_FORMAT::R10G10B10A2_UNORM;
			default: return TEXTURE_FORMAT::R8G8B8A8_UNORM;
		}
	}
}
