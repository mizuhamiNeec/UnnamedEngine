#pragma once

#include <d3d12.h>
#include <string_view>

namespace Unnamed {
	void DumpRootSignatureDesc(
		const D3D12_ROOT_SIGNATURE_DESC& d,
		std::string_view                 tag
	);
}
