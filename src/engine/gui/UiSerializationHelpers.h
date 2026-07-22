#pragma once
#include "Rect.h"
#include "UiDrawCommand.h"
#include "UiWidget.h"
#include "components/UiLayoutComponents.h"

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

namespace Unnamed::Gui {
	/// @brief UIカラーをRGBA順のJSON配列として書き込みます。
	/// @param writer 書き込み先
	/// @param color 書き込むUIカラー
	inline void WriteColor(JsonWriter& writer, const Color& color) {
		writer.BeginArray();
		writer.Write(color.r);
		writer.Write(color.g);
		writer.Write(color.b);
		writer.Write(color.a);
		writer.EndArray();
	}

	/// @brief RGBA順のJSON配列からUIカラーを読み取ります。
	/// @param node 読み取り元
	/// @param fallback 無効または4要素未満の場合の値
	/// @return 読み取ったUIカラー、またはfallback
	inline Color ReadColor(const JsonReader& node, const Color& fallback) {
		if (!node.Valid() || node.Size() < 4) {
			return fallback;
		}
		return {
			.r = node[0].GetFloat(),
			.g = node[1].GetFloat(),
			.b = node[2].GetFloat(),
			.a = node[3].GetFloat(),
		};
	}

	/// @brief Vec2をXY順のJSON配列として書き込みます。
	/// @param writer 書き込み先
	/// @param value 書き込む値
	inline void WriteVec2(JsonWriter& writer, const Vec2& value) {
		writer.BeginArray();
		writer.Write(value.x);
		writer.Write(value.y);
		writer.EndArray();
	}

	/// @brief XY順のJSON配列からVec2を読み取ります。
	/// @param node 読み取り元
	/// @param fallback 無効または2要素未満の場合の値
	/// @return 読み取ったVec2、またはfallback
	inline Vec2 ReadVec2(const JsonReader& node, const Vec2& fallback) {
		if (!node.Valid() || node.Size() < 2) {
			return fallback;
		}
		return Vec2(node[0].GetFloat(), node[1].GetFloat());
	}

	inline void WriteRect(JsonWriter& writer, const Rect& r) {
		writer.BeginArray();
		writer.Write(r.x);
		writer.Write(r.y);
		writer.Write(r.width);
		writer.Write(r.height);
		writer.EndArray();
	}

	inline Rect ReadRect(const JsonReader& node) {
		Rect r{};
		if (!node.Valid() || node.Size() < 4) {
			return r;
		}
		r.x      = node[0].GetFloat();
		r.y      = node[1].GetFloat();
		r.width  = node[2].GetFloat();
		r.height = node[3].GetFloat();
		return r;
	}

	inline void WriteAnchors(JsonWriter& writer, const Anchors& a) {
		writer.BeginArray();
		writer.Write(a.minX);
		writer.Write(a.minY);
		writer.Write(a.maxX);
		writer.Write(a.maxY);
		writer.EndArray();
	}

	inline Anchors ReadAnchors(const JsonReader& node) {
		Anchors a{0, 0, 0, 0};
		if (!node.Valid() || node.Size() < 4) {
			return a;
		}
		a.minX = node[0].GetFloat();
		a.minY = node[1].GetFloat();
		a.maxX = node[2].GetFloat();
		a.maxY = node[3].GetFloat();
		return a;
	}

	inline void WritePadding(JsonWriter& writer, const LayoutPadding& p) {
		writer.BeginArray();
		writer.Write(p.left);
		writer.Write(p.top);
		writer.Write(p.right);
		writer.Write(p.bottom);
		writer.EndArray();
	}

	inline LayoutPadding ReadPadding(const JsonReader& node) {
		LayoutPadding p{};
		if (!node.Valid() || node.Size() < 4) {
			return p;
		}
		p.left   = node[0].GetFloat();
		p.top    = node[1].GetFloat();
		p.right  = node[2].GetFloat();
		p.bottom = node[3].GetFloat();
		return p;
	}

	inline void WritePivot(JsonWriter& writer, const Pivot& p) {
		writer.BeginArray();
		writer.Write(p.x);
		writer.Write(p.y);
		writer.EndArray();
	}

	inline Pivot ReadPivot(const JsonReader& node) {
		Pivot p{0, 0};
		if (!node.Valid() || node.Size() < 2) {
			return p;
		}
		p.x = node[0].GetFloat();
		p.y = node[1].GetFloat();
		return p;
	}

	inline void WriteMargins(JsonWriter& writer, const Margins& m) {
		writer.BeginArray();
		writer.Write(m.left);
		writer.Write(m.top);
		writer.Write(m.right);
		writer.Write(m.bottom);
		writer.EndArray();
	}

	inline Margins ReadMargins(const JsonReader& node) {
		Margins m{};
		if (!node.Valid() || node.Size() < 4) {
			return m;
		}
		m.left   = node[0].GetFloat();
		m.top    = node[1].GetFloat();
		m.right  = node[2].GetFloat();
		m.bottom = node[3].GetFloat();
		return m;
	}

	inline void WriteSizePolicy(
		JsonWriter&         writer,
		const UiSizePolicy& policy
	) {
		writer.BeginArray();
		writer.Write(
			policy.horizontal == UiSizePolicyAxis::FIXED ?
				"Fixed" :
				"Expand"
		);
		writer.Write(
			policy.vertical == UiSizePolicyAxis::FIXED ?
				"Fixed" :
				"Expand"
		);
		writer.EndArray();
	}

	inline UiSizePolicy ReadSizePolicy(const JsonReader& node) {
		UiSizePolicy policy{};
		if (!node.Valid() || node.Size() < 2) {
			return policy;
		}
		const auto h      = node[0].GetString();
		const auto v      = node[1].GetString();
		policy.horizontal = h == "Expand" ?
			                    UiSizePolicyAxis::EXPAND :
			                    UiSizePolicyAxis::FIXED;
		policy.vertical = v == "Expand" ?
			                  UiSizePolicyAxis::EXPAND :
			                  UiSizePolicyAxis::FIXED;
		return policy;
	}

	inline void WriteConstraints(
		JsonWriter& writer, const UiSizeConstraints& c
	) {
		writer.BeginObject();
		writer.Key("minWidth");
		writer.Write(c.minWidth);
		writer.Key("minHeight");
		writer.Write(c.minHeight);
		writer.Key("maxWidth");
		writer.Write(c.maxWidth);
		writer.Key("maxHeight");
		writer.Write(c.maxHeight);
		writer.EndObject();
	}

	inline UiSizeConstraints ReadConstraints(const JsonReader& node) {
		UiSizeConstraints c{};
		if (!node.Valid()) {
			return c;
		}

		if (node.Has("minWidth")) {
			c.minWidth = node["minWidth"].GetFloat();
		}
		if (node.Has("minHeight")) {
			c.minHeight = node["minHeight"].GetFloat();
		}
		if (node.Has("maxWidth")) {
			c.maxWidth = node["maxWidth"].GetFloat();
		}
		if (node.Has("maxHeight")) {
			c.maxHeight = node["maxHeight"].GetFloat();
		}
		return c;
	}
}
