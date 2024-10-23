#pragma once

#include <cstdint>
#include <d3d12.h>
#include <string>
#include <vector>

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
	Mat4 wvp; // ワールドビュープロジェクション
	Mat4 world; // ワールド
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<Vertex> vertices;
	MaterialData material;
};

struct DirectionalLight {
	Vec4 color; //!< ライトの色
	Vec3 direction; //!< ライトの向き
	float intensity; //!< 輝度
};

struct Transform {
	Vec3 scale;
	Vec3 rotate;
	Vec3 translate;
};
