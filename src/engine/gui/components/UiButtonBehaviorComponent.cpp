#include "UiButtonBehaviorComponent.h"

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/gui/UiSerializationHelpers.h"
#include "engine/gui/UiWidget.h"

namespace Unnamed::Gui {
	void UiButtonBehaviorComponent::SetText(const std::string_view& text) {
		mText = text;
	}

	std::string_view UiButtonBehaviorComponent::GetText() const {
		return mText;
	}

	void UiButtonBehaviorComponent::SetOnClick(
		const std::function<void()>& callback
	) {
		mOnClick = callback;
	}

	void UiButtonBehaviorComponent::SetColors(
		const Color& normal,
		const Color& hovered,
		const Color& pressed
	) {
		mColorNormal  = normal;
		mColorHovered = hovered;
		mColorPressed = pressed;
	}

	const Color& UiButtonBehaviorComponent::GetNormalColor() const {
		return mColorNormal;
	}

	const Color& UiButtonBehaviorComponent::GetHoveredColor() const {
		return mColorHovered;
	}

	const Color& UiButtonBehaviorComponent::GetPressedColor() const {
		return mColorPressed;
	}

	void UiButtonBehaviorComponent::SetBorderColor(const Color& color) {
		mBorderColor = color;
	}

	const Color& UiButtonBehaviorComponent::GetBorderColor() const {
		return mBorderColor;
	}

	void UiButtonBehaviorComponent::SetTextColor(const Color& color) {
		mTextColor = color;
	}

	const Color& UiButtonBehaviorComponent::GetTextColor() const {
		return mTextColor;
	}

	void UiButtonBehaviorComponent::SetCornerRadius(const float radius) {
		mCornerRadius = radius;
	}

	float UiButtonBehaviorComponent::GetCornerRadius() const {
		return mCornerRadius;
	}

	void UiButtonBehaviorComponent::SetFontSize(const float size) {
		mFontSize = size;
	}

	float UiButtonBehaviorComponent::GetFontSize() const {
		return mFontSize;
	}

	void UiButtonBehaviorComponent::BuildDrawCommands(
		const UiWidget&             owner,
		std::vector<UiDrawCommand>& out
	) const {
		if (!owner.IsVisible()) {
			return;
		}

		Color background = mColorNormal;
		if (owner.IsPressed()) {
			background = mColorPressed;
		} else if (owner.IsHovered()) {
			background = mColorHovered;
		}

		const Rect& rect = owner.GetGlobalRect();

		UiDrawCommand rectCommand        = {};
		rectCommand.type                 = UI_DRAW_COMMAND_TYPE::RECT;
		rectCommand.rect.rect            = rect;
		rectCommand.rect.fillColor       = background;
		rectCommand.rect.cornerRadius    = mCornerRadius;
		rectCommand.rect.borderThickness = 1.0f;
		rectCommand.rect.borderColor     = mBorderColor;
		out.emplace_back(rectCommand);

		if (!mText.empty()) {
			UiDrawCommand textCommand = {};
			textCommand.type          = UI_DRAW_COMMAND_TYPE::TEXT;
			textCommand.text.text     = mText;
			textCommand.text.color    = mTextColor;
			textCommand.text.fontSize = mFontSize;
			textCommand.text.position = Vec2(
				rect.x + 8.0f,
				rect.y + rect.height * 0.5f
			);
			out.emplace_back(textCommand);
		}
	}

	void UiButtonBehaviorComponent::OnClick(UiWidget& owner) {
		(void)owner;
		if (mOnClick) {
			mOnClick();
		}
	}

	void UiButtonBehaviorComponent::Serialize(JsonWriter& writer) const {
		writer.Key("text");
		writer.Write(mText);
		writer.Key("normalColor");
		WriteColor(writer, mColorNormal);
		writer.Key("hoveredColor");
		WriteColor(writer, mColorHovered);
		writer.Key("pressedColor");
		WriteColor(writer, mColorPressed);
		writer.Key("borderColor");
		WriteColor(writer, mBorderColor);
		writer.Key("textColor");
		WriteColor(writer, mTextColor);
		writer.Key("cornerRadius");
		writer.Write(mCornerRadius);
		writer.Key("fontSize");
		writer.Write(mFontSize);
	}

	bool UiButtonBehaviorComponent::Deserialize(
		const JsonReader& reader, const UiDeserializeContext& context
	) {
		(void)context;
		if (reader.Has("text")) {
			mText = reader["text"].GetString();
		}
		if (reader.Has("normalColor")) {
			mColorNormal = ReadColor(reader["normalColor"], mColorNormal);
		}
		if (reader.Has("hoveredColor")) {
			mColorHovered = ReadColor(reader["hoveredColor"], mColorHovered);
		}
		if (reader.Has("pressedColor")) {
			mColorPressed = ReadColor(reader["pressedColor"], mColorPressed);
		}
		if (reader.Has("borderColor")) {
			mBorderColor = ReadColor(reader["borderColor"], mBorderColor);
		}
		if (reader.Has("textColor")) {
			mTextColor = ReadColor(reader["textColor"], mTextColor);
		}
		if (reader.Has("cornerRadius")) {
			mCornerRadius = reader["cornerRadius"].GetFloat();
		}
		if (reader.Has("fontSize")) {
			mFontSize = reader["fontSize"].GetFloat();
		}
		return true;
	}
}
