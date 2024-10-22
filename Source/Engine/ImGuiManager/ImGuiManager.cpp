#include "ImGuiManager.h"

#ifdef _DEBUG

#include "../Renderer/D3D12.h"
#include "../Window/Window.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include "../Lib/Utils/ClientProperties.h"
#include "../Window/WindowsUtils.h"

void ImGuiManager::Init(const D3D12* renderer, const Window* window) {
	renderer_ = renderer;

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
	io.Fonts->AddFontFromFileTTF(R"(.\Resources\Fonts\JetBrainsMono.ttf)", 18.0f, &imFontConfig, io.Fonts->GetGlyphRangesDefault());
	imFontConfig.MergeMode = true;
	// 日本語フォールバック
	io.Fonts->AddFontFromFileTTF(R"(.\Resources\Fonts\NotoSansJP.ttf)", 18.0f, &imFontConfig, io.Fonts->GetGlyphRangesJapanese());

	// テーマの設定
	// TODO : 多分いらないけどランタイムで変わったらカッコいいよね!!
	if (WindowsUtils::IsAppDarkTheme()) {
		ImGui::StyleColorsDark();
	} else {
		ImGui::StyleColorsLight();
	}

	ImGui_ImplWin32_Init(window->GetWindowHandle());

	ImGui_ImplDX12_Init(
		renderer_->GetDevice(),
		kFrameBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		renderer_->GetSRVDescriptorHeap(),
		renderer_->GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		renderer_->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()
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


	ID3D12DescriptorHeap* imGuiHeap = renderer_->GetSRVDescriptorHeap();
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
#else
void ImGuiManager::Shutdown() {

}
#endif
