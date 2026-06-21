#include "UiDigitStripComponent.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/filesystem/Path.h"
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

	void UiDigitStripComponent::SetStripTexturePath(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mStripTexturePath == path) {
			return;
		}
		mStripTexturePath = std::move(path);
	}

	const Path& UiDigitStripComponent::GetStripTexturePath() const {
		return mStripTexturePath;
	}

	void UiDigitStripComponent::SetValue(const int value) {
		mValue = value;
	}

	int UiDigitStripComponent::GetValue() const {
		return mValue;
	}

	void UiDigitStripComponent::SetMinDigits(const int minDigits) {
		mMinDigits = std::max(1, minDigits);
	}

	int UiDigitStripComponent::GetMinDigits() const {
		return mMinDigits;
	}

	void UiDigitStripComponent::SetDigitSpacing(const float spacingPx) {
		mDigitSpacing = std::max(0.0f, spacingPx);
	}

	float UiDigitStripComponent::GetDigitSpacing() const {
		return mDigitSpacing;
	}

	void UiDigitStripComponent::SetColor(const Color& color) {
		mColor = color;
	}

	const Color& UiDigitStripComponent::GetColor() const {
		return mColor;
	}

	void UiDigitStripComponent::BuildDrawCommands(
		const UiWidget& owner,
		std::vector<UiDrawCommand>& out
	) const {
		if (!owner.IsVisible() || mStripTexturePath.IsEmpty()) {
			return;
		}

		const Rect rect = owner.GetGlobalRect();
		if (rect.width <= 0.0f || rect.height <= 0.0f) {
			return;
		}

		const int rawValue = std::max(0, mValue);
		std::string digits = std::to_string(rawValue);
		if (static_cast<int>(digits.size()) < mMinDigits) {
			digits.insert(
				digits.begin(),
				static_cast<size_t>(mMinDigits - static_cast<int>(digits.size())),
				'0'
			);
		}

		const int digitCount = static_cast<int>(digits.size());
		if (digitCount <= 0) {
			return;
		}

		const float totalSpacing = mDigitSpacing * static_cast<float>(digitCount - 1);
		const float digitWidth = (rect.width - totalSpacing) / static_cast<float>(digitCount);
		if (digitWidth <= 0.0f) {
			return;
		}

		for (int i = 0; i < digitCount; ++i) {
			const char c = digits[static_cast<size_t>(i)];
			if (c < '0' || c > '9') {
				continue;
			}

			const int digit = static_cast<int>(c - '0');
			const float u0 = static_cast<float>(digit) / 10.0f;
			const float u1 = static_cast<float>(digit + 1) / 10.0f;
			const float x = rect.x + (digitWidth + mDigitSpacing) * static_cast<float>(i);

			UiDrawCommand command = {};
			command.type = UI_DRAW_COMMAND_TYPE::IMAGE;
			command.image.rect = Rect(x, rect.y, digitWidth, rect.height);
			command.image.texturePath = mStripTexturePath;
			command.image.color = mColor;
			command.image.uvMin = Vec2(u0, 0.0f);
			command.image.uvMax = Vec2(u1, 1.0f);
			out.emplace_back(std::move(command));
		}
	}

	void UiDigitStripComponent::Serialize(JsonWriter& writer) const {
		writer.Key("stripTexturePath");
		writer.Write(mStripTexturePath.ToGenericUtf8());
		writer.Key("value");
		writer.Write(mValue);
		writer.Key("minDigits");
		writer.Write(mMinDigits);
		writer.Key("digitSpacing");
		writer.Write(mDigitSpacing);
		writer.Key("color");
		WriteColor(writer, mColor);
	}

	void UiDigitStripComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("stripTexturePath")) {
			SetStripTexturePath(
				Path(reader["stripTexturePath"].GetString())
			);
		}
		if (reader.Has("value")) {
			SetValue(reader["value"].GetInt());
		}
		if (reader.Has("minDigits")) {
			SetMinDigits(reader["minDigits"].GetInt());
		}
		if (reader.Has("digitSpacing")) {
			SetDigitSpacing(reader["digitSpacing"].GetFloat());
		}
		if (reader.Has("color")) {
			mColor = ReadColor(reader["color"], mColor);
		}
	}
}
