#include "GoalComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#include <array>
#include <algorithm>
#include <cstring>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

namespace Unnamed {
	void GoalComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader index = reader["index"];
		if (index.Valid()) {
			mIndex = index.GetInt();
		}
		mCourseId = reader["courseId"].GetString(mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
	}

	void GoalComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("index");
		writer.Write(mIndex);
		writer.Key("courseId");
		writer.Write(mCourseId);
	}

#ifdef _DEBUG
	void GoalComponent::DrawInspectorImGui() {
		DrawVolumeInspectorImGui();
		ImGui::InputInt("Index", &mIndex);
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

	REGISTER_COMPONENT(GoalComponent);
}
