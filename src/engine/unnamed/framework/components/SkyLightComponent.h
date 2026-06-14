#pragma once

#include "base/BaseComponent.h"

#include "core/math/Vec3.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class SkyLightComponent final : public BaseComponent {
	public:
		/// @brief 空側 Hemisphere Ambient 色を設定します。
		void SetSkyColor(const Vec3& color) noexcept;

		/// @brief 空側 Hemisphere Ambient 色を取得します。
		[[nodiscard]] const Vec3& GetSkyColor() const noexcept;

		/// @brief 地面側 Hemisphere Ambient 色を設定します。
		void SetGroundColor(const Vec3& color) noexcept;

		/// @brief 地面側 Hemisphere Ambient 色を取得します。
		[[nodiscard]] const Vec3& GetGroundColor() const noexcept;

		/// @brief 環境光強度を設定します。
		void SetIntensity(float intensity) noexcept;

		/// @brief 環境光強度を取得します。
		[[nodiscard]] float GetIntensity() const noexcept;

		/// @brief RenderFrameInputs 用の EnvironmentLightInput を構築します。
		[[nodiscard]] bool BuildLightInput(
			Render::EnvironmentLightInput& outLight
		) const;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		Vec3  mSkyColor    = Vec3(0.25f, 0.30f, 0.40f);
		Vec3  mGroundColor = Vec3(0.08f, 0.07f, 0.06f);
		float mIntensity   = 0.3f;
	};
}
