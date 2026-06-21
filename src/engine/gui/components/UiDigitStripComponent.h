#pragma once

#include "UiComponent.h"

#include "core/filesystem/Path.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed::Gui {
	class UiDigitStripComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "DigitStrip";
		}

		void SetStripTexturePath(Path path);
		[[nodiscard]] const Path& GetStripTexturePath() const;

		void SetValue(int value);
		[[nodiscard]] int GetValue() const;

		void SetMinDigits(int minDigits);
		[[nodiscard]] int GetMinDigits() const;

		void SetDigitSpacing(float spacingPx);
		[[nodiscard]] float GetDigitSpacing() const;

		void SetColor(const Color& color);
		[[nodiscard]] const Color& GetColor() const;

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

	private:
		Path mStripTexturePath;
		int         mValue = 0;
		int         mMinDigits = 2;
		float       mDigitSpacing = 0.0f;
		Color       mColor = {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f};
	};
}
