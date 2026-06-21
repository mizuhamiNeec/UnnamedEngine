#include "UiTextureComponent.h"

#include <algorithm>

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

		void WriteVec2(JsonWriter& writer, const Vec2& value) {
			writer.BeginArray();
			writer.Write(value.x);
			writer.Write(value.y);
			writer.EndArray();
		}

		Vec2 ReadVec2(const JsonReader& reader, const Vec2& fallback) {
			if (!reader.Valid() || reader.Size() < 2) {
				return fallback;
			}
			return Vec2(reader[0].GetFloat(), reader[1].GetFloat());
		}
	}

	void UiTextureComponent::SetTexturePath(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mTexturePath == path) {
			return;
		}
		mTexturePath = std::move(path);
	}

	const Path& UiTextureComponent::GetTexturePath() const {
		return mTexturePath;
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
		if (!owner.IsVisible() || mTexturePath.IsEmpty()) {
			return;
		}

		const Rect rect = owner.GetGlobalRect();
		if (rect.width <= 0.0f || rect.height <= 0.0f) {
			return;
		}

		UiDrawCommand command     = {};
		command.type              = UI_DRAW_COMMAND_TYPE::IMAGE;
		command.image.rect        = rect;
		command.image.texturePath = mTexturePath;
		command.image.color       = mColor;
		command.image.uvMin       = mUvMin;
		command.image.uvMax       = mUvMax;
		command.image.anchor      = mAnchor;
		command.image.rotationRad = mRotationRad;
		out.emplace_back(std::move(command));
	}

	void UiTextureComponent::Serialize(JsonWriter& writer) const {
		writer.Key("texturePath");
		writer.Write(mTexturePath.ToGenericUtf8());
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

	void UiTextureComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("texturePath")) {
			SetTexturePath(Path(reader["texturePath"].GetString()));
		}
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
	}
}
