#include "UCameraComponent.h"

#include <engine/public/gameframework/component/Transform/TransformComponent.h>
#include <engine/public/utils/json/JsonReader.h>
#include <engine/public/utils/json/JsonWriter.h>

namespace Unnamed {
	Mat4 UCameraComponent::View(
		const TransformComponent* transformComponent
	) {
		const Mat4 w = transformComponent ?
			               transformComponent->WorldMat() :
			               Mat4::identity;
		return w.Inverse();
	}

	Mat4 UCameraComponent::Proj(const float aspectRatio) const {
		return Mat4::PerspectiveFovMat(
			fovY, aspectRatio, zNear, zFar
		);
	}

	void UCameraComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fov");
		writer.Write(fovY);
		writer.Key("zNear");
		writer.Write(zNear);
		writer.Key("zFar");
		writer.Write(zFar);
	}

	void UCameraComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("fov")) { fovY = reader["fov"].GetFloat(); }
		if (reader.Has("zNear")) { zNear = reader["zNear"].GetFloat(); }
		if (reader.Has("zFar")) { zFar = reader["zFar"].GetFloat(); }
	}
}
