#include "UiDigitStripComponent.h"

#include <algorithm>
#include <string>

#include "core/assets/AssetManager.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/gui/UiTextureReference.h"
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

	bool UiDigitStripComponent::SetStripTexturePath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		if (
			mStripTexturePath.has_value() && *mStripTexturePath == path &&
			mStripTextureAssetId != kInvalidAssetID
		) {
			return true;
		}
		const AssetID assetId = assetManager.LoadTexture(path);
		if (assetId == kInvalidAssetID) {
			ClearStripTexturePath();
			return false;
		}
		mStripTexturePath    = path;
		mStripTextureAssetId = assetId;
		return true;
	}

	bool UiDigitStripComponent::SetStripTexturePath(
		const std::string_view path, AssetManager& assetManager
	) {
		const auto virtualPath = VirtualPath::ParseContentReference(path);
		if (!virtualPath.has_value()) {
			Error("UI", "Invalid UI texture virtual path: {}", path);
			ClearStripTexturePath();
			return false;
		}
		return SetStripTexturePath(*virtualPath, assetManager);
	}

	void UiDigitStripComponent::ClearStripTexturePath() noexcept {
		mStripTexturePath.reset();
		mStripTextureAssetId = kInvalidAssetID;
	}

	const std::optional<VirtualPath>&
	UiDigitStripComponent::GetStripTexturePath() const noexcept {
		return mStripTexturePath;
	}

	AssetID UiDigitStripComponent::GetStripTextureAssetId() const noexcept {
		return mStripTextureAssetId;
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
		const UiWidget&             owner,
		std::vector<UiDrawCommand>& out
	) const {
		if (!owner.IsVisible() || mStripTextureAssetId == kInvalidAssetID) {
			return;
		}

		const Rect rect = owner.GetGlobalRect();
		if (rect.width <= 0.0f || rect.height <= 0.0f) {
			return;
		}

		const int   rawValue = std::max(0, mValue);
		std::string digits   = std::to_string(rawValue);
		if (static_cast<int>(digits.size()) < mMinDigits) {
			digits.insert(
				digits.begin(),
				static_cast<size_t>(
					mMinDigits - static_cast<int>(digits.size())),
				'0'
			);
		}

		const int digitCount = static_cast<int>(digits.size());
		if (digitCount <= 0) {
			return;
		}

		const float totalSpacing =
			mDigitSpacing * static_cast<float>(digitCount - 1);
		const float digitWidth =
			(rect.width - totalSpacing) / static_cast<float>(digitCount);
		if (digitWidth <= 0.0f) {
			return;
		}

		for (int i = 0; i < digitCount; ++i) {
			const char c = digits[static_cast<size_t>(i)];
			if (c < '0' || c > '9') {
				continue;
			}

			const int   digit = c - '0';
			const float u0 = static_cast<float>(digit) / 10.0f;
			const float u1 = static_cast<float>(digit + 1) / 10.0f;
			const float x = rect.x + (digitWidth + mDigitSpacing) * static_cast<
				                float>(i);

			UiDrawCommand command = {};
			command.type = UI_DRAW_COMMAND_TYPE::IMAGE;
			command.image.rect = Rect(x, rect.y, digitWidth, rect.height);
			command.image.textureAssetId = mStripTextureAssetId;
			command.image.color = mColor;
			command.image.uvMin = Vec2(u0, 0.0f);
			command.image.uvMax = Vec2(u1, 1.0f);
			out.emplace_back(std::move(command));
		}
	}

	void UiDigitStripComponent::Serialize(JsonWriter& writer) const {
		if (mStripTexturePath.has_value()) {
			writer.Key("stripTexturePath");
			writer.Write(mStripTexturePath->String());
		}
		writer.Key("value");
		writer.Write(mValue);
		writer.Key("minDigits");
		writer.Write(mMinDigits);
		writer.Key("digitSpacing");
		writer.Write(mDigitSpacing);
		writer.Key("color");
		WriteColor(writer, mColor);
	}

	bool UiDigitStripComponent::Deserialize(
		const JsonReader& reader, const UiDeserializeContext& context
	) {
		UiTextureReference reference;
		if (!DeserializeUiTextureReference(
				reader, "stripTexturePath", context, reference)) {
			ClearStripTexturePath();
			return false;
		}
		mStripTexturePath    = std::move(reference.virtualPath);
		mStripTextureAssetId = reference.assetId;
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
		return true;
	}
}
