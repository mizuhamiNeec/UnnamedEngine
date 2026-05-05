#pragma once

#include <string>

#include "TriggerVolumeComponentBase.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	class CheckpointComponent final : public TriggerVolumeComponentBase {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "parkour.Checkpoint";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Checkpoint";
		}

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] int32_t GetIndex() const noexcept {
			return mIndex;
		}

		/// @brief チェックポイント中心からのリスポーンオフセット(Hu)を返します。
		[[nodiscard]] Vec3 GetRespawnOffsetHu() const noexcept;

		/// @brief 現在のワールドリスポーン位置を返します。
		[[nodiscard]] Vec3 GetRespawnPosition() const noexcept;

		/// @brief 所属コースIDを返します。
		[[nodiscard]] const std::string& GetCourseId() const noexcept {
			return mCourseId;
		}

	private:
		int32_t mIndex           = 0;
		Vec3    mRespawnOffsetHu = Vec3::zero;
		Vec3    mLegacyRespawnPositionWorld = Vec3::zero;
		bool    mLegacyRespawnPositionValid = false;
		std::string mCourseId    = "default";
	};
}
