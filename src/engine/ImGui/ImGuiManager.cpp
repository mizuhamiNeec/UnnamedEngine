#ifdef _DEBUG
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <engine/Components/Transform/SceneComponent.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiManager.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/renderer/SrvManager.h>
#include <engine/Window/WindowManager.h>
#include <engine/Window/WindowsUtils.h>

ImGuiManager::ImGuiManager(D3D12*      renderer,
                           SrvManager* srvManager) :
	mRenderer(renderer),
	mSrvManager(srvManager) {
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
	imFontConfig.OversampleH      = 4;
	imFontConfig.OversampleV      = 4;
	imFontConfig.PixelSnapH       = true;
	imFontConfig.SizePixels       = 128.0f;
	imFontConfig.GlyphMinAdvanceX = 2.0f;

	// Ascii
	io.Fonts->AddFontFromFileTTF(
		R"(.\content\core\fonts\JetBrainsMono.ttf)", 16.0f, &imFontConfig,
		io.Fonts->GetGlyphRangesDefault()
	);
	imFontConfig.MergeMode = true;

	// 日本語フォールバック
	io.Fonts->AddFontFromFileTTF(
		R"(.\content\core\fonts\NotoSansJP.ttf)", 16.0f, &imFontConfig,
		GetGlyphRangesJapanese()
	);

	// ??? 何故かベースラインがずれるので補正
	imFontConfig.GlyphOffset = ImVec2(0.0f, 5.0f);

	static constexpr ImWchar iconRanges[] = {0xe003, 0xf8ff, 0};
	io.Fonts->AddFontFromFileTTF(
		R"(.\content\core\fonts\MaterialSymbolsRounded_Filled_28pt-Regular.ttf)",
		24.0f, &imFontConfig, iconRanges
	);

	// テーマの設定
	if (WindowsUtils::IsAppDarkTheme()) {
		StyleColorsDark();
	} else {
		StyleColorsLight();
	}

	ImGui_ImplWin32_Init(OldWindowManager::GetMainWindow()->GetWindowHandle());

	// ImGuiの初期化
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device                  = mRenderer->GetDevice();
	init_info.NumFramesInFlight       = kFrameBufferCount;
	init_info.RTVFormat               = DXGI_FORMAT_R8G8B8A8_UNORM;
	init_info.CommandQueue            = mRenderer->GetCommandQueue();
	init_info.SrvDescriptorHeap       = mSrvManager->GetDescriptorHeap();
	init_info.UserData                = this;

	auto allocFn =
		[](ImGui_ImplDX12_InitInfo*     info,
		   D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
		   D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle) {
		const auto mgr   = static_cast<ImGuiManager*>(info->UserData);
		const auto index = mgr->GetSrvManager()->AllocateForTexture2D();
		*cpuHandle       = mgr->GetSrvManager()->GetCPUDescriptorHandle(index);
		*gpuHandle       = mgr->GetSrvManager()->GetGPUDescriptorHandle(index);
	};

	auto freeFn =
		[](ImGui_ImplDX12_InitInfo*    info,
		   D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		   D3D12_GPU_DESCRIPTOR_HANDLE) {
		const auto mgr = static_cast<ImGuiManager*>(info->UserData);
		auto cpuIndex = mgr->GetSrvManager()->GetIndexFromCPUHandle(cpuHandle);
		mgr->GetSrvManager()->DeallocateTexture2D(cpuIndex);
	};

	init_info.SrvDescriptorAllocFn = allocFn;
	init_info.SrvDescriptorFreeFn  = freeFn;


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
	ID3D12DescriptorHeap* imGuiHeap = mSrvManager->GetDescriptorHeap();
	mRenderer->GetCommandList()->SetDescriptorHeaps(1, &imGuiHeap);

	// メインウィンドウのImGuiコンテンツを描画
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),
	                              mRenderer->GetCommandList());

	// マルチビューポートの更新前に、すべてのコマンドを確実にフラッシュする
	mRenderer->GetCommandList()->Close();
	ID3D12CommandList* cmdLists[] = {mRenderer->GetCommandList()};
	mRenderer->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);

	mRenderer->WaitPreviousFrame();

	// マルチビューポート用のレンダリング（新しいコマンドリストが内部で作成される）
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

	// 新しいコマンドリストを作成して作業を続行
	mRenderer->ResetCommandList();

	ImGui::EndFrame();
}

