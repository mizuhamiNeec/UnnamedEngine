#include "UiPanelStyleComponent.h"

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/gui/UiWidget.h"

namespace Unnamed::Gui {
	namespace {
		void WriteColor(JsonWriter& writer, const Color& color) {
			writer.BeginArray();
			writer.Write(color.r);
			writer.Write(color.g);
			writer.Write(color.b);
			writer.Write(color.a);
			writer.EndArray();
		}

		Color ReadColor(const JsonReader& reader, const Color& fallback) {
			if (!reader.Valid() || reader.Size() < 4) {
				return fallback;
			}
			return {
				.r = reader[0].GetFloat(),
				.g = reader[1].GetFloat(),
				.b = reader[2].GetFloat(),
				.a = reader[3].GetFloat(),
			};
		}
	}

	void UiPanelStyleComponent::SetBackgroundColor(const Color& color) {
		mBackgroundColor = color;
	}

	const Color& UiPanelStyleComponent::GetBackgroundColor() const {
		return mBackgroundColor;
	}

	void UiPanelStyleComponent::SetCornerRadius(const float radius) {
		mCornerRadius = radius;
	}

	float UiPanelStyleComponent::GetCornerRadius() const {
		return mCornerRadius;
	}

	void UiPanelStyleComponent::SetBorderThickness(const float thickness) {
		mBorderThickness = thickness;
	}

	float UiPanelStyleComponent::GetBorderThickness() const {
		return mBorderThickness;
	}

	void UiPanelStyleComponent::SetBorderColor(const Color& color) {
		mBorderColor = color;
	}

	const Color& UiPanelStyleComponent::GetBorderColor() const {
		return mBorderColor;
	}

	void UiPanelStyleComponent::BuildDrawCommands(
		const UiWidget&             owner,
		std::vector<UiDrawCommand>& out
	) const {
		if (!owner.IsVisible()) {
			return;
		}

		UiDrawCommand command        = {};
		command.type                 = UI_DRAW_COMMAND_TYPE::RECT;
		command.rect.rect            = owner.GetGlobalRect();
		command.rect.fillColor       = mBackgroundColor;
		command.rect.cornerRadius    = mCornerRadius;
		command.rect.borderThickness = mBorderThickness;
		command.rect.borderColor     = mBorderColor;
		out.emplace_back(command);
	}

	void UiPanelStyleComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fillColor");
		WriteColor(writer, mBackgroundColor);
		writer.Key("cornerRadius");
		writer.Write(mCornerRadius);
		writer.Key("borderThickness");
		writer.Write(mBorderThickness);
		writer.Key("borderColor");
		WriteColor(writer, mBorderColor);
	}

	bool UiPanelStyleComponent::Deserialize(
		const JsonReader& reader, const UiDeserializeContext& context
	) {
		(void)context;
		if (reader.Has("fillColor")) {
			mBackgroundColor = ReadColor(reader["fillColor"], mBackgroundColor);
		}
		if (reader.Has("cornerRadius")) {
			mCornerRadius = reader["cornerRadius"].GetFloat();
		}
		if (reader.Has("borderThickness")) {
			mBorderThickness = reader["borderThickness"].GetFloat();
		}
		if (reader.Has("borderColor")) {
			mBorderColor = ReadColor(reader["borderColor"], mBorderColor);
		}
		return true;
	}
}
