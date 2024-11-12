#pragma once
#include "../Lib/Structs/Structs.h"
#include "../Lib/Math/MathLib.h"
#include "../Lib/Utils/ClientProperties.h"

class Window;

class Camera {
public:
	Camera();

	void Update();

	// Setter
	void SetTransform(const Transform& newTransform);
	void SetPos(const Vec3& newPos);
	void SetRot(const Vec3& newRot);
	void SetFovVertical(const float& newFovVertical);
	void SetNearZ(const float& newNearZ);
	void SetFarZ(const float& newFarZ);
	void SetAspectRatio(float newAspectRatio);

	// Getter
	Transform& GetTransform();
	Vec3& GetPos();
	Vec3& GetRotate();
	float& GetFovVertical();
	float& GetZNear();
	float& GetZFar();
	Mat4& GetViewProjMat();
	Vec3 GetForward();

	Vec3 ScreenToWorld(const Vec3& screenPos, float z) const {
		// 1. スクリーン空間からNDC（Normalized Device Coordinates）に変換
		float ndcX = (screenPos.x / kClientWidth) * 2.0f - 1.0f;
		float ndcY = 1.0f - (screenPos.y / kClientHeight) * 2.0f;  // Y軸を逆にする
		Vec4 ndcPos(ndcX, ndcY, z, 1.0f);

		// 2. ビュープロジェクション行列の逆行列を取得
		Mat4 invViewProj = (projMat_ * viewMat_).Inverse();

		// 3. NDC座標をワールド座標に変換
		Vec4 worldPos = ndcPos * invViewProj;

		// 4. w成分で正規化して3Dベクトルに変換
		if (worldPos.w != 0.0f) {
			worldPos /= worldPos.w;
		}

		return Vec3(worldPos.x, worldPos.y, worldPos.z);
	}

private:
	float fov_ = 90.0f * Math::deg2Rad;
	float aspectRatio_ = 0.0f;
	float zNear_ = 0.1f;
	float zFar_ = 10000.0f;

	Transform transform_{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	Mat4 worldMat_;
	Mat4 viewMat_;
	Mat4 projMat_;
	Mat4 viewProjMat_;
};
