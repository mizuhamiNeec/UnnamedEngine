#include "ImGuiManager.h"

#include <Entity/Base/Entity.h>

#include <ResourceSystem/SRV/ShaderResourceViewManager.h>

#include <Window/WindowManager.h>


#ifdef _DEBUG
#include "ImGuiWidgets.h"
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <Lib/Utils/ClientProperties.h>

#include <Renderer/D3D12.h>

#include <Window/WindowsUtils.h>

ImGuiManager::ImGuiManager(D3D12*            renderer,
                           const SrvManager* srvManager) :
	renderer_(renderer),
	srvManager_(srvManager) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

	// 少し角丸に
	ImGuiStyle* style     = &ImGui::GetStyle();
	style->WindowRounding = 8;
	style->FrameRounding  = 4;

	ImFontConfig imFontConfig;
	imFontConfig.OversampleH      = 1;
	imFontConfig.OversampleV      = 1;
	imFontConfig.PixelSnapH       = true;
	imFontConfig.SizePixels       = 18;
	imFontConfig.GlyphMinAdvanceX = 2.0f;

	// Ascii
	io.Fonts->AddFontFromFileTTF(
		R"(.\Resources\Fonts\JetBrainsMono.ttf)", 18.0f, &imFontConfig,
		io.Fonts->GetGlyphRangesDefault()
	);
	imFontConfig.MergeMode = true;

	// 日本語フォールバック
	io.Fonts->AddFontFromFileTTF(
		R"(.\Resources\Fonts\NotoSansJP.ttf)", 18.0f, &imFontConfig,
		io.Fonts->GetGlyphRangesJapanese()
	);

	// ??? 何故かベースラインがずれるので補正
	imFontConfig.GlyphOffset = ImVec2(0.0f, 5.0f);

	static constexpr ImWchar iconRanges[] = {0xe003, 0xf8ff, 0};
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

	ImGui_ImplWin32_Init(WindowManager::GetMainWindow()->GetWindowHandle());

	// ImGuiの初期化
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device                  = renderer_->GetDevice();
	init_info.NumFramesInFlight       = kFrameBufferCount;
	init_info.RTVFormat               = DXGI_FORMAT_R8G8B8A8_UNORM;
	init_info.SrvDescriptorHeap       = srvManager_->GetDescriptorHeap();
	init_info.CommandQueue            = renderer_->GetCommandQueue();

	// ディスクリプタ割り当て関数を実装
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info,
	                                    D3D12_CPU_DESCRIPTOR_HANDLE*
	                                    out_cpu_handle,
	                                    D3D12_GPU_DESCRIPTOR_HANDLE*
	                                    out_gpu_handle) {
		// 有効なディスクリプタを割り当てるコード
		*out_cpu_handle = info->SrvDescriptorHeap->
		                        GetCPUDescriptorHandleForHeapStart();
		*out_gpu_handle = info->SrvDescriptorHeap->
		                        GetGPUDescriptorHandleForHeapStart();
	};

	ImGui_ImplDX12_Init(&init_info);
}

void ImGuiManager::NewFrame() {
	// ImGuiの新しいフレームを開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiManager::EndFrame() {
	// ImGuiのフレームを終了しレンダリング準備
	ImGui::Render();

	// レンダーターゲットとして使用されたテクスチャリソースをすべてSRV状態に遷移
	// これによりImGuiのマルチビューポートでも安全にテクスチャとして使用できる
	ID3D12DescriptorHeap* imGuiHeap =
		ShaderResourceViewManager::GetDescriptorHeap().Get();
	renderer_->GetCommandList()->SetDescriptorHeaps(1, &imGuiHeap);

	// メインウィンドウのImGuiコンテンツを描画
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),
	                              renderer_->GetCommandList());

	// マルチビューポートの更新前に、すべてのコマンドを確実にフラッシュする
	renderer_->GetCommandList()->Close();
	ID3D12CommandList* cmdLists[] = {renderer_->GetCommandList()};
	renderer_->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);

	renderer_->WaitPreviousFrame();

	// マルチビューポート用のレンダリング（新しいコマンドリストが内部で作成される）
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	// 新しいコマンドリストを作成して作業を続行
	renderer_->ResetCommandList();

	ImGui::EndFrame();
}

