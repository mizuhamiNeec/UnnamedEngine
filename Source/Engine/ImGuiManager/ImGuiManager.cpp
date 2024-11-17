#include "ImGuiManager.h"
#include "../Renderer/SrvManager.h"

#ifdef _DEBUG
#include "imgui/imgui_internal.h"
#include "../Renderer/D3D12.h"
#include "../Window/Window.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include "../Lib/Utils/ClientProperties.h"
#include "../Window/WindowsUtils.h"

void ImGuiManager::Init(const D3D12* renderer, const Window* window, const SrvManager* srvManager) {
	renderer_ = renderer;
	srvManager_ = srvManager;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

	// 少し角丸に
	auto& style = ImGui::GetStyle();
	style.WindowRounding = 4;
	style.FrameRounding = 2;

	ImFontConfig imFontConfig;
	imFontConfig.OversampleH = 1;
	imFontConfig.OversampleV = 1;
	imFontConfig.PixelSnapH = true;
	imFontConfig.SizePixels = 18;

	// Ascii
	io.Fonts->AddFontFromFileTTF(R"(.\Resources\Fonts\JetBrainsMono.ttf)", 18.0f, &imFontConfig,
		io.Fonts->GetGlyphRangesDefault());
	imFontConfig.MergeMode = true;
	// 日本語フォールバック
	io.Fonts->AddFontFromFileTTF(R"(.\Resources\Fonts\NotoSansJP.ttf)", 18.0f, &imFontConfig,
		io.Fonts->GetGlyphRangesJapanese());

	// テーマの設定
	// TODO : 多分絶対いらないけどランタイムで変わったらカッコいいよね!!
	if (WindowsUtils::IsAppDarkTheme()) {
		StyleColorsDark();
	} else {
		ImGui::StyleColorsLight();
	}

	ImGui_ImplWin32_Init(window->GetWindowHandle());

	ImGui_ImplDX12_Init(
		renderer_->GetDevice(),
		kFrameBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		srvManager_->GetDescriptorHeap(),
		srvManager_->GetDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		srvManager_->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()
	);
}

void ImGuiManager::NewFrame() {
	// ImGuiの新しいフレームを開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
}

