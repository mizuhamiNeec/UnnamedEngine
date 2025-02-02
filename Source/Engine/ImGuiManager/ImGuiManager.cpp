#include "ImGuiManager.h"

#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <Entity/Base/Entity.h>
#include <ResourceSystem/SRV/ShaderResourceViewManager.h>

#ifdef _DEBUG
#include "../Lib/Utils/ClientProperties.h"
#include "../Renderer/D3D12.h"
#include "../Window/Window.h"
#include "../Window/WindowsUtils.h"

ImGuiManager::ImGuiManager(const D3D12* renderer, const ShaderResourceViewManager* srvManager) : renderer_(renderer), srvManager_(srvManager) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

	// 少し角丸に
	ImGuiStyle* style = &ImGui::GetStyle();
	style->WindowRounding = 4;
	style->FrameRounding = 2;

	ImFontConfig imFontConfig;
	imFontConfig.OversampleH = 1;
	imFontConfig.OversampleV = 1;
	imFontConfig.PixelSnapH = true;
	imFontConfig.SizePixels = 18;
	imFontConfig.GlyphMinAdvanceX = 2.0f;

	// Ascii
	io.Fonts->AddFontFromFileTTF(
		R"(.\Resources\Fonts\JetBrainsMono.ttf)", 18.0f, &imFontConfig, io.Fonts->GetGlyphRangesDefault()
	);
	imFontConfig.MergeMode = true;

	// 日本語フォールバック
	io.Fonts->AddFontFromFileTTF(
		R"(.\Resources\Fonts\NotoSansJP.ttf)", 18.0f, &imFontConfig, io.Fonts->GetGlyphRangesJapanese()
	);

	// 何故かベースラインがずれるので補正
	imFontConfig.GlyphOffset = ImVec2(0.0f, 5.0f);

	static constexpr ImWchar iconRanges[] = { 0xe003, 0xf8ff, 0 };
	io.Fonts->AddFontFromFileTTF(
		R"(.\Resources\Fonts\MaterialSymbolsRounded_Filled_28pt-Regular.ttf)",
		24.0f, &imFontConfig, iconRanges
	);

	// テーマの設定
	if (WindowsUtils::IsAppDarkTheme()) {
		StyleColorsDark();
	} else {
		StyleColorsLight();
	}

	ImGui_ImplWin32_Init(Window::GetWindowHandle());

	ImGui_ImplDX12_Init(
		renderer_->GetDevice(),
		kFrameBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		srvManager_->GetDescriptorHeap().Get(),
		srvManager_->GetDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		srvManager_->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()
	);
}

void ImGuiManager::NewFrame() {
	// ImGuiの新しいフレームを開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiManager::EndFrame() const {
	// ImGuiのフレームを終了しレンダリング準備
	ImGui::Render();

	// ImGuiマルチビューポート用
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	ID3D12DescriptorHeap* imGuiHeap = srvManager_->GetDescriptorHeap().Get();
	renderer_->GetCommandList()->SetDescriptorHeaps(1, &imGuiHeap);

	// 実際のCommandListのImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), renderer_->GetCommandList());

	ImGui::EndFrame();
}

void ImGuiManager::Shutdown() {
	ImGui::DestroyPlatformWindows();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	srvHeap_.Reset();
}

void ImGuiManager::StyleColorsDark() {
	ImGui::StyleColorsDark();

	ImGuiStyle* style = &ImGui::GetStyle();
	// テキストの色を少し暗めに
	ImVec4* colors = style->Colors;
	colors[ImGuiCol_Text] = ImVec4(0.71f, 0.71f, 0.71f, 1.0f);
}

void ImGuiManager::StyleColorsLight() {
	ImGui::StyleColorsLight();
}

void ImGuiManager::PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive) {
	ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
}

bool ImGuiManager::EditTransform(Transform& transform, const float& vSpeed) {
	bool isEditing = false;

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		isEditing |= DragVec3("Position", transform.translate, vSpeed, "%.3f");

		// 回転を取っておく
		Vec3 rotate = transform.rotate * Math::rad2Deg;
		if (DragVec3("Rotation", transform.rotate, vSpeed, "%.3f")) {
			isEditing |= true;
			transform.rotate = rotate * Math::deg2Rad;
		}

		isEditing |= DragVec3("Scale", transform.scale, vSpeed, "%.3f");
	}

	return isEditing;
}