void ImGuiManager::Shutdown() {
	ImGui::DestroyPlatformWindows();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	mSrvHeap.Reset();
}

void ImGuiManager::StyleColorsDark() {
	ImGuiStyle& style  = ImGui::GetStyle();
	ImVec4*     colors = style.Colors;

	colors[ImGuiCol_WindowBg]              = ImVec4(0.14f, 0.14f, 0.14f, 0.94f);
	colors[ImGuiCol_Border]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
	colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	colors[ImGuiCol_TitleBg]               = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_TitleBgActive]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_ButtonHovered]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_ButtonActive]          = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_Header]                = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
	colors[ImGuiCol_HeaderHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
	colors[ImGuiCol_HeaderActive]          = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
	colors[ImGuiCol_ResizeGrip]            = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
	colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
	colors[ImGuiCol_Text]                  = ImVec4(0.71f, 0.71f, 0.71f, 1.00f);
	colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_CheckMark]             = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
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

	// 角丸
	style.WindowRounding    = 4.0f;
	style.FrameRounding     = 4.0f;
	style.GrabRounding      = 4.0f;
	style.ScrollbarRounding = 8.0f;
	style.TabRounding       = 4.0f;

	// ボーダー
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize  = 0.0f;
	style.TabBarBorderSize = 2.0f;

	// パディング
	style.FramePadding  = ImVec2(6, 6);
	style.WindowPadding = ImVec2(8, 8);

	// アイテムの間隔
	style.ItemSpacing = ImVec2(8, 8);

	// ウィンドウタイトルの配置
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

	style.SeparatorTextBorderSize = 2.0f;

	// アクティブなタブのオーバーラインA
	style.TabBarOverlineSize = 1.5f;

	style.CellPadding = ImVec2(4, 4);
}

