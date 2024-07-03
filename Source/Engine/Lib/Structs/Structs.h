#pragma once

#include <cstdint>
#include <d3d12.h>

#include "../Math/Matrix/Mat4.h"
#include "../Math/Vector/Vec2.h"
#include "../Math/Vector/Vec3.h"
#include "../Math/Vector/Vec4.h"

struct Vertex {
	Vec4 position; // 座標
	Vec2 texcoord; // テクスチャ座標
	Vec3 normal; // 法線

	static const D3D12_INPUT_LAYOUT_DESC inputLayout;

private:
	static constexpr int inputElementCount = 3;
	static const D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount];
};

struct Material {
	Vec4 color;
	int32_t enableLighting;
	float padding[3];
	Mat4 uvTransform;
};

struct TransformationMatrix {
	Mat4 WVP; // ワールドビュープロジェクション
	Mat4 World; // ワールド
};