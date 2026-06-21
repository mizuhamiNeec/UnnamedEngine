#pragma once
#include <cstdint>
#include <string>

namespace Unnamed {
	enum class ASSET_TYPE : uint16_t {
		UNKNOWN            = 1 << 0,  // 不明
		TEXTURE            = 1 << 1,  // テクスチャ
		SHADER_SOURCE      = 1 << 2,  // シェーダー
		MATERIAL           = 1 << 3,  // マテリアル
		MESH               = 1 << 4,  // メッシュ
		SOUND              = 1 << 5,  // サウンド
		RAW_FILE           = 1 << 6,  // 生ファイル
		SHADER_PROGRAM     = 1 << 7,  // シェーダープログラム
		MATERIAL_INSTANCE  = 1 << 8,  // マテリアルインスタンス
		POST_FX_CHAIN      = 1 << 9,  // ポストFXチェーン
		UI_DOCUMENT        = 1 << 10, // UIドキュメント
		EVENT_PRESENTATION = 1 << 11, // EventPresentation v2 プロファイル
		SEQUENCE           = 1 << 12, // Sequence / Timeline
		EDITOR_GUI         = 1 << 13, // Editor GUI
	};

	std::string ToString(ASSET_TYPE e);
	ASSET_TYPE  GuessAssetTypeFromPath(const Path& path);
}
