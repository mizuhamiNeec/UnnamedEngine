#pragma once

#ifdef _DEBUG

#include <cstdint>
#include <functional>
#include <string_view>

#include <imgui.h>

namespace Ui {
	//-------------------------------------------------------------------------
	// デザイン調整のために永遠にStyleVarやColor、レイアウトをいじっていて、
	// 頭がフットーしそうだったのでRAIIで書けるようにしたImGuiラッパー
	//-------------------------------------------------------------------------

	/// @brief UI上で使用するフォントスタイルの識別子です。
	enum class UiFontStyle : uint8_t {
		Body = 0,
		Heading,
		Caption,
	};

	/// @brief UI上で使用する色ロールの識別子です。
	enum class UiColorRole : uint8_t {
		Text = 0,
		TextMuted,
		Accent,
		SectionHeaderBackground,
		SectionHeaderText,
	};

	/// @brief 角丸矩形のコーナーモードを指定します。
	enum class UiCornerMode : uint8_t {
		None = 0,
		TopOnly,
		BottomOnly,
		All,
	};

	/// @brief UIの見た目に関する最小スタイルセットです。
	struct UiStyleSet {
		ImVec4 text                    = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
		ImVec4 textMuted               = ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
		ImVec4 accent                  = ImVec4(0.89f, 0.49f, 0.02f, 1.00f);
		ImVec4 sectionHeaderBackground = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		ImVec4 sectionHeaderText       = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		ImVec2 sectionHeaderPadding    = ImVec2(10.0f, 6.0f);
		float  sectionHeaderMinHeight  = 30.0f;
		float  sectionHeaderRounding   = 6.0f;
		float  sectionContentSpacingY  = 6.0f;
		float  defaultSectionRounding  = 4.0f;
		float  headingFontScale        = 1.20f;
		float  captionFontScale        = 0.90f;
	};

	/// @brief UIラッパーのコンテキストです。色とフォント解決を差し替えできます。
	struct UiContext {
		UiStyleSet                          styleSet;
		std::function<ImFont*(UiFontStyle)> fontResolver;
		std::function<ImU32(UiColorRole)>   colorResolver;
	};

	/// @brief DrawListベースのテキスト描画パラメータです。
	struct UiTextDrawParams {
		ImVec2           position              = ImVec2(0.0f, 0.0f);
		std::string_view text                  = {};
		ImDrawList*      drawList              = nullptr;
		ImFont*          font                  = nullptr;
		UiFontStyle      fontStyle             = UiFontStyle::Body;
		UiColorRole      colorRole             = UiColorRole::Text;
		ImU32            color                 = 0;
		float            fontSize              = 0.0f;
		bool             useExplicitColor      = false;
		bool             positionIsScreenSpace = true;
	};

	/// @brief セクションヘッダー描画のパラメータです。
	struct UiSectionHeaderParams {
		std::string_view text = {};
		UiFontStyle fontStyle = UiFontStyle::Heading;
		UiColorRole textColorRole = UiColorRole::SectionHeaderText;
		UiColorRole backgroundColorRole = UiColorRole::SectionHeaderBackground;
		ImFont* font = nullptr;
		float fontSize = 0.0f;
		float minHeight = 0.0f;
		float rounding = 0.0f;
		UiCornerMode cornerMode = UiCornerMode::TopOnly;
		ImVec2 padding = ImVec2(0.0f, 0.0f);
	};

	/// @brief ImGui::PushID/PopID を安全に扱うスコープです。
	class ScopedId {
	public:
		/// @brief 文字列IDでスコープを開始します。
		explicit ScopedId(const char* id);
		/// @brief 整数IDでスコープを開始します。
		explicit ScopedId(int32_t id);
		/// @brief 整数IDでスコープを開始します。
		explicit ScopedId(uint64_t id);
		~ScopedId();

		ScopedId(const ScopedId&)            = delete;
		ScopedId& operator=(const ScopedId&) = delete;
		ScopedId(ScopedId&& other) noexcept;
		ScopedId& operator=(ScopedId&& other) noexcept;

	private:
		bool mActive = false;
	};

	/// @brief ImGui::PushStyleVar/PopStyleVar を安全に扱うスコープです。
	class ScopedStyleVar {
	public:
		/// @brief float値のStyleVarをプッシュします。
		ScopedStyleVar(ImGuiStyleVar styleVar, float value);
		/// @brief ImVec2値のStyleVarをプッシュします。
		ScopedStyleVar(ImGuiStyleVar styleVar, const ImVec2& value);
		~ScopedStyleVar();

		ScopedStyleVar(const ScopedStyleVar&)            = delete;
		ScopedStyleVar& operator=(const ScopedStyleVar&) = delete;
		ScopedStyleVar(ScopedStyleVar&& other) noexcept;
		ScopedStyleVar& operator=(ScopedStyleVar&& other) noexcept;

	private:
		bool mActive = false;
	};

	/// @brief ImGui::PushStyleColor/PopStyleColor を安全に扱うスコープです。
	class ScopedStyleColor {
	public:
		/// @brief ImVec4色でStyleColorをプッシュします。
		ScopedStyleColor(ImGuiCol colorIndex, const ImVec4& color);
		/// @brief ImU32色でStyleColorをプッシュします。
		ScopedStyleColor(ImGuiCol colorIndex, ImU32 color);
		~ScopedStyleColor();

		ScopedStyleColor(const ScopedStyleColor&)            = delete;
		ScopedStyleColor& operator=(const ScopedStyleColor&) = delete;
		ScopedStyleColor(ScopedStyleColor&& other) noexcept;
		ScopedStyleColor& operator=(ScopedStyleColor&& other) noexcept;

	private:
		bool mActive = false;
	};

