#include "UCameraComponent.h"

#include <core/json/JsonReader.h>
#include <core/json/JsonWriter.h>

#include <engine/gameframework/component/Transform/TransformComponent.h>

namespace Unnamed {
	/// @brief ビュー行列を取得する
	/// @param transformComponent トランスフォームコンポーネント
	/// @return ビュー行列
	Mat4 UCameraComponent::View(
		const TransformComponent* transformComponent
	) {
		const Mat4 w = transformComponent ?
			               transformComponent->WorldMat() :
			               Mat4::identity;
		return w.Inverse();
	}

	/// @brief 射影行列を取得する
	/// @param aspectRatio アスペクト比
	/// @return 射影行列
	Mat4 UCameraComponent::Proj(const float aspectRatio) const {
		return Mat4::PerspectiveFovMat(
			fovY, aspectRatio, zNear, zFar
		);
	}

	/// @brief シリアライズ処理
	/// @param writer JSONライター
	void UCameraComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fov");
		writer.Write(fovY);
		writer.Key("zNear");
		writer.Write(zNear);
		writer.Key("zFar");
		writer.Write(zFar);
	}

	/// @brief デシリアライズ処理
	/// @param reader JSONリーダー
	void UCameraComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("fov")) { fovY = reader["fov"].GetFloat(); }
		if (reader.Has("zNear")) { zNear = reader["zNear"].GetFloat(); }
		if (reader.Has("zFar")) { zFar = reader["zFar"].GetFloat(); }
	}
}