void ImGuiManager::EndFrame() const {
	// ImGuiのフレームを終了しレンダリング準備
	ImGui::Render();

	// ImGuiマルチビューポート用
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();


	ID3D12DescriptorHeap* imGuiHeap = srvManager_->GetDescriptorHeap();
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

void ImGuiManager::StyleColorsDark(ImGuiStyle* dst) {
	ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	//colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	//colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
	//colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	//colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	//colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	//colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	//colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
	//colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	//colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	//colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	//colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
	//colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	//colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	//colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	//colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	//colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	//colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	//colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	//colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	//colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	//colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	//colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	//colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	//colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	//colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	//colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	//colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
	//colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	//colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	//colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	//colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	//colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	//colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
	//colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.80f);
	//colors[ImGuiCol_TabSelected] = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
	//colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_HeaderActive];
	//colors[ImGuiCol_TabDimmed] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
	//colors[ImGuiCol_TabDimmedSelected] = ImLerp(colors[ImGuiCol_TabSelected], colors[ImGuiCol_TitleBg], 0.40f);
	//colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	//colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_HeaderActive] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
	//colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	//colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	//colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	//colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	//colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	//colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	//colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);   // Prefer using Alpha=1.0 here
	//colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);   // Prefer using Alpha=1.0 here
	//colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	//colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	//colors[ImGuiCol_TextLink] = colors[ImGuiCol_HeaderActive];
	//colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	//colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	//colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	//colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	//colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	//colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive) {
	ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
}

void EditTransform(const std::string& name, Transform& transform, const float& vSpeed) {
	constexpr ImVec4 xBg = { 0.72f, 0.11f, 0.11f, 0.75f };
	constexpr ImVec4 xBgHovered = { 0.83f, 0.18f, 0.18f, 0.75f };
	constexpr ImVec4 xBgActive = { 0.96f, 0.26f, 0.21f, 0.75f };

	constexpr ImVec4 yBg = { 0.11f, 0.37f, 0.13f, 0.75f };
	constexpr ImVec4 yBgHovered = { 0.22f, 0.56f, 0.24f, 0.75f };
	constexpr ImVec4 yBgActive = { 0.3f, 0.69f, 0.31f, 0.75f };

	constexpr ImVec4 zBg = { 0.05f, 0.28f, 0.63f, 0.75f };
	constexpr ImVec4 zBgHovered = { 0.1f, 0.46f, 0.82f, 0.75f };
	constexpr ImVec4 zBgActive = { 0.13f, 0.59f, 0.95f, 0.75f };

	// 幅を取得
	const float width = ImGui::GetCurrentContext()->Style.ItemInnerSpacing.x;

	if (ImGui::CollapsingHeader(("Transform##" + name).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
		// XYZの3つ分
		constexpr int components = 3;
		// 回転を取っておく
		Vec3 rotate = transform.rotate * Math::rad2Deg;

		// 幅を決定
		ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

		/* --- 座標 --- */
		// 色を送る
		PushStyleColorForDrag(xBg, xBgHovered, xBgActive);
		ImGui::DragFloat(("##posX" + name).c_str(), &transform.translate.x, vSpeed, 0.0f, 0.0f, "X %.3f");
		// 色を取り出す
		ImGui::PopStyleColor(components);

		// 同じ行に配置
		ImGui::SameLine(0, width);

		PushStyleColorForDrag(yBg, yBgHovered, yBgActive);
		ImGui::DragFloat(("##posY" + name).c_str(), &transform.translate.y, vSpeed, 0.0f, 0.0f, "Y %.3f");
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);

		PushStyleColorForDrag(zBg, zBgHovered, zBgActive);
		ImGui::DragFloat(("##posZ" + name).c_str(), &transform.translate.z, vSpeed, 0.0f, 0.0f, "Z %.3f");
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);
		ImGui::Text("Position");

		/* --- 回転 --- */
		PushStyleColorForDrag(xBg, xBgHovered, xBgActive);
		if (ImGui::DragFloat(("##rotX" + name).c_str(), &rotate.x, 10.0f * vSpeed, 0.0f, 0.0f, "X %.3f")) {
			transform.rotate.x = rotate.x * Math::deg2Rad;
		}
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);

		PushStyleColorForDrag(yBg, yBgHovered, yBgActive);
		if (ImGui::DragFloat(("##rotY" + name).c_str(), &rotate.y, 10.0f * vSpeed, 0.0f, 0.0f, "Y %.3f")) {
			transform.rotate.y = rotate.y * Math::deg2Rad;
		}
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);

		PushStyleColorForDrag(zBg, zBgHovered, zBgActive);
		if (ImGui::DragFloat(("##rotZ" + name).c_str(), &rotate.z, 10.0f * vSpeed, 0.0f, 0.0f, "Z %.3f")) {
			transform.rotate.z = rotate.z * Math::deg2Rad;
		}
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);
		ImGui::Text("Rotate");

		/* --- 拡縮 --- */
		PushStyleColorForDrag(xBg, xBgHovered, xBgActive);
		ImGui::DragFloat(("##scaleX" + name).c_str(), &transform.scale.x, vSpeed, 0.0f, 0.0f, "X %.3f");
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);

		PushStyleColorForDrag(yBg, yBgHovered, yBgActive);
		ImGui::DragFloat(("##scaleY" + name).c_str(), &transform.scale.y, vSpeed, 0.0f, 0.0f, "Y %.3f");
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);

		PushStyleColorForDrag(zBg, zBgHovered, zBgActive);
		ImGui::DragFloat(("##scaleZ" + name).c_str(), &transform.scale.z, vSpeed, 0.0f, 0.0f, "Z %.3f");
		ImGui::PopStyleColor(components);

		ImGui::SameLine(0, width);
		ImGui::Text("Scale");

		for (int i = 0; i < components; ++i) {
			ImGui::PopItemWidth();
		}
	}
}

void TextOutlined(
	ImDrawList* drawList,
	const ImVec2& pos,
	const char* text,
	const ImU32 textColor,
	const ImU32 outlineColor,
	const float outlineSize
) {
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y), outlineColor, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y), outlineColor, text);
	drawList->AddText(ImVec2(pos.x, pos.y - outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x, pos.y + outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y - outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y - outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y + outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y + outlineSize), outlineColor, text);
	drawList->AddText(pos, textColor, text);
}
#else
void ImGuiManager::Shutdown() {
}
#endif
