#pragma once

#include <cstdint>

#include "UnnamedUITypes.h"

namespace Unnamed::UI {
	/// @brief UI描画の最小テーマ定義です。
	struct UITheme {
		//---------------------------------------------------------------------
		// グローバル↓
		//---------------------------------------------------------------------
		float defaultGap = 4.0f; // レイアウトのデフォルトギャップ

		//---------------------------------------------------------------------
		// ボタン↓
		//---------------------------------------------------------------------
		// ボタンの通常色
		UIColor buttonNormalColor = {
			.r = 0.6f, .g = 0.6f, .b = 0.6f, .a = 1.0f
		};
		// ボタンのホバー色
		UIColor buttonHoveredColor = {
			.r = 0.5f, .g = 0.5f, .b = 0.5f, .a = 1.0f
		};
		// ボタンの押下色
		UIColor buttonPressedColor = {
			.r = 0.89f, .g = 0.49f, .b = 0.02f, .a = 1.0f
		};
		// ボタンの枠線色
		UIColor buttonBorderColor = {
			.r = 0.125f, .g = 0.125f, .b = 0.125f, .a = 1.0f
		};
		// ボタンテキストのパディング TODO: いるんけ?
		Vec2 buttonTextPadding = Vec2(12.0f, 12.0f);

		// ボタンの高さ
		float buttonHeight = 18.0f; // デフォルトのボタン高さ

		// ボタンの枠線幅
		float buttonBorderWidth = 0.5f; // ボタンの枠線幅

		UI_TEXT_ALIGN buttonTextAlign = UI_TEXT_ALIGN::CENTER;

		float buttonHoverAnimationSpeed = 18.0f; // ホバーアニメーションの速度
		float buttonPressAnimationSpeed = 28.0f; // 押下アニメーションの速度

		float buttonPressedOffsetY = 1.5f; // 押下時のY方向のオフセット量

		//---------------------------------------------------------------------
		// チェックボックス↓
		//---------------------------------------------------------------------
		UIColor checkboxBoxColor = {
			.r = 0.6f, .g = 0.6f, .b = 0.6f, .a = 1.0f
		};
		UIColor checkboxBoxHoveredColor = {
			.r = 0.5f, .g = 0.5f, .b = 0.5f, .a = 1.0f
		};
		UIColor checkboxBoxPressedColor = {
			.r = 0.89f, .g = 0.49f, .b = 0.02f, .a = 1.0f
		};
		UIColor checkboxBorderColor = {
			.r = 0.125f, .g = 0.125f, .b = 0.125f, .a = 1.0f
		};
		UIColor checkboxCheckColor = {
			.r = 0.25f, .g = 0.25f, .b = 0.25f, .a = 1.0f
		};

		float checkboxSize         = 18.0f; // チェックボックスの正方形のサイズ
		float checkboxLabelGap     = 8.0f;  // チェックボックスとテキストの間の距離
		float checkboxHeight       = 18.0f; // チェックボックスの行の高さ（チェックボックスとテキストを含む）
		float checkboxCheckPadding = 5.0f;  // チェックマークとチェックボックスの内側の距離
		float checkboxBorderWidth  = 0.5f;  // チェックボックスの枠線幅

		//---------------------------------------------------------------------
		// スライダー↓
		//---------------------------------------------------------------------
		UIColor sliderTrackColor = {
			.r = 0.25f, .g = 0.25f, .b = 0.25f, .a = 1.0f
		};
		UIColor sliderFillColor = {
			.r = 0.6f, .g = 0.6f, .b = 0.6f, .a = 1.0f
		};
		UIColor sliderHoveredFillColor = {
			.r = 0.5f, .g = 0.5f, .b = 0.5f, .a = 1.0f
		};
		UIColor sliderPressedFillColor = {
			.r = 0.89f, .g = 0.49f, .b = 0.02f, .a = 1.0f
		};
		UIColor sliderBorderColor = {
			.r = 0.25f, .g = 0.25f, .b = 0.25f, .a = 1.0f
		};
		UIColor sliderTextColor = {
			.r = 0.25f, .g = 0.25f, .b = 0.25f, .a = 1.0f
		};

		float sliderHeight      = 18.0f;  // スライダーの高さ
		float sliderTrackHeight = 18.0f;  // スライダートラックの高さ
		float sliderWidth       = 240.0f; // スライダーの幅
		float sliderLabelWidth  = 96.0f;  // スライダーのラベルの幅
		float sliderValueWidth  = 48.0f;  // スライダーの値表示の幅
		float sliderGap         = 8.0f;   // スライダーの要素間の距離
		float sliderBorderWidth = 0.5f;   // スライダーの枠線幅

		//---------------------------------------------------------------------
		// パネル↓
		//---------------------------------------------------------------------
		// パネルの背景色
		UIColor panelColor = {
			.r = 0.69f, .g = 0.69f, .b = 0.69f, .a = 1.0f
		};
		// パネルの枠線色
		UIColor panelBorderColor = {
			.r = 0.125f, .g = 0.125f, .b = 0.125f, .a = 1.0f
		};

		// パネル内のコンテンツとパネル端の距離
		float panelPadding = 8.0f;

		// パネルの枠線幅
		float panelBorderWidth = 0.5f; // パネルの枠線幅

		//---------------------------------------------------------------------
		// セパレーター↓
		//---------------------------------------------------------------------
		UIColor separatorColor = {
			.r = 0.125f, .g = 0.125f, .b = 0.125f, .a = 1.0f
		};

		float separatorThickness = 1.0f;
		float separatorLength    = 240.0f;

		//---------------------------------------------------------------------
		// テキスト↓
		//---------------------------------------------------------------------
		// テキストの色
		UIColor textColor = {
			.r = 0.25f, .g = 0.25f, .b = 0.25f, .a = 1.0f
		};

		// フォントサイズ
		float fontSize = 16.0f;

		// フォントのオーバーサンプリング量（水平/垂直）
		uint32_t fontOversampleH = 4;
		uint32_t fontOversampleV = 4;
	};
}
