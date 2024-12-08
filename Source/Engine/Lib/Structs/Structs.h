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
	Vec2 uv; // テクスチャ座標
	Vec3 normal; // 法線

	static const D3D12_INPUT_LAYOUT_DESC inputLayout;

private:
	static constexpr int inputElementCount = 3;
	static const D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount];
};

struct Material {
	Vec4 color;
	int32_t enableLighting;
	Vec3 padding;
	Mat4 uvTransform;
	float shininess;
	Vec3 specularColor;
};

struct TransformationMatrix {
	Mat4 wvp; // ワールドビュープロジェクション
	Mat4 world; // ワールド
};

struct ParticleForGPU {
	Mat4 wvp;
	Mat4 world;
	Vec4 color;
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

struct PointLight {
	Vec4 color; //!< ライトの色
	Vec3 position; //!< ライトの位置
	float intensity; //!< 輝度
	float radius; //!< ライトの届く最大距離
	float decay; //!< 減衰率
	float padding[2];
};

struct SpotLight {
	Vec4 color; //!< ライトの色
	Vec3 position; //!< ライトの位置
	float intensity; //!< 輝度
	Vec3 direction; //!< スポットライトの方向
	float distance; //!< ライトの届く最大距離
	float decay; //!< 減衰率
	float cosAngle; //!< スポットライトの余弦
	float cosFalloffStart;
	float padding[2];
};

struct Transform {
	Vec3 scale;
	Vec3 rotate;
	Vec3 translate;
};

// カメラの座標をGPUへ送る用
struct CameraForGPU {
	Vec3 worldPosition;
};

struct Particle {
	Transform transform;
	Vec3 vel;
	Vec4 color;
	float lifeTime;
	float currentTime;
	Vec3 drag;
	Vec3 gravity;
};

struct AABB {
	Vec3 min;
	Vec3 max;
};

struct Emitter {
	Transform transform; // エミッタのトランスフォーム
	uint32_t count; // 発生数
	float frequency; // 発生頻度
	float frequencyTime; // 頻度用時刻
	Vec3 size; // エミッターのサイズ
};

struct AccelerationField {
	Vec3 acceleration; // 加速度
	AABB area; // 範囲
};