bool ImGuiManager::EditTransform(TransformComponent& transform, const float& vSpeed) {
	bool isEditing = false;
	Vec3 localPos = transform.GetLocalPos();
	Quaternion localRot = transform.GetLocalRot();
	Vec3 localScale = transform.GetLocalScale();

	if (ImGui::CollapsingHeader(("Transform##" + transform.GetOwner()->GetName()).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
		// Position 編集
		if (DragVec3("Position", localPos, vSpeed, "%.3f")) {
			transform.SetLocalPos(localPos);
			isEditing = true;
		}

		// Rotation 編集
		Vec3 eulerDegrees = localRot.ToEulerDegrees();
		if (DragVec3("Rotation", eulerDegrees, vSpeed * 10.0f, "%.3f")) {
			// 編集された Euler 角を Quaternion に変換
			localRot = Quaternion::EulerDegrees(eulerDegrees);
			transform.SetLocalRot(localRot);
			isEditing = true;
		}

		// Scale 編集
		if (DragVec3("Scale", localScale, vSpeed, "%.3f")) {
			transform.SetLocalScale(localScale);
			isEditing = true;
		}
	}
	return isEditing;
}

bool ImGuiManager::DragVec3(const std::string& name, Vec3& v, const float& vSpeed, const char* format) {
	// 編集中かどうか
	bool isEditing = false;

	// XYZの3つ分
	constexpr int components = 3;

	// 幅を取得
	const float width = ImGui::GetCurrentContext()->Style.ItemInnerSpacing.x;

	constexpr ImVec4 xBg = { 0.72f, 0.11f, 0.11f, 0.75f };
	constexpr ImVec4 xBgHovered = { 0.83f, 0.18f, 0.18f, 0.75f };
	constexpr ImVec4 xBgActive = { 0.96f, 0.26f, 0.21f, 0.75f };

	constexpr ImVec4 yBg = { 0.11f, 0.37f, 0.13f, 0.75f };
	constexpr ImVec4 yBgHovered = { 0.22f, 0.56f, 0.24f, 0.75f };
	constexpr ImVec4 yBgActive = { 0.3f, 0.69f, 0.31f, 0.75f };

	constexpr ImVec4 zBg = { 0.05f, 0.28f, 0.63f, 0.75f };
	constexpr ImVec4 zBgHovered = { 0.1f, 0.46f, 0.82f, 0.75f };
	constexpr ImVec4 zBgActive = { 0.13f, 0.59f, 0.95f, 0.75f };

	// 幅を決定
	ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

	/* --- 座標 --- */
	// 色を送る
	ImGui::PushID("X");
	PushStyleColorForDrag(xBg, xBgHovered, xBgActive);
	isEditing |= ImGui::DragFloat(("##X" + name).c_str(), &v.x, vSpeed, 0.0f, 0.0f, format);
	ImGui::PopStyleColor(components);
	ImGui::PopID();
	ImGui::SameLine(0, width);

	ImGui::PushID("Y");
	PushStyleColorForDrag(yBg, yBgHovered, yBgActive);
	isEditing |= ImGui::DragFloat(("##Y" + name).c_str(), &v.y, vSpeed, 0.0f, 0.0f, format);
	ImGui::PopStyleColor(components);
	ImGui::PopID();
	ImGui::SameLine(0, width);

	ImGui::PushID("Z");
	PushStyleColorForDrag(zBg, zBgHovered, zBgActive);
	isEditing |= ImGui::DragFloat(("##Z" + name).c_str(), &v.z, vSpeed, 0.0f, 0.0f, format);
	ImGui::PopStyleColor(components);
	ImGui::PopID();
	ImGui::SameLine(0, width);

	ImGui::Text(name.c_str());

	for (int i = 0; i < components; i++) {
		ImGui::PopItemWidth();
	}

	return isEditing;
}

void ImGuiManager::TextOutlined(
	ImDrawList* drawList,
	const ImVec2& pos,
	const char* text,
	const ImVec4 textColor,
	const ImVec4 outlineColor,
	const float outlineSize
) {
	ImU32 outlineCol = ImGui::ColorConvertFloat4ToU32(outlineColor);
	ImU32 textCol = ImGui::ColorConvertFloat4ToU32(textColor);

	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y), outlineCol, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y), outlineCol, text);
	drawList->AddText(ImVec2(pos.x, pos.y - outlineSize), outlineCol, text);
	drawList->AddText(ImVec2(pos.x, pos.y + outlineSize), outlineCol, text);
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y - outlineSize), outlineCol, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y - outlineSize), outlineCol, text);
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y + outlineSize), outlineCol, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y + outlineSize), outlineCol, text);
	drawList->AddText(pos, textCol, text);
}

bool ImGuiManager::IconButton(const char* icon, const char* label, const ImVec2& size) {
	// ボタンのサイズを計算
	const ImVec2 iconSize = ImGui::CalcTextSize(icon);
	// フォントサイズを小さくしてラベルサイズを計算
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // デフォルトフォントに切り替え
	const ImVec2 labelSize = ImGui::CalcTextSize(label);
	ImGui::PopFont();

	const float spacing = ImGui::GetStyle().ItemSpacing.y;

	// 必要なサイズを計算
	ImVec2 totalSize = ImVec2(
		max(iconSize.x, labelSize.x) + 20.0f, // パディングを追加
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
#else
void ImGuiManager::Shutdown() {}
#endif
