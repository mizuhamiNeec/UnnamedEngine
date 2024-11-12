#pragma once

#include <memory>

#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Structs/Structs.h"
#include "../Model/Model.h"

class Camera;
class ModelCommon;
class ConstantBuffer;
struct Vertex;
class VertexBuffer;
class Object3DCommon;

class Object3D {

	// マテリアル
	struct Material {
		Vec4 color;
		int32_t enableLighting;
		float padding[3];
		Mat4 uvTransform;
	};

	// 座標変換行列データ
	struct TransformationMatrix {
		Mat4 wvp;
		Mat4 world;
	};

	// 指向性ライト
	struct DirectionalLight {
		Vec4 color;
		Vec3 direction;
		float intensity;
	};

public:
	void Init(Object3DCommon* object3DCommon, ModelCommon* modelCommon);
	void Update();
	void Draw() const;

	// Setter
	void SetModel(Model* model);
	void SetModel(const std::string& filePath);
	void SetCamera(Camera* camera) { this->camera_ = camera; }
	void SetScale(const Vec3& scale) { transform_.scale = scale; }
	void SetRot(const Vec3& newRot) { transform_.rotate = newRot; }
	void SetPos(const Vec3& newPos) { transform_.translate = newPos; }

	void SetLighting(const bool& newLighting) const;

	// Getter
	const Vec3& GetScale() const { return transform_.scale; }
	const Vec3& GetRot() const { return transform_.rotate; }
	const Vec3& GetPos() const { return transform_.translate; }
	const Mat4& GetWorldMat() const;
	Transform& GetTransform();

protected:
	Camera* camera_ = nullptr;

	Object3DCommon* object3DCommon_ = nullptr;
	ModelCommon* modelCommon_ = nullptr;

	Model* model_ = nullptr;

	Mat4 worldMat_ = Mat4::Identity();

	Transform transform_ = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	TransformationMatrix* transformationMatrixData_ = nullptr; // 座標変換行列のポインタ
	DirectionalLight* directionalLightData_ = nullptr; // 指向性ライトのポインタ

	std::unique_ptr<ConstantBuffer> transformationMatrixConstantBuffer_;
	std::unique_ptr<ConstantBuffer> directionalLightConstantBuffer_;
};

