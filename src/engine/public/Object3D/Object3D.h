#pragma once

#include <runtime/core/math/Math.h>
#include <engine/public/renderer/Structs.h>
#include <engine/public/Model/Model.h>
#include <engine/public/Model/ModelCommon.h>

#include <memory>

class Model;
class Camera;
class CameraComponent;
class ModelCommon;
class ConstantBuffer;
struct Vertex;
class Object3DCommon;

class Object3D {
public:
	// マテリアル
	struct Material {
		Vec4    color;
		int32_t enableLighting;
		float   padding[3];
		Mat4    uvTransform;
	};

	// 座標変換行列データ
	struct TransformationMatrix {
		Mat4 wvp;
		Mat4 world;
		Mat4 worldInverseTranspose;
	};

	// 指向性ライト
	struct DirectionalLight {
		Vec4  color;
		Vec3  direction;
		float intensity;
	};

	void Init(Object3DCommon* object3DCommon);
	void Update();
	void Draw() const;

	// Setter
	void SetModel(Model* model);
	void SetModel(const std::string& filePath);

	void SetCamera(CameraComponent* camera) {
		this->mCamera = camera;
	}

	void SetScale(const Vec3& scale);
	void SetRot(const Vec3& newRot);
	void SetPos(const Vec3& newPos);

	// Getter
	[[nodiscard]] const Vec3& GetScale() const;
	[[nodiscard]] const Vec3& GetRot() const;
	[[nodiscard]] const Vec3& GetPos() const;

private:
	CameraComponent* mCamera = nullptr;

	Object3DCommon* mObject3DCommon = nullptr;

	Model* mModel = nullptr;

	Transform mTransform = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	TransformationMatrix* mTransformationMatrixData = nullptr; // 座標変換行列のポインタ
	DirectionalLight*     mDirectionalLightData = nullptr; // 指向性ライトのポインタ
	CameraForGPU*         mCameraForGPU = nullptr; // カメラのポインタ
	PointLight*           mPointLightData = nullptr; // ポイントライトのポインタ????
	SpotLight*            mSpotLightData = nullptr; // スポットライトのポインタ

	std::unique_ptr<ConstantBuffer> mTransformationMatrixConstantBuffer;
	std::unique_ptr<ConstantBuffer> mDirectionalLightConstantBuffer;
	std::unique_ptr<ConstantBuffer> mCameraConstantBuffer;
	std::unique_ptr<ConstantBuffer> mPointLightConstantBuffer;
	std::unique_ptr<ConstantBuffer> mSpotLightConstantBuffer;
};