void ImGuiManager::Shutdown() {
	ImGui::DestroyPlatformWindows();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	srvHeap_.Reset();
}

void ImGuiManager::Recreate() const {
	// 1. ImGuiのDX12/Win32バックエンドを完全にシャットダウン
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();

	// 2. Win32バックエンドを再初期化（ウィンドウハンドルを渡す）
	ImGui_ImplWin32_Init(WindowManager::GetMainWindow()->GetWindowHandle());

	// 3. DX12バックエンドを再初期化（ディスクリプタヒープ等を渡す）
	ImGui_ImplDX12_Init(
		renderer_->GetDevice(),
		kFrameBufferCount,
		kBufferFormat,
		ShaderResourceViewManager::GetDescriptorHeap().Get(),
		ShaderResourceViewManager::GetDescriptorHeap()->
		GetCPUDescriptorHandleForHeapStart(),
		ShaderResourceViewManager::GetDescriptorHeap()->
		GetGPUDescriptorHandleForHeapStart()
	);

	// 4. デバイスオブジェクト再作成
	ImGui_ImplDX12_CreateDeviceObjects();
}

void ImGuiManager::StyleColorsDark() {
	ImGuiStyle& style  = ImGui::GetStyle();
	ImVec4*     colors = style.Colors;

	colors[ImGuiCol_WindowBg]              = ImVec4(0.14f, 0.14f, 0.14f, 0.94f);
	colors[ImGuiCol_Border]                = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
	colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
	colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
	colors[ImGuiCol_TitleBg]               = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	colors[ImGuiCol_TitleBgActive]         = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
	colors[ImGuiCol_ButtonHovered]         = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
	colors[ImGuiCol_ButtonActive]          = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
	colors[ImGuiCol_Header]                = ImVec4(0.23f, 0.23f, 0.23f, 1.0f);
	colors[ImGuiCol_HeaderHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
	colors[ImGuiCol_HeaderActive]          = ImVec4(0.32f, 0.32f, 0.32f, 1.0f);
	colors[ImGuiCol_ResizeGrip]            = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
	colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
	colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
	colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
	colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
	colors[ImGuiCol_Text]                  = ImVec4(0.71f, 0.71f, 0.71f, 1.0f);
	colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
	colors[ImGuiCol_CheckMark]             = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
	colors[ImGuiCol_MenuBarBg]             = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_SliderGrab]            = ImVec4(0.89f, 0.49f, 0.02f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.89f, 0.57f, 0.19f, 1.00f);
	colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.89f, 0.49f, 0.02f, 0.78f);
	colors[ImGuiCol_SeparatorActive]       = ImVec4(0.89f, 0.49f, 0.02f, 1.00f);
	colors[ImGuiCol_TabHovered]            = ImVec4(0.20f, 0.20f, 0.20f, 0.81f);
	colors[ImGuiCol_Tab]                   = ImVec4(0.25f, 0.25f, 0.25f, 0.86f);
	colors[ImGuiCol_TabSelected]           = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_TabSelectedOverline]   = ImVec4(0.89f, 0.49f, 0.02f, 1.00f);
	colors[ImGuiCol_TabDimmed]             = ImVec4(0.18f, 0.18f, 0.18f, 0.97f);
	colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_DockingPreview]        = ImVec4(0.89f, 0.49f, 0.02f, 0.70f);
	colors[ImGuiCol_TextLink]              = ImVec4(0.89f, 0.49f, 0.02f, 1.00f);
	colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.17f, 0.17f, 0.17f, 0.86f);
	colors[ImGuiCol_NavCursor]             = ImVec4(0.89f, 0.49f, 0.02f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.89f, 0.49f, 0.02f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.17f, 0.17f, 0.17f, 0.86f);
	colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.17f, 0.17f, 0.17f, 0.86f);

	style.GrabMinSize   = 8.0f;
	style.ScrollbarSize = 16.0f;

	style.WindowRounding    = 4.0f;
	style.FrameRounding     = 4.0f;
	style.GrabRounding      = 4.0f;
	style.ScrollbarRounding = 8.0f;
	style.TabRounding       = 0.0f;

	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize  = 0.0f;

	style.ItemSpacing   = ImVec2(8, 8);
	style.FramePadding  = ImVec2(8, 8);
	style.WindowPadding = ImVec2(8, 8);

	style.SeparatorTextBorderSize = 2.0f;

	style.TabBarBorderSize = 2.0f;

	style.CellPadding = ImVec2(4, 4);
}

