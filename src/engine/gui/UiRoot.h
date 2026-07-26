#pragma once
#include <memory>
#include <vector>

#include "Rect.h"
#include "UiDocument.h"
#include "UiDrawCommand.h"

namespace Unnamed::Gui {
	class UiWidget;

	/// @brief UiRootは、GUI階層の最上位でレイアウト、入力配送、描画コマンド生成を統括します
	class UiRoot {
	public:
		UiRoot();
		~UiRoot();

		/// @brief ルートウィジェットの矩形を設定します。
		/// @param rect 新しい矩形
		void SetRootRect(const Rect& rect);

		/// @brief ルートウィジェットのサイズを設定します。
		/// @param width 幅
		/// @param height 高さ
		void SetRootSize(float width, float height);

		/// @brief ルートウィジェットに子ウィジェットを追加します。
		/// @param child 追加する子ウィジェット
		void AddChild(std::unique_ptr<UiWidget> child) const;

		/// @brief ルートウィジェットから子ウィジェットの所有権を取り出します。
		/// @param child 取り出す子ウィジェット
		/// @return 取り出された子ウィジェット（見つからない場合はnullptr）
		std::unique_ptr<UiWidget> TakeChild(const UiWidget* child) const;

		/// @brief 更新処理
		/// @param deltaTime 経過時間
		void Tick(float deltaTime) const;

		/// @brief レイアウトを更新します。
		void UpdateLayout() const;

		/// @brief 描画コマンドを構築します。
		/// @param out 描画コマンドの出力先ベクター
		void BuildDrawCommands(std::vector<UiDrawCommand>& out) const;

		/// @brief ルートウィジェットを取得します。
		/// @return ルートウィジェットへのポインタ
		[[nodiscard]] UiWidget* GetRootWidget() const;

		/// @brief マウス入力を処理します。
		/// @param mouseX マウスのX座標
		/// @param mouseY マウスのY座標
		/// @param leftDown 左ボタンが押されているかどうか
		/// @param leftPressed 左ボタンが押された瞬間かどうか
		/// @param leftReleased 左ボタンが離された瞬間かどうか
		void ProcessMouse(
			float mouseX, float  mouseY,
			bool  leftDown, bool leftPressed, bool leftReleased
		);

	private:
		std::unique_ptr<UiWidget> mRootWidget;
		Rect                      mRootRect;

		UiWidget* mHoveredWidget = nullptr;
		UiWidget* mPressedWidget = nullptr;
	};
}