void ImGuiManager::Recreate() const {
	// 1. ImGuiのDX12/Win32バックエンドを完全にシャットダウン
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();

	// 2. Win32バックエンドを再初期化（ウィンドウハンドルを渡す）
	ImGui_ImplWin32_Init(OldWindowManager::GetMainWindow()->GetWindowHandle());

	// 3. DX12バックエンドを再初期化（ディスクリプタヒープ等を渡す）
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device                  = mRenderer->GetDevice();
	init_info.NumFramesInFlight       = kFrameBufferCount;
	init_info.RTVFormat               = DXGI_FORMAT_R8G8B8A8_UNORM;
	init_info.SrvDescriptorHeap       = mSrvManager->GetDescriptorHeap();
	init_info.CommandQueue            = mRenderer->GetCommandQueue();

	ImGui_ImplDX12_Init(&init_info);

	// 4. デバイスオブジェクト再作成
	ImGui_ImplDX12_CreateDeviceObjects();
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

bool ImGuiManager::EditTransform(SceneComponent& transform,
                                 const float&    vSpeed) {
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

const ImWchar* ImGuiManager::GetGlyphRangesJapanese() {
	// 2999 ideograms code points for Japanese
	// - 2136 Joyo (meaning "for regular use" or "for common use") Kanji code points
	// - 863 Jinmeiyo (meaning "for personal name") Kanji code points
	// - Sourced from official information provided by the government agencies of Japan:
	//   - List of Joyo Kanji by the Agency for Cultural Affairs
	//     - https://www.bunka.go.jp/kokugo_nihongo/sisaku/joho/joho/kijun/naikaku/kanji/
	//   - List of Jinmeiyo Kanji by the Ministry of Justice
	//     - http://www.moj.go.jp/MINJI/minji86.html
	//   - Available under the terms of the Creative Commons Attribution 4.0 International (CC BY 4.0).
	//     - https://creativecommons.org/licenses/by/4.0/legalcode
	// - You can generate this code by the script at:
	//   - https://github.com/vaiorabbit/everyday_use_kanji
	// - References:
	//   - List of Joyo Kanji
	//     - (Wikipedia) https://en.wikipedia.org/wiki/List_of_j%C5%8Dy%C5%8D_kanji
	//   - List of Jinmeiyo Kanji
	//     - (Wikipedia) https://en.wikipedia.org/wiki/Jinmeiy%C5%8D_kanji
	// - Missing 1 Joyo Kanji: U+20B9F (Kun'yomi: Shikaru, On'yomi: Shitsu,shichi), see https://github.com/ocornut/imgui/pull/3627 for details.
	// You can use ImFontGlyphRangesBuilder to create your own ranges derived from this, by merging existing ranges or adding new characters.
	// (Stored as accumulative offsets from the initial unicode codepoint 0x4E00. This encoding is designed to helps us compact the source code size.)
	static const short accumulative_offsets_from_0x4E00[] =
	{
		0, 1, 2, 4, 1, 1, 1, 1, 2, 1, 3, 3, 2, 2, 1, 5, 3, 5, 7, 5, 6, 1, 2, 1,
		7, 2, 6, 3, 1, 8, 1, 1, 4, 1, 1, 18, 2, 11, 2, 6, 2, 1, 2, 1, 5, 1, 2,
		1, 3, 1, 2, 1, 2, 3, 3, 1, 1, 2, 3, 1, 1, 1, 12, 7, 9, 1, 4, 5, 1,
		1, 2, 1, 10, 1, 1, 9, 2, 2, 4, 5, 6, 9, 3, 1, 1, 1, 1, 9, 3, 18, 5, 2,
		2, 2, 2, 1, 6, 3, 7, 1, 1, 1, 1, 2, 2, 4, 2, 1, 23, 2, 10, 4, 3, 5, 2,
		4, 10, 2, 4, 13, 1, 6, 1, 9, 3, 1, 1, 6, 6, 7, 6, 3, 1, 2, 11, 3,
		2, 2, 3, 2, 15, 2, 2, 5, 4, 3, 6, 4, 1, 2, 5, 2, 12, 16, 6, 13, 9, 13,
		2, 1, 1, 7, 16, 4, 7, 1, 19, 1, 5, 1, 2, 2, 7, 7, 8, 2, 6, 5, 4, 9, 18,
		7, 4, 5, 9, 13, 11, 8, 15, 2, 1, 1, 1, 2, 1, 2, 2, 1, 2, 2, 8,
		2, 9, 3, 3, 1, 1, 4, 4, 1, 1, 1, 4, 9, 1, 4, 3, 5, 5, 2, 7, 5, 3, 4, 8,
		2, 1, 13, 2, 3, 3, 1, 14, 1, 1, 4, 5, 1, 3, 6, 1, 5, 2, 1, 1, 3, 3, 3,
		3, 1, 1, 2, 7, 6, 6, 7, 1, 4, 7, 6, 1, 1, 1, 1, 1, 12, 3, 3, 9, 5,
		2, 6, 1, 5, 6, 1, 2, 3, 18, 2, 4, 14, 4, 1, 3, 6, 1, 1, 6, 3, 5, 5, 3,
		2, 2, 2, 2, 12, 3, 1, 4, 2, 3, 2, 3, 11, 1, 7, 4, 1, 2, 1, 3, 17, 1, 9,
		1, 24, 1, 1, 4, 2, 2, 4, 1, 2, 7, 1, 1, 1, 3, 1, 2, 2, 4, 15, 1,
		1, 2, 1, 1, 2, 1, 5, 2, 5, 20, 2, 5, 9, 1, 10, 8, 7, 6, 1, 1, 1, 1, 1,
		1, 6, 2, 1, 2, 8, 1, 1, 1, 1, 5, 1, 1, 3, 1, 1, 1, 1, 3, 1, 1, 12, 4, 1,
		3, 1, 1, 1, 1, 1, 10, 3, 1, 7, 5, 13, 1, 2, 3, 4, 6, 1, 1, 30,
		2, 9, 9, 1, 15, 38, 11, 3, 1, 8, 24, 7, 1, 9, 8, 10, 2, 1, 9, 31, 2, 13,
		6, 2, 9, 4, 49, 5, 2, 15, 2, 1, 10, 2, 1, 1, 1, 2, 2, 6, 15, 30, 35, 3,
		14, 18, 8, 1, 16, 10, 28, 12, 19, 45, 38, 1, 3, 2, 3,
		13, 2, 1, 7, 3, 6, 5, 3, 4, 3, 1, 5, 7, 8, 1, 5, 3, 18, 5, 3, 6, 1, 21,
		4, 24, 9, 24, 40, 3, 14, 3, 21, 3, 2, 1, 2, 4, 2, 3, 1, 15, 15, 6, 5, 1,
		1, 3, 1, 5, 6, 1, 9, 7, 3, 3, 2, 1, 4, 3, 8, 21, 5, 16, 4,
		5, 2, 10, 11, 11, 3, 6, 3, 2, 9, 3, 6, 13, 1, 2, 1, 1, 1, 1, 11, 12, 6,
		6, 1, 4, 2, 6, 5, 2, 1, 1, 3, 3, 6, 13, 3, 1, 1, 5, 1, 2, 3, 3, 14, 2,
		1, 2, 2, 2, 5, 1, 9, 5, 1, 1, 6, 12, 3, 12, 3, 4, 13, 2, 14,
		2, 8, 1, 17, 5, 1, 16, 4, 2, 2, 21, 8, 9, 6, 23, 20, 12, 25, 19, 9, 38,
		8, 3, 21, 40, 25, 33, 13, 4, 3, 1, 4, 1, 2, 4, 1, 2, 5, 26, 2, 1, 1, 2,
		1, 3, 6, 2, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1, 1, 9, 2, 3, 1, 1,
		1, 3, 6, 3, 2, 1, 1, 6, 6, 1, 8, 2, 2, 2, 1, 4, 1, 2, 3, 2, 7, 3, 2, 4,
		1, 2, 1, 2, 2, 1, 1, 1, 1, 1, 3, 1, 2, 5, 4, 10, 9, 4, 9, 1, 1, 1, 1, 1,
		1, 5, 3, 2, 1, 6, 4, 9, 6, 1, 10, 2, 31, 17, 8, 3, 7, 5, 40, 1,
		7, 7, 1, 6, 5, 2, 10, 7, 8, 4, 15, 39, 25, 6, 28, 47, 18, 10, 7, 1, 3,
		1, 1, 2, 1, 1, 1, 3, 3, 3, 1, 1, 1, 3, 4, 2, 1, 4, 1, 3, 6, 10, 7, 8, 6,
		2, 2, 1, 3, 3, 2, 5, 8, 7, 9, 12, 2, 15, 1, 1, 4, 1, 2, 1, 1,
		1, 3, 2, 1, 3, 3, 5, 6, 2, 3, 2, 10, 1, 4, 2, 8, 1, 1, 1, 11, 6, 1, 21,
		4, 16, 3, 1, 3, 1, 4, 2, 3, 6, 5, 1, 3, 1, 1, 3, 3, 4, 6, 1, 1, 10, 4,
		2, 7, 10, 4, 7, 4, 2, 9, 4, 3, 1, 1, 1, 4, 1, 8, 3, 4, 1, 3, 1,
		6, 1, 4, 2, 1, 4, 7, 2, 1, 8, 1, 4, 5, 1, 1, 2, 2, 4, 6, 2, 7, 1, 10, 1,
		1, 3, 4, 11, 10, 8, 21, 4, 6, 1, 3, 5, 2, 1, 2, 28, 5, 5, 2, 3, 13, 1,
		2, 3, 1, 4, 2, 1, 5, 20, 3, 8, 11, 1, 3, 3, 3, 1, 8, 10, 9, 2,
		10, 9, 2, 3, 1, 1, 2, 4, 1, 8, 3, 6, 1, 7, 8, 6, 11, 1, 4, 29, 8, 4, 3,
		1, 2, 7, 13, 1, 4, 1, 6, 2, 6, 12, 12, 2, 20, 3, 2, 3, 6, 4, 8, 9, 2, 7,
		34, 5, 1, 18, 6, 1, 1, 4, 4, 5, 7, 9, 1, 2, 2, 4, 3, 4, 1, 7,
		2, 2, 2, 6, 2, 3, 25, 5, 3, 6, 1, 4, 6, 7, 4, 2, 1, 4, 2, 13, 6, 4, 4,
		3, 1, 5, 3, 4, 4, 3, 2, 1, 1, 4, 1, 2, 1, 1, 3, 1, 11, 1, 6, 3, 1, 7, 3,
		6, 2, 8, 8, 6, 9, 3, 4, 11, 3, 2, 10, 12, 2, 5, 11, 1, 6, 4, 5,
		3, 1, 8, 5, 4, 6, 6, 3, 5, 1, 1, 3, 2, 1, 2, 2, 6, 17, 12, 1, 10, 1, 6,
		12, 1, 6, 6, 19, 9, 6, 16, 1, 13, 4, 4, 15, 7, 17, 6, 11, 9, 15, 12, 6,
		7, 2, 1, 2, 2, 15, 9, 3, 21, 4, 6, 49, 18, 7, 3, 2, 3, 1,
		6, 8, 2, 2, 6, 2, 9, 1, 3, 6, 4, 4, 1, 2, 16, 2, 5, 2, 1, 6, 2, 3, 5, 3,
		1, 2, 5, 1, 2, 1, 9, 3, 1, 8, 6, 4, 8, 11, 3, 1, 1, 1, 1, 3, 1, 13, 8,
		4, 1, 3, 2, 2, 1, 4, 1, 11, 1, 5, 2, 1, 5, 2, 5, 8, 6, 1, 1, 7,
		4, 3, 8, 3, 2, 7, 2, 1, 5, 1, 5, 2, 4, 7, 6, 2, 8, 5, 1, 11, 4, 5, 3, 6,
		18, 1, 2, 13, 3, 3, 1, 21, 1, 1, 4, 1, 4, 1, 1, 1, 8, 1, 2, 2, 7, 1, 2,
		4, 2, 2, 9, 2, 1, 1, 1, 4, 3, 6, 3, 12, 5, 1, 1, 1, 5, 6, 3, 2,
		4, 8, 2, 2, 4, 2, 7, 1, 8, 9, 5, 2, 3, 2, 1, 3, 2, 13, 7, 14, 6, 5, 1,
		1, 2, 1, 4, 2, 23, 2, 1, 1, 6, 3, 1, 4, 1, 15, 3, 1, 7, 3, 9, 14, 1, 3,
		1, 4, 1, 1, 5, 8, 1, 3, 8, 3, 8, 15, 11, 4, 14, 4, 4, 2, 5, 5,
		1, 7, 1, 6, 14, 7, 7, 8, 5, 15, 4, 8, 6, 5, 6, 2, 1, 13, 1, 20, 15, 11,
		9, 2, 5, 6, 2, 11, 2, 6, 2, 5, 1, 5, 8, 4, 13, 19, 25, 4, 1, 1, 11, 1,
		34, 2, 5, 9, 14, 6, 2, 2, 6, 1, 1, 14, 1, 3, 14, 13, 1, 6,
		12, 21, 14, 14, 6, 32, 17, 8, 32, 9, 28, 1, 2, 4, 11, 8, 3, 1, 14, 2, 5,
		15, 1, 1, 1, 1, 3, 6, 4, 1, 3, 4, 11, 3, 1, 1, 11, 30, 1, 5, 1, 4, 1, 5,
		8, 1, 1, 3, 2, 4, 3, 17, 35, 2, 6, 12, 17, 3, 1, 6, 2,
		1, 1, 12, 2, 7, 3, 3, 2, 1, 16, 2, 8, 3, 6, 5, 4, 7, 3, 3, 8, 1, 9, 8,
		5, 1, 2, 1, 3, 2, 8, 1, 2, 9, 12, 1, 1, 2, 3, 8, 3, 24, 12, 4, 3, 7, 5,
		8, 3, 3, 3, 3, 3, 3, 1, 23, 10, 3, 1, 2, 2, 6, 3, 1, 16, 1, 16,
		22, 3, 10, 4, 11, 6, 9, 7, 7, 3, 6, 2, 2, 2, 4, 10, 2, 1, 1, 2, 8, 7, 1,
		6, 4, 1, 3, 3, 3, 5, 10, 12, 12, 2, 3, 12, 8, 15, 1, 1, 16, 6, 6, 1, 5,
		9, 11, 4, 11, 4, 2, 6, 12, 1, 17, 5, 13, 1, 4, 9, 5, 1, 11,
		2, 1, 8, 1, 5, 7, 28, 8, 3, 5, 10, 2, 17, 3, 38, 22, 1, 2, 18, 12, 10,
		4, 38, 18, 1, 4, 44, 19, 4, 1, 8, 4, 1, 12, 1, 4, 31, 12, 1, 14, 7, 75,
		7, 5, 10, 6, 6, 13, 3, 2, 11, 11, 3, 2, 5, 28, 15, 6, 18,
		18, 5, 6, 4, 3, 16, 1, 7, 18, 7, 36, 3, 5, 3, 1, 7, 1, 9, 1, 10, 7, 2,
		4, 2, 6, 2, 9, 7, 4, 3, 32, 12, 3, 7, 10, 2, 23, 16, 3, 1, 12, 3, 31, 4,
		11, 1, 3, 8, 9, 5, 1, 30, 15, 6, 12, 3, 2, 2, 11, 19, 9,
		14, 2, 6, 2, 3, 19, 13, 17, 5, 3, 3, 25, 3, 14, 1, 1, 1, 36, 1, 3, 2,
		19, 3, 13, 36, 9, 13, 31, 6, 4, 16, 34, 2, 5, 4, 2, 3, 3, 5, 1, 1, 1, 4,
		3, 1, 17, 3, 2, 3, 5, 3, 1, 3, 2, 3, 5, 6, 3, 12, 11, 1, 3,
		1, 2, 26, 7, 12, 7, 2, 14, 3, 3, 7, 7, 11, 25, 25, 28, 16, 4, 36, 1, 2,
		1, 6, 2, 1, 9, 3, 27, 17, 4, 3, 4, 13, 4, 1, 3, 2, 2, 1, 10, 4, 2, 4, 6,
		3, 8, 2, 1, 18, 1, 1, 24, 2, 2, 4, 33, 2, 3, 63, 7, 1, 6,
		40, 7, 3, 4, 4, 2, 4, 15, 18, 1, 16, 1, 1, 11, 2, 41, 14, 1, 3, 18, 13,
		3, 2, 4, 16, 2, 17, 7, 15, 24, 7, 18, 13, 44, 2, 2, 3, 6, 1, 1, 7, 5, 1,
		7, 1, 4, 3, 3, 5, 10, 8, 2, 3, 1, 8, 1, 1, 27, 4, 2, 1,
		12, 1, 2, 1, 10, 6, 1, 6, 7, 5, 2, 3, 7, 11, 5, 11, 3, 6, 6, 2, 3, 15,
		4, 9, 1, 1, 2, 1, 2, 11, 2, 8, 12, 8, 5, 4, 2, 3, 1, 5, 2, 2, 1, 14, 1,
		12, 11, 4, 1, 11, 17, 17, 4, 3, 2, 5, 5, 7, 3, 1, 5, 9, 9, 8,
		2, 5, 6, 6, 13, 13, 2, 1, 2, 6, 1, 2, 2, 49, 4, 9, 1, 2, 10, 16, 7, 8,
		4, 3, 2, 23, 4, 58, 3, 29, 1, 14, 19, 19, 11, 11, 2, 7, 5, 1, 3, 4, 6,
		2, 18, 5, 12, 12, 17, 17, 3, 3, 2, 4, 1, 6, 2, 3, 4, 3, 1,
		1, 1, 1, 5, 1, 1, 9, 1, 3, 1, 3, 6, 1, 8, 1, 1, 2, 6, 4, 14, 3, 1, 4,
		11, 4, 1, 3, 32, 1, 2, 4, 13, 4, 1, 2, 4, 2, 1, 3, 1, 11, 1, 4, 2, 1, 4,
		4, 6, 3, 5, 1, 6, 5, 7, 6, 3, 23, 3, 5, 3, 5, 3, 3, 13, 3, 9, 10,
		1, 12, 10, 2, 3, 18, 13, 7, 160, 52, 4, 2, 2, 3, 2, 14, 5, 4, 12, 4, 6,
		4, 1, 20, 4, 11, 6, 2, 12, 27, 1, 4, 1, 2, 2, 7, 4, 5, 2, 28, 3, 7, 25,
		8, 3, 19, 3, 6, 10, 2, 2, 1, 10, 2, 5, 4, 1, 3, 4, 1, 5,
		3, 2, 6, 9, 3, 6, 2, 16, 3, 3, 16, 4, 5, 5, 3, 2, 1, 2, 16, 15, 8, 2, 6,
		21, 2, 4, 1, 22, 5, 8, 1, 1, 21, 11, 2, 1, 11, 11, 19, 13, 12, 4, 2, 3,
		2, 3, 6, 1, 8, 11, 1, 4, 2, 9, 5, 2, 1, 11, 2, 9, 1, 1, 2,
		14, 31, 9, 3, 4, 21, 14, 4, 8, 1, 7, 2, 2, 2, 5, 1, 4, 20, 3, 3, 4, 10,
		1, 11, 9, 8, 2, 1, 4, 5, 14, 12, 14, 2, 17, 9, 6, 31, 4, 14, 1, 20, 13,
		26, 5, 2, 7, 3, 6, 13, 2, 4, 2, 19, 6, 2, 2, 18, 9, 3, 5,
		12, 12, 14, 4, 6, 2, 3, 6, 9, 5, 22, 4, 5, 25, 6, 4, 8, 5, 2, 6, 27, 2,
		35, 2, 16, 3, 7, 8, 8, 6, 6, 5, 9, 17, 2, 20, 6, 19, 2, 13, 3, 1, 1, 1,
		4, 17, 12, 2, 14, 7, 1, 4, 18, 12, 38, 33, 2, 10, 1, 1,
		2, 13, 14, 17, 11, 50, 6, 33, 20, 26, 74, 16, 23, 45, 50, 13, 38, 33, 6,
		6, 7, 4, 4, 2, 1, 3, 2, 5, 8, 7, 8, 9, 3, 11, 21, 9, 13, 1, 3, 10, 6, 7,
		1, 2, 2, 18, 5, 5, 1, 9, 9, 2, 68, 9, 19, 13, 2, 5,
		1, 4, 4, 7, 4, 13, 3, 9, 10, 21, 17, 3, 26, 2, 1, 5, 2, 4, 5, 4, 1, 7,
		4, 7, 3, 4, 2, 1, 6, 1, 1, 20, 4, 1, 9, 2, 2, 1, 3, 3, 2, 3, 2, 1, 1, 1,
		20, 2, 3, 1, 6, 2, 3, 6, 2, 4, 8, 1, 3, 2, 10, 3, 5, 3, 4, 4,
		3, 4, 16, 1, 6, 1, 10, 2, 4, 2, 1, 1, 2, 10, 11, 2, 2, 3, 1, 24, 31, 4,
		10, 10, 2, 5, 12, 16, 164, 15, 4, 16, 7, 9, 15, 19, 17, 1, 2, 1, 1, 5,
		1, 1, 1, 1, 1, 3, 1, 4, 3, 1, 3, 1, 3, 1, 2, 1, 1, 3, 3, 7,
		2, 8, 1, 2, 2, 2, 1, 3, 4, 3, 7, 8, 12, 92, 2, 10, 3, 1, 3, 14, 5, 25,
		16, 42, 4, 7, 7, 4, 2, 21, 5, 27, 26, 27, 21, 25, 30, 31, 2, 1, 5, 13,
		3, 22, 5, 6, 6, 11, 9, 12, 1, 5, 9, 7, 5, 5, 22, 60, 3, 5,
		13, 1, 1, 8, 1, 1, 3, 3, 2, 1, 9, 3, 3, 18, 4, 1, 2, 3, 7, 6, 3, 1, 2,
		3, 9, 1, 3, 1, 3, 2, 1, 3, 1, 1, 1, 2, 1, 11, 3, 1, 6, 9, 1, 3, 2, 3, 1,
		2, 1, 5, 1, 1, 4, 3, 4, 1, 2, 2, 4, 4, 1, 7, 2, 1, 2, 2, 3, 5, 13,
		18, 3, 4, 14, 9, 9, 4, 16, 3, 7, 5, 8, 2, 6, 48, 28, 3, 1, 1, 4, 2, 14,
		8, 2, 9, 2, 1, 15, 2, 4, 3, 2, 10, 16, 12, 8, 7, 1, 1, 3, 1, 1, 1, 2, 7,
		4, 1, 6, 4, 38, 39, 16, 23, 7, 15, 15, 3, 2, 12, 7, 21,
		37, 27, 6, 5, 4, 8, 2, 10, 8, 8, 6, 5, 1, 2, 1, 3, 24, 1, 16, 17, 9, 23,
		10, 17, 6, 1, 51, 55, 44, 13, 294, 9, 3, 6, 2, 4, 2, 2, 15, 1, 1, 1, 13,
		21, 17, 68, 14, 8, 9, 4, 1, 4, 9, 3, 11, 7, 1, 1, 1,
		5, 6, 3, 2, 1, 1, 1, 2, 3, 8, 1, 2, 2, 4, 1, 5, 5, 2, 1, 4, 3, 7, 13, 4,
		1, 4, 1, 3, 1, 1, 1, 5, 5, 10, 1, 6, 1, 5, 2, 1, 5, 2, 4, 1, 4, 5, 7, 3,
		18, 2, 9, 11, 32, 4, 3, 3, 2, 4, 7, 11, 16, 9, 11, 8, 13, 38,
		32, 8, 4, 2, 1, 1, 2, 1, 2, 4, 4, 1, 1, 1, 4, 1, 21, 3, 11, 1, 16, 1, 1,
		6, 1, 3, 2, 4, 9, 8, 57, 7, 44, 1, 3, 3, 13, 3, 10, 1, 1, 7, 5, 2, 7,
		21, 47, 63, 3, 15, 4, 7, 1, 16, 1, 1, 2, 8, 2, 3, 42, 15, 4,
		1, 29, 7, 22, 10, 3, 78, 16, 12, 20, 18, 4, 67, 11, 5, 1, 3, 15, 6, 21,
		31, 32, 27, 18, 13, 71, 35, 5, 142, 4, 10, 1, 2, 50, 19, 33, 16, 35, 37,
		16, 19, 27, 7, 1, 133, 19, 1, 4, 8, 7, 20, 1, 4,
		4, 1, 10, 3, 1, 6, 1, 2, 51, 5, 40, 15, 24, 43, 22928, 11, 1, 13, 154,
		70, 3, 1, 1, 7, 4, 10, 1, 2, 1, 1, 2, 1, 2, 1, 2, 2, 1, 1, 2, 1, 1, 1,
		1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
		3, 2, 1, 1, 1, 1, 2, 1, 1,
	};
	static ImWchar base_ranges[] = // not zero-terminated
	{
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
		0x31F0, 0x31FF, // Katakana Phonetic Extensions
		0xFF00, 0xFFEF, // Half-width characters
		0xFFFD, 0xFFFD  // Invalid
	};
	static ImWchar full_ranges[IM_ARRAYSIZE(base_ranges) + IM_ARRAYSIZE(
		accumulative_offsets_from_0x4E00) * 2 + 1] = {0};
	return &full_ranges[0];
}
#endif
