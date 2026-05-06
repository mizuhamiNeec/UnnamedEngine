#include "CheckpointComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#include <array>
#include <algorithm>
#include <cstring>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

namespace Unnamed {
	Vec3 CheckpointComponent::GetRespawnOffsetHu() const noexcept {
		if (mLegacyRespawnPositionValid) {
			return Math::MtoH(mLegacyRespawnPositionWorld - GetWorldCenter());
		}
		return mRespawnOffsetHu;
	}

	Vec3 CheckpointComponent::GetRespawnPosition() const noexcept {
		return mLegacyRespawnPositionValid ?
			       mLegacyRespawnPositionWorld :
			       GetWorldCenter() + Math::HtoM(mRespawnOffsetHu);
	}

	void CheckpointComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader index = reader["index"];
		if (index.Valid()) {
			mIndex = index.GetInt();
		}

		if (const JsonReader respawnOffsetHu = reader["respawnOffsetHu"];
			respawnOffsetHu.Valid()) {
			mRespawnOffsetHu           = respawnOffsetHu.GetVec3(mRespawnOffsetHu);
			mLegacyRespawnPositionValid = false;
		} else if (const JsonReader respawnPosition = reader["respawnPosition"];
		           respawnPosition.Valid()) {
			// 旧シーン互換: 既存のワールド座標を優先して保持します。
			mLegacyRespawnPositionWorld = respawnPosition.GetVec3(
				mLegacyRespawnPositionWorld
			);
			mLegacyRespawnPositionValid = true;
		}
		mCourseId        = reader["courseId"].GetString(mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
	}

	void CheckpointComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("index");
		writer.Write(mIndex);
		writer.WriteVec3("respawnOffsetHu", GetRespawnOffsetHu());
		writer.Key("courseId");
		writer.Write(mCourseId);
	}

#ifdef _DEBUG
	void CheckpointComponent::DrawInspectorImGui() {
		DrawVolumeInspectorImGui();
		ImGui::InputInt("Index", &mIndex);
		Vec3 respawnOffsetHu = GetRespawnOffsetHu();
		if (ImGui::DragFloat3("Respawn Offset (Hu)", &respawnOffsetHu.x, 1.0f)) {
			mRespawnOffsetHu            = respawnOffsetHu;
			mLegacyRespawnPositionValid = false;
		}
		std::array<char, 64> courseIdBuffer = {};
		const size_t copyLen                = std::min(
			mCourseId.size(),
			courseIdBuffer.size() - 1
		);
		if (copyLen > 0) {
			std::memcpy(courseIdBuffer.data(), mCourseId.data(), copyLen);
		}
		if (ImGui::InputText(
			"Course Id",
			courseIdBuffer.data(),
			courseIdBuffer.size()
		)) {
			mCourseId = courseIdBuffer.data();
		}
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
	}
#endif

	REGISTER_COMPONENT(CheckpointComponent);
}
