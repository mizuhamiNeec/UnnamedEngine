#pragma once

#include "base/BaseComponent.h"

#include "core/math/Vec3.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	/// @brief DirectionalLightComponentは、方向光の向き、色、強度、shadow設定を描画入力へ提供します
	class DirectionalLightComponent final : public BaseComponent {
	public:
		/// @brief ライト色を設定します。
		void SetColor(const Vec3& color) noexcept;

		/// @brief ライト色を取得します。
		[[nodiscard]] const Vec3& GetColor() const noexcept;

		/// @brief ライト強度を設定します。
		void SetIntensity(float intensity) noexcept;

		/// @brief ライト強度を取得します。
		[[nodiscard]] float GetIntensity() const noexcept;

		/// @brief ShadowMap caster として使うかを設定します。
		void SetCastsShadow(bool castsShadow) noexcept;

		/// @brief ShadowMap caster として使うかを取得します。
		[[nodiscard]] bool GetCastsShadow() const noexcept;

		/// @brief RenderFrameInputs 用の DirectionalLightInput を構築します。
		[[nodiscard]] bool BuildLightInput(
			Render::DirectionalLightInput& outLight
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
		Vec3  mColor       = Vec3::one;
		float mIntensity   = 1.0f;
		bool  mCastsShadow = true;
	};
}
