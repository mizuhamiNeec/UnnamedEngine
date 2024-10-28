#include "Camera.h"

#include "Math/Matrix/Mat4.h"
#include "Structs/Structs.h"

void Camera::Update() {
	// transform_からアフィン変換行列を作成
	worldMat_ = Mat4::Affine(
		transform_.scale,
		transform_.rotate,
		transform_.translate
	);

	// worldMatの逆行列
	viewMat_ = worldMat_.Inverse();
}
