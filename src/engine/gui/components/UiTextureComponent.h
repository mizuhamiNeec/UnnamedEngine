#pragma once

#include "UiComponent.h"

#include "core/filesystem/Path.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed::Gui {
	class UiTextureComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "Texture";
		}

		void                      SetTexturePath(Path path);
		[[nodiscard]] const Path& GetTexturePath() const;

		void                       SetColor(const Color& color);
		[[nodiscard]] const Color& GetColor() const;

		void                      SetUvMin(const Vec2& uvMin);
		[[nodiscard]] const Vec2& GetUvMin() const;

		void                      SetUvMax(const Vec2& uvMax);
		[[nodiscard]] const Vec2& GetUvMax() const;

		void                      SetAnchor(const Vec2& anchor);
		[[nodiscard]] const Vec2& GetAnchor() const;

		void                SetRotationRad(float rotationRad);
		[[nodiscard]] float GetRotationRad() const;

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

	private:
		Path  mTexturePath;
		Color mColor       = {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f};
		Vec2  mUvMin       = Vec2(0.0f, 0.0f);
		Vec2  mUvMax       = Vec2(1.0f, 1.0f);
		Vec2  mAnchor      = Vec2(0.0f, 0.0f);
		float mRotationRad = 0.0f;
	};
}
