#pragma once

#include <d3d12.h>
#include <string_view>

namespace Unnamed {
	/// @brief ルートシグネチャ記述子をダンプする
	/// @param d ルートシグネチャ記述子
	/// @param tag タグ文字列
	void DumpRootSignatureDesc(
		const D3D12_ROOT_SIGNATURE_DESC& d,
		std::string_view                 tag
	);
}
