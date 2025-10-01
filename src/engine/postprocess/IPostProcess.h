#pragma once

#include <d3d12.h>
#include <cstdint>

struct PostProcessContext {
	ID3D12GraphicsCommandList*  commandList;  // コマンドリスト
	ID3D12Resource*             inputTexture; // 適用するテクスチャ
	D3D12_CPU_DESCRIPTOR_HANDLE outRtv;       // 出力先のRTVハンドル
	uint32_t                    width;        // テクスチャの幅
	uint32_t                    height;       // テクスチャの高さ
};


class IPostProcess {
public:
	virtual ~IPostProcess() = default;

	virtual void Update(float) {
	}

	virtual void Execute(const PostProcessContext& context) = 0;
};
