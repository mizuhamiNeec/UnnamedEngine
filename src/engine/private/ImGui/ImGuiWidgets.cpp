#ifdef _DEBUG
#include <pch.h>
#include <array>
#include <imgui.h>
#include <imgui_internal.h>

#include <runtime/core/math/Math.h>

#include <engine/public/ImGui/Icons.h>

#include <runtime/core/Properties.h>

namespace ImGuiWidgets {
	void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered,
	                           const ImVec4& bgActive) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
	}

	bool DragVec3(const std::string& name, Vec3&                value,
	              const Vec3&        defaultValue, const float& vSpeed,
	              const char*        format) {
		if (ImGui::GetCurrentWindow()->SkipItems) {
			return false;
		}

		bool valueChanged = false;

		// 全体のウィジェット幅は通常の DragFloat3 と同じ
		const float widgetWidth = ImGui::CalcItemWidth();
		const float setWidth    = widgetWidth / 3.0f; // 各軸のセル幅

		// 角丸四角形のサイズ ImGuiから取ってくる
		const float rectWidth  = ImGui::GetFrameHeight();
		const float rectHeight = rectWidth;
		const float rounding   = ImGui::GetStyle().FrameRounding;
		const float dragWidth  = setWidth - rectWidth; // 各 DragFloat の幅

		static constexpr std::array rectColors = {
			IM_COL32(50, 43, 43, 255),
			IM_COL32(43, 45, 39, 255),
			IM_COL32(45, 48, 51, 255)
		};

		static constexpr std::array textColors = {
			IM_COL32(226, 110, 110, 255),
			IM_COL32(168, 204, 96, 255),
			IM_COL32(132, 181, 230, 255)
		};

		// リセットアイコンの文字列を作成 (UTF-8)
		const std::string iconResetStr = StrUtil::ConvertToUtf8(kIconReset);

		ImGui::PushID(name.c_str());
		ImGui::BeginGroup();
		{
			for (int i = 0; i < 3; i++) {
				ImGui::PushID(i);
				// Shiftキーが押されている場合は全要素をリセット
				if (ImGui::InvisibleButton("##reset",
				                           ImVec2(rectWidth, rectHeight))) {
					if (ImGui::GetIO().KeyShift) {
						for (int j = 0; j < 3; ++j) {
							value[j] = defaultValue[j];
						}
						valueChanged = true;
					} else {
						value[i]     = defaultValue[i];
						valueChanged = true;
					}
				}
				const bool isHovered = ImGui::IsItemHovered(); // マウスオーバー状態を取得

				ImVec2 rectPos = ImGui::GetItemRectMin();
				ImVec2 rectMax(rectPos.x + rectWidth, rectPos.y + rectHeight);
				ImGui::GetWindowDrawList()->AddRectFilled(
					rectPos,
					rectMax,
					rectColors[i],
					rounding,
					ImDrawFlags_RoundCornersTopLeft |
					ImDrawFlags_RoundCornersBottomLeft
				);

				constexpr std::array<const char*, 3> axisLabels = {
					"X", "Y", "Z"
				};
				if (ImGui::GetIO().KeyShift || isHovered) {
					// Shiftキーが押されている間はリセットアイコンを表示
					const ImVec2 iconSize = ImGui::CalcTextSize(
						iconResetStr.c_str());
					ImVec2 iconPos(
						rectPos.x + (rectWidth - iconSize.x) * 0.5f,
						rectPos.y + (rectHeight - iconSize.y) * 0.5f
					);
					ImGui::GetWindowDrawList()->AddText(
						iconPos, textColors[i], iconResetStr.c_str());
				} else {
					// 通常時は軸ラベルを表示
					const ImVec2 labelSize = ImGui::CalcTextSize(axisLabels[i]);
					ImVec2       labelPos(
						rectPos.x + (rectWidth - labelSize.x) * 0.5f,
						rectPos.y + (rectHeight - labelSize.y) * 0.5f
					);
					ImGui::GetWindowDrawList()->AddText(
						labelPos, textColors[i], axisLabels[i]);
				}

				// 角丸四角形と DragFloat の間に隙間を作らない
				ImGui::SameLine(0, 0);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
				ImGui::SetNextItemWidth(dragWidth - 2);
				valueChanged |= ImGui::DragFloat(
					"", &value[i], vSpeed, 0, 0, format);
				ImGui::PopStyleVar();

				if (i < 2) {
					ImGui::SameLine(0, 4); // 各セットの間隔
				}
				ImGui::PopID();
			}
		}
		ImGui::EndGroup();
		ImGui::PopID();

		// 右側に通常通りラベルを表示
		ImGui::SameLine();
		ImGui::Text("%s", name.c_str());

		return valueChanged;
	}

	bool EditCubicBezier(const std::string& label, float& p0, float& p1,
	                     float&             p2, float&    p3) {
		// ドラッグ中か?
		static bool bIsDraggingCp1 = false;
		static bool bIsDraggingCp2 = false;

		float controlPoints[] = {p0, p1, p2, p3};

		ImGui::DragFloat4(label.c_str(), controlPoints, 0.01f, 0.0f, 1.0f);

		// ウィンドウ内の利用可能領域から正方形のキャンバスサイズを決定
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float  side  = (avail.x < avail.y) ? avail.x : avail.y;
		ImVec2 canvasSize(side, side);
		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("canvas", canvasSize);

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// キャンバスの背景描画
		drawList->AddRectFilled(canvasPos,
		                        ImVec2(canvasPos.x + canvasSize.x,
		                               canvasPos.y + canvasSize.y),
		                        IM_COL32(50, 50, 50, 255));

		// Cubic Bézierの各点を計算・描画 （始点:P0=(0,0), 終点:P3=(1,1)固定）
		constexpr int steps = 64;
		ImVec2        prevPoint;
		bool          first = true;
		for (int i = 0; i < steps; ++i) {
			const float t         = static_cast<float>(i) / (steps - 1);
			const float oneMinusT = 1.0f - t;
			const float bx        = oneMinusT * oneMinusT * oneMinusT * 0.0f +
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
				drawList->AddLine(prevPoint, point, IM_COL32(255, 255, 0, 255),
				                  2.0f);
			}
			prevPoint = point;
			first     = false;
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
		const ImGuiIO& io       = ImGui::GetIO();
		const ImVec2   mousePos = io.MousePos;

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
		auto clamp = [
			](const float v, const float lo, const float hi) -> float {
			return (v < lo) ? lo : ((v > hi) ? hi : v);
		};
		if (bIsDraggingCp1) {
			const float nx = clamp((mousePos.x - canvasPos.x) / canvasSize.x,
			                       0.0f, 1.0f);
			const float ny = clamp((mousePos.y - canvasPos.y) / canvasSize.y,
			                       0.0f, 1.0f);
			// Y軸は反転しているため
			controlPoints[0] = nx;
			controlPoints[1] = 1.0f - ny;
		}
		if (bIsDraggingCp2) {
			const float nx = clamp((mousePos.x - canvasPos.x) / canvasSize.x,
			                       0.0f, 1.0f);
			const float ny = clamp((mousePos.y - canvasPos.y) / canvasSize.y,
			                       0.0f, 1.0f);
			controlPoints[2] = nx;
			controlPoints[3] = 1.0f - ny;
		}

		p0 = controlPoints[0];
		p1 = controlPoints[1];
		p2 = controlPoints[2];
		p3 = controlPoints[3];


		return bIsDraggingCp1 || bIsDraggingCp2;
	}

	bool IconButton(
		const char*    icon,
		const char*    label,
		ImVec2         size,
		const float    iconScale,
		const ImGuiDir labelDir
	) {
		// 1) レイアウト境界をまとめる
		ImGui::BeginGroup();
		ImDrawList* dl    = ImGui::GetWindowDrawList();
		ImVec2      start = ImGui::GetCursorScreenPos();
		ImGuiStyle& style = ImGui::GetStyle();
		ImFont*     font  = ImGui::GetFont();

		// 2) サイズを見積もる
		const bool  hasLabel = label && label[0];
		const float kGap     = (labelDir == ImGuiDir_Left || labelDir ==
			                       ImGuiDir_Right) ?
			                       style.ItemSpacing.x :
			                       style.ItemSpacing.y;

		// アイコン基準サイズ（フォント等倍時）
		const ImVec2 iconBaseSize = ImGui::CalcTextSize(icon);

		// 仮の高さが決まらないと iconFontSize が計算できないので、
		// まず label を含まない最小寸法で初期化しておく
		ImVec2 labelSize = hasLabel ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
		if (size.x <= 0.0f)
			size.x = std::max(iconBaseSize.x, labelSize.x);
		if (size.y <= 0.0f)
			size.y = iconBaseSize.y + (hasLabel ? (labelSize.y + kGap) : 0);

		ImVec2 pad        = style.FramePadding;
		ImVec2 innerStart = {start.x + pad.x, start.y + pad.y};
		ImVec2 innerSize  = {size.x - pad.x * 2.0f, size.y - pad.y * 2.0f};

		// 確定した高さからアイコン描画サイズを算出
		const float iconFontSize = innerSize.y * iconScale;
		ImGui::PushFont(font, iconFontSize);
		const ImVec2 iconSize = ImGui::CalcTextSize(icon);
		ImGui::PopFont();

		// 横レイアウト時は幅を再調整
		if (labelDir == ImGuiDir_Left || labelDir == ImGuiDir_Right)
			if (size.x <= 0.0f)
				size.x = iconSize.x + (hasLabel ? (labelSize.x + kGap) : 0);

		// 3) InvisibleButton でヒット領域登録
		const std::string btnId = "##IconBtn" + std::string(icon) +
			std::to_string(ImGui::GetID(icon));
		const bool pressed = ImGui::Button(btnId.c_str(), size);

		// 4) テキスト描画位置を計算
		ImVec2 iconPos, labelPos;

		switch (labelDir) {
		case ImGuiDir_Up:
		case ImGuiDir_Down: {
			// 垂直方向の合計サイズ
			const float blockH = iconSize.y + (hasLabel ?
				                                   (labelSize.y + kGap) :
				                                   0.0f);
			const float y0 = innerStart.y + (innerSize.y - blockH) * 0.5f;

			if (labelDir == ImGuiDir_Up) {
				labelPos = {start.x + (size.x - labelSize.x) * 0.5f, y0};
				iconPos  = {
					start.x + (size.x - iconSize.x) * 0.5f,
					y0 + labelSize.y + kGap
				};
			} else // Down (= デフォルト)
			{
				iconPos = {
					start.x + (size.x - iconSize.x) * 0.5f,
					y0
				};
				if (hasLabel)
					labelPos = {
						start.x + (size.x - labelSize.x) * 0.5f,
						y0 + iconSize.y + kGap
					};
			}
			break;
		}
		case ImGuiDir_Left:
		case ImGuiDir_Right:
		default: {
			// 水平方向の合計サイズ
			const float blockW = iconSize.x + (hasLabel ?
				                                   (labelSize.x + kGap) :
				                                   0.0f);
			const float x0 = innerStart.x + (innerSize.x - blockW) * 0.5f;
			// ブロック X 先頭
			const float cy = innerStart.y + (innerSize.y - std::max(
				iconSize.y, labelSize.y)) * 0.5f;

			if (labelDir == ImGuiDir_Left) {
				labelPos = {x0, cy + (iconSize.y - labelSize.y) * 0.5f};
				iconPos  = {x0 + labelSize.x + kGap, cy};
			} else // Right
			{
				iconPos = {x0, cy};
				if (hasLabel)
					labelPos = {
						x0 + iconSize.x + kGap,
						cy + (iconSize.y - labelSize.y) * 0.5f
					};
			}
			break;
		}
		}

		// 5) 描画
		const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
		dl->AddText(font, iconFontSize, iconPos, col, icon);

		if (hasLabel)
			dl->AddText(labelPos, col, label);

		// 6) グループ終了
		ImGui::EndGroup();
		return pressed;
	}

	bool MenuItemWithIcon(const char* icon, const char* label) {
		std::string ret;
		if (icon && icon[0] != '\0') {
			ret = std::string(icon) + " " + label;
		} else {
			ret = std::string(kIconSpace) + " " + label;
		}
		return ImGui::MenuItem(ret.c_str());
	}

	bool BeginMainMenu(const char* label, bool enabled) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
		                    ImVec2(ImGui::GetStyle().FramePadding.x,
		                           kTitleBarH * 0.5f - ImGui::GetFontSize() *
		                           0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
		                    ImVec2(ImGui::GetStyle().ItemSpacing.x,
		                           kTitleBarH));
		const bool ret = ImGui::BeginMenu(label, enabled);
		ImGui::PopStyleVar(2);
		return ret;
	}
}
#endif
