#pragma once

#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <assimp/vector3.h>

#include "core/math/Mat4.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

namespace Unnamed::AssimpConversions {
	/// @brief Assimpの4x4行列をエンジンのMat4に変換します。
	/// @param matrix 変換するAssimp行列。
	/// @return エンジン側のMat4。
	[[nodiscard]] Mat4 ToMat4(const aiMatrix4x4& matrix);

	/// @brief Assimpの3DベクトルをエンジンのVec3に変換します。
	/// @param vector 変換するAssimpベクトル。
	/// @return エンジン側のVec3。
	[[nodiscard]] Vec3 ToVec3(const aiVector3D& vector);

	/// @brief AssimpのクォータニオンをエンジンのQuaternionに変換します。
	/// @param quaternion 変換するAssimpクォータニオン。
	/// @return 正規化済みのエンジン側Quaternion。
	[[nodiscard]] Quaternion ToQuaternion(const aiQuaternion& quaternion);
}
