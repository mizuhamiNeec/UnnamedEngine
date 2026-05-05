#pragma once

#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class TransformComponent;

	class TriggerVolumeComponentBase : public BaseComponent {
	public:
		/// @brief ローカル中心座標(Hu)を返します。
		[[nodiscard]] Vec3 GetLocalCenterHu() const noexcept {
			return mLocalCenterHu;
		}

		/// @brief ローカル中心座標(メートル)を返します。
		[[nodiscard]] Vec3 GetLocalCenter() const noexcept;

		[[nodiscard]] Vec3 GetExtentsHu() const noexcept {
			return mExtentsHu;
		}

		[[nodiscard]] Vec3 GetWorldCenter() const noexcept;
		[[nodiscard]] Vec3 GetWorldHalfExtentsMeters() const noexcept;

	protected:
		void DeserializeVolume(const JsonReader& reader);
		void SerializeVolume(JsonWriter& writer) const;
		[[nodiscard]] TransformComponent* GetTransform() const;

#ifdef _DEBUG
		void DrawVolumeInspectorImGui();
#endif

		Vec3 mLocalCenterHu = Vec3::zero;
		Vec3 mExtentsHu     = Vec3(32.0f, 32.0f, 32.0f);
	};
}
