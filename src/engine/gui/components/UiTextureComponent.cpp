#include "UiTextureComponent.h"

#include <algorithm>

#include "core/assets/AssetManager.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/gui/UiSerializationHelpers.h"
#include "engine/gui/UiDeserializeContext.h"
#include "engine/gui/UiTextureReference.h"
#include "engine/gui/UiWidget.h"

namespace Unnamed::Gui {
	bool UiTextureComponent::SetTexturePath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		if (
			mTexturePath.has_value() && *mTexturePath == path &&
			mTextureAssetId != kInvalidAssetID
		) {
			return true;
		}
		const AssetID assetId = assetManager.LoadTexture(path);
		if (assetId == kInvalidAssetID) {
			ClearTexturePath();
			return false;
		}
		mTexturePath    = path;
		mTextureAssetId = assetId;
		return true;
	}

	bool UiTextureComponent::SetTexturePath(
		const std::string_view path, AssetManager& assetManager
	) {
		const auto virtualPath = VirtualPath::ParseContentReference(path);
		if (!virtualPath.has_value()) {
			Error("UI", "Invalid UI texture virtual path: {}", path);
			ClearTexturePath();
			return false;
		}
		return SetTexturePath(*virtualPath, assetManager);
	}

	void UiTextureComponent::ClearTexturePath() noexcept {
		mTexturePath.reset();
		mTextureAssetId = kInvalidAssetID;
	}

	const std::optional<VirtualPath>& UiTextureComponent::GetTexturePath()
	const noexcept {
		return mTexturePath;
	}

	AssetID UiTextureComponent::GetTextureAssetId() const noexcept {
		return mTextureAssetId;
	}

	void UiTextureComponent::SetColor(const Color& color) {
		mColor = color;
	}

	const Color& UiTextureComponent::GetColor() const {
		return mColor;
	}

	void UiTextureComponent::SetUvMin(const Vec2& uvMin) {
		mUvMin.x = std::clamp(uvMin.x, 0.0f, 1.0f);
		mUvMin.y = std::clamp(uvMin.y, 0.0f, 1.0f);
	}

	const Vec2& UiTextureComponent::GetUvMin() const {
		return mUvMin;
	}

	void UiTextureComponent::SetUvMax(const Vec2& uvMax) {
		mUvMax.x = std::clamp(uvMax.x, 0.0f, 1.0f);
		mUvMax.y = std::clamp(uvMax.y, 0.0f, 1.0f);
	}

	const Vec2& UiTextureComponent::GetUvMax() const {
		return mUvMax;
	}

	void UiTextureComponent::SetAnchor(const Vec2& anchor) {
		mAnchor.x = std::clamp(anchor.x, 0.0f, 1.0f);
		mAnchor.y = std::clamp(anchor.y, 0.0f, 1.0f);
	}

	const Vec2& UiTextureComponent::GetAnchor() const {
		return mAnchor;
	}

	void UiTextureComponent::SetRotationRad(const float rotationRad) {
		mRotationRad = rotationRad;
	}

	float UiTextureComponent::GetRotationRad() const {
		return mRotationRad;
	}

	void UiTextureComponent::BuildDrawCommands(
		const UiWidget&             owner,
		std::vector<UiDrawCommand>& out
	) const {
		if (!owner.IsVisible() || mTextureAssetId == kInvalidAssetID) {
			return;
		}

		const Rect rect = owner.GetGlobalRect();
		if (rect.width <= 0.0f || rect.height <= 0.0f) {
			return;
		}

		UiDrawCommand command     = {};
		command.type              = UI_DRAW_COMMAND_TYPE::IMAGE;
		command.image.rect        = rect;
		command.image.textureAssetId = mTextureAssetId;
		command.image.color       = mColor;
		command.image.uvMin       = mUvMin;
		command.image.uvMax       = mUvMax;
		command.image.anchor      = mAnchor;
		command.image.rotationRad = mRotationRad;
		out.emplace_back(std::move(command));
	}

	void UiTextureComponent::Serialize(JsonWriter& writer) const {
		if (mTexturePath.has_value()) {
			writer.Key("texturePath");
			writer.Write(mTexturePath->String());
		}
		writer.Key("color");
		WriteColor(writer, mColor);
		writer.Key("uvMin");
		WriteVec2(writer, mUvMin);
		writer.Key("uvMax");
		WriteVec2(writer, mUvMax);
		writer.Key("anchor");
		WriteVec2(writer, mAnchor);
		writer.Key("rotationRad");
		writer.Write(mRotationRad);
	}

	bool UiTextureComponent::Deserialize(
		const JsonReader& reader, const UiDeserializeContext& context
	) {
		UiTextureReference reference;
		if (!DeserializeUiTextureReference(
				reader, "texturePath", context, reference)) {
			ClearTexturePath();
			return false;
		}
		mTexturePath    = std::move(reference.virtualPath);
		mTextureAssetId = reference.assetId;
		if (reader.Has("color")) {
			mColor = ReadColor(reader["color"], mColor);
		}
		if (reader.Has("uvMin")) {
			SetUvMin(ReadVec2(reader["uvMin"], mUvMin));
		}
		if (reader.Has("uvMax")) {
			SetUvMax(ReadVec2(reader["uvMax"], mUvMax));
		}
		if (reader.Has("anchor")) {
			SetAnchor(ReadVec2(reader["anchor"], mAnchor));
		}
		if (reader.Has("rotationRad")) {
			SetRotationRad(reader["rotationRad"].GetFloat(mRotationRad));
		}
		return true;
	}
}
