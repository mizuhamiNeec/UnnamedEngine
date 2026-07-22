#pragma once
#include <dxgiformat.h>

#include "engine/rhi/PipelineKey.h"
#include "engine/rhi/RhiTypes.h"

namespace Unnamed::Rhi {
	void           Throw(HRESULT hr);
	/// @brief RHIの頂点セマンティクスをD3D12入力レイアウト用の名前へ変換します。
	/// @param semantic 変換する頂点セマンティクス。
	/// @return D3D12入力要素で参照するセマンティクス名。
	const char*    ToD3D12SemanticName(VertexSemantic semantic);
	/// @brief RHIの頂点フォーマットをDXGI_FORMATへ変換します。
	/// @param format 変換する頂点フォーマット。
	/// @return 対応するDXGI_FORMAT。
	DXGI_FORMAT    ToDxgiFormat(VertexFormat format);
	DXGI_FORMAT    ToDxgiFormat(TEXTURE_FORMAT format);
	TEXTURE_FORMAT ToTextureFormat(DXGI_FORMAT format);
}
