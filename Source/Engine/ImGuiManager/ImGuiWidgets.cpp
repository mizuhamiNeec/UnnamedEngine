#include "ImGuiWidgets.h"

#ifdef _DEBUG
#include <imgui.h>
#include <imgui_internal.h>
#endif

namespace ImGuiWidgets {
	void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
	}

	bool DragVec3(const std::string& name, Vec3& v, const float& vSpeed, const char* format) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		bool value_changed = false;

		// 全体のウィジェット幅は通常の DragFloat3 と同じ
		float widgetWidth = ImGui::CalcItemWidth();
		float setWidth = widgetWidth / 3.0f; // 各軸のセル幅

		// 角丸四角形のサイズ
		float rectWidth = 24.0f;
		float rectHeight = 24.0f;
		float rounding = 4.0f;
		float dragWidth = setWidth - rectWidth; // 各 DragFloat の幅

		const char* axisLabels[3] = { "X", "Y", "Z" };
		static const ImU32 rectColors[3] = {
			IM_COL32(50, 43, 43, 255),
			IM_COL32(43, 45, 39, 255),
			IM_COL32(45, 48, 51, 255)
		};
		static const ImU32 textColors[3] = {
			IM_COL32(226, 110, 110, 255),
			IM_COL32(168, 204, 96, 255),
			IM_COL32(132, 181, 230, 255)
		};

		ImGui::PushID(name.c_str());
		ImGui::BeginGroup();
		{
			for (int i = 0; i < 3; i++) {
				ImGui::PushID(i);
				// 角丸四角形のダミー領域を描画
				ImGui::Dummy(ImVec2(rectWidth, rectHeight));
				ImVec2 rectPos = ImGui::GetItemRectMin();
				ImVec2 rectMax(rectPos.x + rectWidth, rectPos.y + rectHeight);
				ImGui::GetWindowDrawList()->AddRectFilled(rectPos, rectMax, rectColors[i], rounding, ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);

				ImVec2 labelSize = ImGui::CalcTextSize(axisLabels[i]);
				ImVec2 labelPos(rectPos.x + (rectWidth - labelSize.x) * 0.5f,
					rectPos.y + (rectHeight - labelSize.y) * 0.5f);
				ImGui::GetWindowDrawList()->AddText(labelPos, textColors[i], axisLabels[i]);

				// 角丸四角形と DragFloat の間に隙間を作らない
				ImGui::SameLine(0, 0);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
				ImGui::SetNextItemWidth(dragWidth - 2);
				value_changed |= ImGui::DragFloat("", &v[i], vSpeed, 0, 0, format);
				ImGui::PopID();

				ImGui::PopStyleVar(); // `FrameRounding` の変更を元に戻す

				// 各セットの後に `SameLine()` を適用して 1 行に並ぶようにする
				if (i < 2) {
					ImGui::SameLine(0, 4); // 間隔を調整
				}
			}
		}
		ImGui::EndGroup();
		ImGui::PopID();

		// 右側に通常通りラベルを表示
		ImGui::SameLine();
		ImGui::Text("%s", name.c_str());

		return value_changed;
		//return value_changed;

		//// 編集中かどうか
		//bool isEditing = false;

		//// XYZの3つ分
		//constexpr int components = 3;

		//// 幅を取得
		//const float width = ImGui::GetCurrentContext()->Style.ItemInnerSpacing.x;

		//constexpr ImVec4 xBg = { 0.72f, 0.11f, 0.11f, 0.75f };
		//constexpr ImVec4 xBgHovered = { 0.83f, 0.18f, 0.18f, 0.75f };
		//constexpr ImVec4 xBgActive = { 0.96f, 0.26f, 0.21f, 0.75f };

		//constexpr ImVec4 yBg = { 0.11f, 0.37f, 0.13f, 0.75f };
		//constexpr ImVec4 yBgHovered = { 0.22f, 0.56f, 0.24f, 0.75f };
		//constexpr ImVec4 yBgActive = { 0.3f, 0.69f, 0.31f, 0.75f };

		//constexpr ImVec4 zBg = { 0.05f, 0.28f, 0.63f, 0.75f };
		//constexpr ImVec4 zBgHovered = { 0.1f, 0.46f, 0.82f, 0.75f };
		//constexpr ImVec4 zBgActive = { 0.13f, 0.59f, 0.95f, 0.75f };

		//// 幅を決定
		//ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

		///* --- 座標 --- */
		//// 色を送る
		//ImGui::PushID("X");
		//PushStyleColorForDrag(xBg, xBgHovered, xBgActive);
		//isEditing |= ImGui::DragFloat(("##X" + name).c_str(), &v.x, vSpeed, 0.0f, 0.0f, format);
		//ImGui::PopStyleColor(components);
		//ImGui::PopID();
		//ImGui::SameLine(0, width);

		//ImGui::PushID("Y");
		//PushStyleColorForDrag(yBg, yBgHovered, yBgActive);
		//isEditing |= ImGui::DragFloat(("##Y" + name).c_str(), &v.y, vSpeed, 0.0f, 0.0f, format);
		//ImGui::PopStyleColor(components);
		//ImGui::PopID();
		//ImGui::SameLine(0, width);

		//ImGui::PushID("Z");
		//PushStyleColorForDrag(zBg, zBgHovered, zBgActive);
		//isEditing |= ImGui::DragFloat(("##Z" + name).c_str(), &v.z, vSpeed, 0.0f, 0.0f, format);
		//ImGui::PopStyleColor(components);
		//ImGui::PopID();
		//ImGui::SameLine(0, width);

		//ImGui::Text(name.c_str());

		//for (int i = 0; i < components; i++) {
		//	ImGui::PopItemWidth();
		//}

		//return value_changed;
	}

	bool EditCubicBezier(const std::string& label, float& p0, float& p1, float& p2, float& p3) {
		// ドラッグ中か?
		static bool bIsDraggingCp1 = false;
		static bool bIsDraggingCp2 = false;

		float controlPoints[] = { p0, p1, p2, p3 };

		ImGui::DragFloat4(label.c_str(), controlPoints, 0.01f, 0.0f, 1.0f);

		// ウィンドウ内の利用可能領域から正方形のキャンバスサイズを決定
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float side = (avail.x < avail.y) ? avail.x : avail.y;
		ImVec2 canvasSize(side, side);
		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("canvas", canvasSize);

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// キャンバスの背景描画
		drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(50, 50, 50, 255));

		// Cubic Bézierの各点を計算・描画 （始点:P0=(0,0), 終点:P3=(1,1)固定）
		constexpr int steps = 64;
		ImVec2 prevPoint;
		bool first = true;
		for (int i = 0; i < steps; ++i) {
			const float t = static_cast<float>(i) / (steps - 1);
			const float oneMinusT = 1.0f - t;
			const float bx = oneMinusT * oneMinusT * oneMinusT * 0.0f +
				3 * oneMinusT * oneMinusT * t * controlPoints[0] +
				3 * oneMinusT * t * t * controlPoints[2] +
				t * t * t * 1.0f;
			const float by = oneMinusT * oneMinusT * oneMinusT * 0.0f +
				3 * oneMinusT * oneMinusT * t * controlPoints[1] +
				3 * oneMinusT * t * t * controlPoints[3] +
				t * t * t * 1.0f;
			auto point = ImVec2(
				canvasPos.x + bx * canvasSize.x,
				canvasPos.y + (1.0f - by) * canvasSize.y
			);

			if (!first) {
				drawList->AddLine(prevPoint, point, IM_COL32(255, 255, 0, 255), 2.0f);
			}
			prevPoint = point;
			first = false;
		}

		// 各コントロールポイントのキャンバス上の位置を計算
		ImVec2 cp1 = ImVec2(
			canvasPos.x + controlPoints[0] * canvasSize.x,
			canvasPos.y + (1.0f - controlPoints[1]) * canvasSize.y
		);
		ImVec2 cp2 = ImVec2(
			canvasPos.x + controlPoints[2] * canvasSize.x,
			canvasPos.y + (1.0f - controlPoints[3]) * canvasSize.y
		);

		// 円描画（各コントロールポイント）
		constexpr float handleRadius = 16.0f;
		drawList->AddCircleFilled(cp1, handleRadius, IM_COL32(255, 0, 0, 255));
		drawList->AddCircleFilled(cp2, handleRadius, IM_COL32(255, 0, 0, 255));

		// マウス操作による直接操作
		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 mousePos = io.MousePos;

		auto distance = [](const ImVec2& a, const ImVec2& b) -> float {
			const float dx = a.x - b.x;
			const float dy = a.y - b.y;
			return sqrtf(dx * dx + dy * dy);
			};

		// マウスクリック判定：クリック時、各コントロールポイントの円領域内ならドラッグ開始
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
			if (distance(mousePos, cp1) <= handleRadius * 1.5f) {
				bIsDraggingCp1 = true;
			} else if (distance(mousePos, cp2) <= handleRadius * 1.5f) {
				bIsDraggingCp2 = true;
			}
		}
		// マウスボタンリリースでドラッグ終了
		if (!io.MouseDown[0]) {
			bIsDraggingCp1 = bIsDraggingCp2 = false;
		}

		// ドラッグ中なら、マウス位置に応じてコントロールポイントを更新（キャンバス座標を[0,1]に変換）
		auto clamp = [](const float v, const float lo, const float hi) -> float {
			return (v < lo) ? lo : ((v > hi) ? hi : v);
			};
		if (bIsDraggingCp1) {
			const float nx = clamp((mousePos.x - canvasPos.x) / canvasSize.x, 0.0f, 1.0f);
			const float ny = clamp((mousePos.y - canvasPos.y) / canvasSize.y, 0.0f, 1.0f);
			// Y軸は反転しているため
			controlPoints[0] = nx;
			controlPoints[1] = 1.0f - ny;
		}
		if (bIsDraggingCp2) {
			const float nx = clamp((mousePos.x - canvasPos.x) / canvasSize.x, 0.0f, 1.0f);
			const float ny = clamp((mousePos.y - canvasPos.y) / canvasSize.y, 0.0f, 1.0f);
			controlPoints[2] = nx;
			controlPoints[3] = 1.0f - ny;
		}

		p0 = controlPoints[0];
		p1 = controlPoints[1];
		p2 = controlPoints[2];
		p3 = controlPoints[3];


		return bIsDraggingCp1 || bIsDraggingCp2;
	}

	bool IconButton(const char* icon, const char* label, const ImVec2& size) {
		// ボタンのサイズを計算
		const ImVec2 iconSize = ImGui::CalcTextSize(icon);
		// フォントサイズを小さくしてラベルサイズを計算
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // デフォルトフォントに切り替え
		const ImVec2 labelSize = ImGui::CalcTextSize(label);
		ImGui::PopFont();

		const float spacing = ImGui::GetStyle().ItemSpacing.y;

		// 必要なサイズを計算
		ImVec2 totalSize = ImVec2(
			std::max(iconSize.x, labelSize.x) + 20.0f, // パディングを追加
			iconSize.y + labelSize.y + spacing
		);

		// ボタンのサイズを指定
		ImVec2 buttonSize = size.x > 0 && size.y > 0 ? size : totalSize;

		// ボタンの開始位置を記録
		ImVec2 buttonPos = ImGui::GetCursorPos();

		// ボタンを描画
		bool pressed = ImGui::Button("##icon_button", buttonSize);

		// アイコンを中央揃えで描画
		float iconY = buttonPos.y + (buttonSize.y - (iconSize.y + labelSize.y + spacing)) * 0.5f;
		ImGui::SetCursorPos(ImVec2(
			buttonPos.x + (buttonSize.x - iconSize.x) * 0.5f,
			iconY
		));
		ImGui::Text("%s", icon);

		// ラベルを小さいフォントで中央揃えで描画
		ImGui::SetCursorPos(ImVec2(
			buttonPos.x + (buttonSize.x - labelSize.x * 0.8f) * 0.5f,
			iconY + iconSize.y + spacing
		));

		float defaultFontSize = ImGui::GetFont()->Scale;
		ImGui::GetFont()->Scale = 0.8f;
		ImGui::PushFont(ImGui::GetFont());
		ImGui::Text("%s", label);
		ImGui::GetFont()->Scale = defaultFontSize;
		ImGui::PopFont();

		return pressed;
	}
}