	/// @brief ImGui::PushFont/PopFont を安全に扱うスコープです。
	class ScopedFont {
	public:
		/// @brief フォントをプッシュします。nullptrは無視されます。
		explicit ScopedFont(ImFont* font);
		~ScopedFont();

		ScopedFont(const ScopedFont&)            = delete;
		ScopedFont& operator=(const ScopedFont&) = delete;
		ScopedFont(ScopedFont&& other) noexcept;
		ScopedFont& operator=(ScopedFont&& other) noexcept;

	private:
		bool mActive = false;
	};

	/// @brief ImGui::BeginDisabled/EndDisabled を安全に扱うスコープです。
	class ScopedDisabled {
	public:
		/// @brief disabled=true のときだけBeginDisabledします。
		explicit ScopedDisabled(bool disabled = true);
		~ScopedDisabled();

		ScopedDisabled(const ScopedDisabled&)            = delete;
		ScopedDisabled& operator=(const ScopedDisabled&) = delete;
		ScopedDisabled(ScopedDisabled&& other) noexcept;
		ScopedDisabled& operator=(ScopedDisabled&& other) noexcept;

	private:
		bool mActive = false;
	};

	/// @brief 現在のUIコンテキストを更新します。
	void SetContext(const UiContext& context);

	/// @brief 現在のUIコンテキストを取得します。
	[[nodiscard]] const UiContext& GetContext();

	/// @brief UIコンテキストをデフォルト値に戻します。
	void ResetContext();

	/// @brief 色ロールをImU32に解決します。
	[[nodiscard]] ImU32 ResolveColor(UiColorRole colorRole);

	/// @brief フォントスタイルをフォントポインタに解決します。
	[[nodiscard]] ImFont* ResolveFont(UiFontStyle fontStyle);

	/// @brief フォントスタイルに対応したフォントサイズを解決します。
	[[nodiscard]] float ResolveFontSize(
		UiFontStyle fontStyle, float requestedSize = 0.0f
	);

	/// @brief 1行横並びコンテキストを開始します。
	void Row(const std::function<void()>& drawContent, float spacing = -1.0f);

	/// @brief 指定幅を基準にコンテンツを中央寄せで描画します。
	void Centered(
		const std::function<void()>& drawContent, float contentWidth = 0.0f
	);

	/// @brief コンテナ高さに対して現在カーソルを上下中央位置へ調整します。
	/// @param containerHeight 中央揃えの基準高さ。0なら残り利用可能高さを使います。
	/// @param contentHeight 描画対象の高さ。0ならフレーム高さを使います。
	void AlignCenterY(float containerHeight = 0.0f, float contentHeight = 0.0f);

	/// @brief 指定高さ領域内でコンテンツを上下中央寄せで描画します。
	/// @param drawContent 描画処理
	/// @param containerHeight 中央揃え先のコンテナ高さ。0なら残り利用可能高さを使います。
	/// @param contentHeight 描画対象の高さ。0ならフレーム高さを使います。
	void CenteredY(
		const std::function<void()>& drawContent,
		float                        containerHeight = 0.0f,
		float                        contentHeight   = 0.0f
	);

	/// @brief セクション見出し+本文の簡易ブロックを描画します。
	void Section(
		const char* sectionLabel, const std::function<void()>& drawContent,
		bool        drawTrailingSeparator = true
	);

	/// @brief 垂直スペースを挿入します。
	void Spacing(float height = 0.0f);

	/// @brief 区切り線を描画します。
	void Separator();

	/// @brief 通常レイアウトに乗るテキストを描画します。
	void Text(std::string_view text);

	/// @brief 折り返し有効でテキストを描画します。
	void TextWrapped(std::string_view text);

	/// @brief DrawListベースでテキストを描画します。
	void DrawTextW(const UiTextDrawParams& params);

	/// @brief 標準ボタンを描画します。
	[[nodiscard]] bool Button(
		const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f)
	);

	/// @brief Drag系ウィジェット向けの背景色セットをプッシュします。
	void PushStyleColorForDrag(
		const ImVec4& bg,
		const ImVec4& bgHovered,
		const ImVec4& bgActive
	);

	/// @brief アイコン付きボタンを描画します。
	bool IconButton(
		uint32_t    icon,
		const char* label     = nullptr,
		ImVec2      size      = ImVec2(0, 0),
		float       iconScale = 1.0f,
		ImGuiDir    labelDir  = ImGuiDir_Down
	);

	/// @brief メインメニュー用のBeginMenuラッパーです。
	[[nodiscard]] bool BeginMainMenu(const char* label, bool enabled = true);

	/// @brief 角丸付きイメージを描画します。
	void ImageWithRounding(
		ImTextureID textureId,
		ImVec2      imageSize,
		float       rounding,
		ImDrawFlags flags     = ImDrawFlags_RoundCornersAll,
		ImVec2      uv0       = ImVec2(0, 0),
		ImVec2      uv1       = ImVec2(1, 1),
		ImVec4      tintColor = ImVec4(1, 1, 1, 1)
	);

	/// @brief セクションヘッダーを描画します。
	void SectionHeader(const char* text);

	/// @brief セクションヘッダーを詳細パラメータで描画します。
	void SectionHeader(const UiSectionHeaderParams& params);

	/// @brief 塗りつぶし矩形を描画します。
	void DrawFilledRect(
		ImDrawList*   drawList,
		const ImVec2& min,
		const ImVec2& max,
		ImU32         color,
		float         rounding   = 0.0f,
		UiCornerMode  cornerMode = UiCornerMode::All
	);

	/// @brief 上側のみ角丸の塗りつぶし矩形を描画します。
	void DrawFilledRectTopRounded(
		ImDrawList*   drawList,
		const ImVec2& min,
		const ImVec2& max,
		ImU32         color,
		float         rounding
	);
}

#endif
