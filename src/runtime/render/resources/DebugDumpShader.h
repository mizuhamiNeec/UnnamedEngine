#pragma once

#include <d3d12.h>

namespace Unnamed {
	/// @brief シェーダーリソースの内容をダンプします
	/// @param bc シェーダーバイトコード
	/// @param tag ダンプ時のタグ文字列
	void DumpShaderResources(const D3D12_SHADER_BYTECODE& bc, const char* tag);

	/// @brief ルートシグネチャとシェーダーの整合性を検証します
	/// @param rs ルートシグネチャ記述子
	/// @param bc シェーダーバイトコード
	/// @param stage シェーダーステージ
	/// @param tag 検証時のタグ文字列
	/// @return 整合性が取れている場合true
	bool ValidateRSvsShader(
		const D3D12_ROOT_SIGNATURE_DESC& rs,
		const D3D12_SHADER_BYTECODE&     bc,
		D3D12_SHADER_VISIBILITY          stage,
		const char*                      tag
	);
}
