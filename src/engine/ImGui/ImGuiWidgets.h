#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
#include <imgui.h>

#include "core/assets/AssetType.h"
#include "engine/editor/ContentBrowser.h"

struct Vec3;

namespace ImGuiWidgets {
	using AssetTypeMask = Unnamed::EditorContentBrowser::AssetTypeMask;
	inline constexpr AssetTypeMask kAssetTypeMaskAny =
		Unnamed::EditorContentBrowser::kAssetTypeMaskAny;
	inline constexpr const char* kAssetDragDropPayloadId =
		Unnamed::EditorContentBrowser::kAssetDragDropPayloadId;
	using AssetDragDropPayload = Unnamed::EditorContentBrowser::
	AssetDragDropPayload;

	[[nodiscard]] AssetTypeMask AssetTypeToMask(Unnamed::ASSET_TYPE type);
	[[nodiscard]] bool          IsAssetTypeAccepted(
		Unnamed::ASSET_TYPE type,
		AssetTypeMask       acceptedMask
	);

	/// @brief 固定容量の編集バッファを使ってstd::stringを入力します。
	/// @param label ウィジェットのラベル
	/// @param value 編集対象の文字列
	/// @param capacity 終端nullを含む編集バッファ容量
	/// @return ImGuiが値の変更を報告した場合はtrue
	inline bool InputText(
		const char* label, std::string& value, const size_t capacity
	) {
		if (capacity == 0) {
			return false;
		}
		std::vector<char> buffer(capacity, '\0');
		const size_t      copyLength = std::min(value.size(), capacity - 1);
		if (copyLength > 0) {
			std::memcpy(buffer.data(), value.data(), copyLength);
		}
		if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
			return false;
		}
		value = buffer.data();
		return true;
	}

	/// @brief スタック上の固定長バッファを使ってstd::stringを入力します。
	/// @tparam Capacity 終端nullを含む編集バッファ容量
	/// @param label ウィジェットのラベル
	/// @param value 編集対象の文字列
	/// @return ImGuiが値の変更を報告した場合はtrue
	template <size_t Capacity>
	bool InputText(const char* label, std::string& value) {
		static_assert(Capacity > 0);
		std::array<char, Capacity> buffer = {};
		const size_t copyLength = std::min(value.size(), buffer.size() - 1);
		if (copyLength > 0) {
			std::memcpy(buffer.data(), value.data(), copyLength);
		}
		if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
			return false;
		}
		value = buffer.data();
		return true;
	}

	/// @brief Dragウィジェット用のスタイルカラーをプッシュします。
	/// @param bg 通常時の背景色
	/// @param bgHovered ホバー時の背景色
	/// @param bgActive アクティブ時の背景色
	void PushStyleColorForDrag(
		const ImVec4& bg,
		const ImVec4& bgHovered,
		const ImVec4& bgActive
	);

	/// @brief Vec3型のドラッグウィジェットを表示します。
	/// @param name ウィジェットのラベル
	/// @param value 編集するVec3型の値への参照
	/// @param defaultValue リセット時のデフォルト値
	/// @param vSpeed ドラッグ操作の速度
	/// @param format 表示フォーマット
	/// @return 値が変更された場合にtrueを返します。
	bool DragVec3(
		const std::string& name,
		Vec3&              value,
		Vec3               defaultValue,
		float              vSpeed,
		const char*        format
	);

	/// @brief Cubic Bézier曲線の編集ウィジェットを表示します。
	/// @param label ウィジェットのラベル
	/// @param p0 コントロールポイント1のX座標への参照
	/// @param p1 コントロールポイント1のY座標への参照
	/// @param p2 コントロールポイント2のX座標への参照
	/// @param p3 コントロールポイント2のY座標への参照
	/// @return 値が変更された場合にtrueを返します。
	bool EditCubicBezier(
		const std::string& label,
		float              p0,
		float              p1,
		float              p2,
		float              p3
	);

	/// @brief アイコン付きボタンウィジェットを表示します。
	/// @param icon アイコン（フォントアイコン）
	/// @param label ラベル文字列（省略可能）
	/// @param size ボタンのサイズ（(0,0)で自動調整）
	/// @param iconScale アイコンのスケーリング
	/// @param labelDir ラベルの配置方向
	/// @return ボタンが押された場合にtrueを返します。
	bool IconButton(
		uint32_t    icon,
		const char* label     = nullptr,
		ImVec2      size      = ImVec2(0, 0),
		float       iconScale = 1.0f,
		ImGuiDir    labelDir  = ImGuiDir_Down
	);

	/// @brief アイコン付きメニューアイテムを表示します。
	/// @param icon アイコン文字列（フォントアイコン）
	/// @param label ラベル文字列
	/// @param shortcut ショートカット文字列（省略可能）
	/// @param selected 選択状態かどうか
	/// @param enabled メニューアイテムが有効かどうか
	/// @param rounding 角丸の半径
	/// @return メニューアイテムが選択された場合にtrueを返します
	bool MenuItemWithIcon(
		const char* label,
		uint32_t    icon,
		const char* shortcut = nullptr,
		bool        selected = false,
		bool        enabled  = true,
		float       rounding = 4.0f
	);

	/// @brief メインメニューバーを開始します。
	/// @param label メニューバーのラベル
	/// @param height メニューバーの高さ
	/// @param enabled メニューバーが有効かどうか
	/// @return メニューバーが開始された場合にtrueを返します
	bool BeginMainMenu(const char* label, float height, bool enabled = true);

	/// @brief エンジン用のメニューを開始します。
	void BeginMenu();

	/// @brief エンジン用のメニューを終了します。
	void EndMenu();

	/// @brief ホバー中のコンボメニューに対するマウスホイールスクロールを処理します。
	/// @param index 現在の選択インデックスへの参照
	/// @param itemSize コンボメニューの項目数
	void HandleHoveredComboMenuMouseWheelScroll(
		uint32_t& index, uint32_t itemSize
	);

	/// @brief ImGui1.92.6から取ってきたSelectableウィジェット。角丸四角形で表示します。
	/// @param label ウィジェットのラベル（非表示にする場合は"##unique_id"のようにユニークなIDを指定）
	/// @param selected 選択状態かどうか
	/// @param flags ImGuiSelectableFlagsのフラグ
	/// @param sizeArg ウィジェットのサイズ（(0,0)で自動調整）
	/// @param rounding 角丸の半径
	/// @return ウィジェットが選択された場合にtrueを返します
	bool SelectableWithRounding(
		const char*   label, bool    selected, ImGuiSelectableFlags flags,
		const ImVec2& sizeArg, float rounding
	);

	/// @brief ImGui1.92.6から取ってきたBeginMenuウィジェット。角丸四角形で表示します。
	/// @param label メニューのラベル
	/// @param icon アイコン文字列（フォントアイコン）
	/// @param enabled メニューが有効かどうか
	/// @param rounding 角丸の半径
	/// @return メニューが開かれた場合にtrueを返します
	bool BeginMenuEx(
		const char* label, const char* icon, bool enabled, float rounding = 4.0f
	);

	/// @brief 角丸四角形のメニューアイテムを表示します。
	/// @param label メニューアイテムのラベル
	/// @param icon アイコン文字列（フォントアイコン）
	/// @param shortcut ショートカット文字列（省略可能）
	/// @param selected 選択状態かどうか
	/// @param enabled メニューアイテムが有効かどうか
	/// @param rounding 角丸の半径
	/// @return メニューアイテムが選択された場合にtrueを返します
	bool MenuItemExWithRounding(
		const char* label, const char* icon, const char* shortcut,
		bool        selected, bool     enabled, float    rounding
	);

	/// @brief 角丸四角形のイメージを表示します。
	/// @param textureId ImGuiのテクスチャID
	/// @param imageSize イメージの表示サイズ
	/// @param rounding 角丸の半径
	/// @param flags ImDrawFlagsのフラグ
	/// @param uv0 テクスチャのUV座標の左上
	/// @param uv1 テクスチャのUV座標の右下
	/// @param tintColor テクスチャの色の乗算
	void ImageWithRounding(
		ImTextureID textureId, ImVec2     imageSize,
		float       rounding, ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
		ImVec2      uv0 = ImVec2(0, 0), ImVec2 uv1 = ImVec2(1, 1),
		ImVec4      tintColor = ImVec4(1, 1, 1, 1)
	);

	/// @brief 「このソフトウェアについて」ウィンドウを表示します。
	/// @param systemName システム名
	/// @param version バージョン文字列
	/// @param logo ロゴのコードポイント(フォントアイコン)
	/// @param bShow ウィンドウの表示状態への参照。ウィンドウが閉じられた場合にfalseになります。
	void ShowAboutWindow(
		const std::string& systemName, const std::string& version,
		uint32_t           logo, bool&                    bShow
	);

	/// @brief アセットパスの入力 + D&D受け入れ + ピッカー起動を行うウィジェット
	/// @param label ラベル名
	/// @param path 編集対象のパス
	/// @param acceptedMask 受け入れ可能なアセットタイプマスク
	/// @param helpText 補助テキスト（省略可能）
	/// @return pathが変更された場合にtrue
	bool AssetPathPicker(
		const char*   label,
		std::string&  path,
		AssetTypeMask acceptedMask,
		const char*   helpText = nullptr
	);

	enum class HeaderMenuAction {
		None = 0,
		MoveUp,
		MoveDown,
		Remove,
	};

	/// @brief 折りたたみヘッダーとチェックボックスを組み合わせたウィジェットを表示します。
	/// @param icon アイコン（フォントアイコン）
	/// @param label ヘッダーのラベル
	/// @param id ヘッダーのID。GUIDを突っ込む
	/// @param checkbox チェックボックスの状態への参照
	/// @param action ヘッダーメニューで選択されたアクション
	/// @param canMoveUp 「上に移動」項目を有効化するかどうか
	/// @param canMoveDown 「下に移動」項目を有効化するかどうか
	/// @param canRemove 「削除」項目を有効化するかどうか
	/// @param flags ImGuiTreeNodeFlagsのフラグ
	/// @return ヘッダーが開かれた場合にtrueを返します
	/// @details 主にインスペクタで使用します。
	bool CollapsingHeaderWithCheckbox(
		uint32_t           icon,
		const char*        label,
		uint64_t           id,
		bool*              checkbox,
		HeaderMenuAction*  action,
		bool               canMoveUp   = true,
		bool               canMoveDown = true,
		bool               canRemove   = true,
		ImGuiTreeNodeFlags flags       = 0
	);
}
#endif