void ImGuiManager::StyleColorsLight() {
	ImGui::StyleColorsLight();
}

void ImGuiManager::PushStyleColorForDrag(const ImVec4& bg,
                                         const ImVec4& bgHovered,
                                         const ImVec4& bgActive) {
	ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
}

bool ImGuiManager::EditTransform(Transform& transform, const float& vSpeed) {
	bool isEditing = false;

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		isEditing |= ImGuiWidgets::DragVec3(
			"Position",
			transform.translate,
			Vec3::zero,
			vSpeed, "%.3f"
		);

		// 回転を取っておく
		Vec3 rotate = transform.rotate * Math::rad2Deg;
		if (ImGuiWidgets::DragVec3(
			"Rotation",
			transform.rotate,
			Vec3::zero,
			vSpeed,
			"%.3f")) {
			isEditing |= true;
			transform.rotate = rotate * Math::deg2Rad;
		}

		isEditing |= ImGuiWidgets::DragVec3(
			"Scale",
			transform.scale,
			Vec3::one,
			vSpeed,
			"%.3f"
		);
	}

	return isEditing;
}

bool ImGuiManager::EditTransform(TransformComponent& transform,
                                 const float&        vSpeed) {
	bool       isEditing  = false;
	Vec3       localPos   = transform.GetLocalPos();
	Quaternion localRot   = transform.GetLocalRot();
	Vec3       localScale = transform.GetLocalScale();

	if (ImGui::CollapsingHeader(
		("Transform##" + transform.GetOwner()->GetName()).c_str(),
		ImGuiTreeNodeFlags_DefaultOpen)) {
		// Position 編集
		if (ImGuiWidgets::DragVec3(
			"Position",
			localPos,
			Vec3::zero,
			vSpeed,
			"%.3f"
		)) {
			transform.SetLocalPos(localPos);
			isEditing = true;
		}

		// Rotation 編集
		Vec3 eulerDegrees = localRot.ToEulerDegrees();
		if (ImGuiWidgets::DragVec3(
				"Rotation",
				eulerDegrees,
				Vec3::zero,
				vSpeed * 10.0f,
				"%.3f")
		) {
			// 編集された Euler 角を Quaternion に変換
			localRot = Quaternion::EulerDegrees(eulerDegrees);
			transform.SetLocalRot(localRot);
			isEditing = true;
		}

		// Scale 編集
		if (ImGuiWidgets::DragVec3(
				"Scale",
				localScale,
				Vec3::one,
				vSpeed,
				"%.3f")
		) {
			transform.SetLocalScale(localScale);
			isEditing = true;
		}
	}
	return isEditing;
}

