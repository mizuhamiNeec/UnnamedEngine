#pragma once

#include <memory>

#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Structs/Structs.h"
#include "../Model/Model.h"

class Camera;
class CameraComponent;
class ModelCommon;
class ConstantBuffer;
struct Vertex;
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
		Mat4 worldInverseTranspose;
	};

	// 指向性ライト
	struct DirectionalLight {
		Vec4 color;
		Vec3 direction;
		float intensity;
	};

public:
	void Init(Object3DCommon* object3DCommon);
	void Update();
	void Draw() const;

	// Setter
	void SetModel(Model* model);
	void SetModel(const std::string& filePath);

	void SetCamera(Camera* camera) {
		this->camera_ = camera;
	}

	void SetScale(const Vec3& scale) {
		transform_.scale = scale;
	}

	void SetRot(const Vec3& newRot) {
		transform_.rotate = newRot;
	}

	void SetPos(const Vec3& newPos) {
		transform_.translate = newPos;
	}

	// Getter
	const Vec3& GetScale() const {
		return transform_.scale;
	}

	const Vec3& GetRot() const {
		return transform_.rotate;
	}

	const Vec3& GetPos() const {
		return transform_.translate;
	}

private:
	Camera* camera_ = nullptr;

	Object3DCommon* object3DCommon_ = nullptr;

	Model* model_ = nullptr;

	Transform transform_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	TransformationMatrix* transformationMatrixData_ = nullptr; // 座標変換行列のポインタ
	DirectionalLight* directionalLightData_ = nullptr; // 指向性ライトのポインタ
	CameraForGPU* cameraForGPU_ = nullptr; // カメラのポインタ
	PointLight* pointLightData_ = nullptr; // ポイントライトのポインタ????
	SpotLight* spotLightData_ = nullptr; // スポットライトのポインタ

	std::unique_ptr<ConstantBuffer> transformationMatrixConstantBuffer_;
	std::unique_ptr<ConstantBuffer> directionalLightConstantBuffer_;
	std::unique_ptr<ConstantBuffer> cameraConstantBuffer_;
	std::unique_ptr<ConstantBuffer> pointLightConstantBuffer_;
	std::unique_ptr<ConstantBuffer> spotLightConstantBuffer_;
};
