#include "Structs.h"

#include <dxgiformat.h>

const D3D12_INPUT_ELEMENT_DESC Vertex::inputElements[] = {
	{
		.SemanticName = "POSITION",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT
	},
	{
		.SemanticName = "TEXCOORD",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32_FLOAT,
		.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT
	},
	{
		.SemanticName = "NORMAL",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32B32_FLOAT,
		.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT
	},
};

const D3D12_INPUT_LAYOUT_DESC Vertex::inputLayout = {
	inputElements,
	inputElementCount
};