bool ImGuiManager::DragVec3(const std::string& name, Vec3&         v,
                            const float&       vSpeed, const char* format) {
	// 編集中かどうか
	bool isEditing = false;

	// XYZの3つ分
	constexpr int components = 3;

	// 幅を取得
	const float width = ImGui::GetCurrentContext()->Style.ItemInnerSpacing.x;

	constexpr ImVec4 xBg        = {0.72f, 0.11f, 0.11f, 0.75f};
	constexpr ImVec4 xBgHovered = {0.83f, 0.18f, 0.18f, 0.75f};
	constexpr ImVec4 xBgActive  = {0.96f, 0.26f, 0.21f, 0.75f};

	constexpr ImVec4 yBg        = {0.11f, 0.37f, 0.13f, 0.75f};
	constexpr ImVec4 yBgHovered = {0.22f, 0.56f, 0.24f, 0.75f};
	constexpr ImVec4 yBgActive  = {0.3f, 0.69f, 0.31f, 0.75f};

	constexpr ImVec4 zBg        = {0.05f, 0.28f, 0.63f, 0.75f};
	constexpr ImVec4 zBgHovered = {0.1f, 0.46f, 0.82f, 0.75f};
	constexpr ImVec4 zBgActive  = {0.13f, 0.59f, 0.95f, 0.75f};

	// 幅を決定
	ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

	/* --- 座標 --- */
	// 色を送る
	ImGui::PushID("X");
	PushStyleColorForDrag(xBg, xBgHovered, xBgActive);
	isEditing |= ImGui::DragFloat(("##X" + name).c_str(), &v.x, vSpeed, 0.0f,
	                              0.0f, format);
	ImGui::PopStyleColor(components);
	ImGui::PopID();
	ImGui::SameLine(0, width);

	ImGui::PushID("Y");
	PushStyleColorForDrag(yBg, yBgHovered, yBgActive);
	isEditing |= ImGui::DragFloat(("##Y" + name).c_str(), &v.y, vSpeed, 0.0f,
	                              0.0f, format);
	ImGui::PopStyleColor(components);
	ImGui::PopID();
	ImGui::SameLine(0, width);

	ImGui::PushID("Z");
	PushStyleColorForDrag(zBg, zBgHovered, zBgActive);
	isEditing |= ImGui::DragFloat(("##Z" + name).c_str(), &v.z, vSpeed, 0.0f,
	                              0.0f, format);
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
	ImDrawList*   drawList,
	const ImVec2& pos,
	const char*   text,
	const ImVec4  textColor,
	const ImVec4  outlineColor,
	const float   outlineSize
) {
	// クライアント領域の左上座標を取得
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 clientPos = ImVec2(windowPos.x + pos.x, windowPos.y + pos.y);

	ImU32 outlineCol = ImGui::ColorConvertFloat4ToU32(outlineColor);
	ImU32 textCol    = ImGui::ColorConvertFloat4ToU32(textColor);

	drawList->AddText(ImVec2(clientPos.x - outlineSize, clientPos.y),
	                  outlineCol, text);
	drawList->AddText(ImVec2(clientPos.x + outlineSize, clientPos.y),
	                  outlineCol, text);
	drawList->AddText(ImVec2(clientPos.x, clientPos.y - outlineSize),
	                  outlineCol, text);
	drawList->AddText(ImVec2(clientPos.x, clientPos.y + outlineSize),
	                  outlineCol, text);
	drawList->AddText(
		ImVec2(clientPos.x - outlineSize, clientPos.y - outlineSize),
		outlineCol, text);
	drawList->AddText(
		ImVec2(clientPos.x + outlineSize, clientPos.y - outlineSize),
		outlineCol, text);
	drawList->AddText(
		ImVec2(clientPos.x - outlineSize, clientPos.y + outlineSize),
		outlineCol, text);
	drawList->AddText(
		ImVec2(clientPos.x + outlineSize, clientPos.y + outlineSize),
		outlineCol, text);
	drawList->AddText(clientPos, textCol, text);
}

bool ImGuiManager::IconButton(const char*   icon, const char* label,
                              const ImVec2& size) {
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
	float iconY = buttonPos.y + (buttonSize.y - (iconSize.y + labelSize.y +
		spacing)) * 0.5f;
	ImGui::SetCursorPos(
		ImVec2(
			buttonPos.x + (buttonSize.x - iconSize.x) * 0.5f,
			iconY
		)
	);
	ImGui::Text("%s", icon);

	// ラベルを小さいフォントで中央揃えで描画
	ImGui::SetCursorPos(
		ImVec2(
			buttonPos.x + (buttonSize.x - labelSize.x * 0.8f) * 0.5f,
			iconY + iconSize.y + spacing
		)
	);

	float defaultFontSize   = ImGui::GetFont()->Scale;
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
