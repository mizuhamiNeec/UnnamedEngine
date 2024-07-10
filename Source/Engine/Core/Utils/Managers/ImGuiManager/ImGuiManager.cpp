#include "ImGuiManager.h"

#include <winrt/Windows.UI.ViewManagement.h>

#ifdef _DEBUG
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include "../../../Renderer/D3D12.h"
#include "../../ClientProperties.h"

using namespace winrt::Windows::UI::ViewManagement;

inline bool IsColorLight(const winrt::Windows::UI::Color& clr) {
	return 5 * clr.G + 2 * clr.R + clr.B > 8 * 128;
}

void ImGuiManager::Initialize(const D3D12* renderer, const Window* window) {
	renderer_ = renderer;

	colorTransitions.resize(ImGuiCol_COUNT);

	// ImGuiの初期化。詳細はさして重要ではないので解説はh省略する。
	// こういうもんである
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

	ImFontConfig imFontConfig;
	imFontConfig.OversampleH = 1;
	imFontConfig.OversampleV = 1;
	imFontConfig.PixelSnapH = true;
	imFontConfig.SizePixels = 18;

	// 日本語
	io.Fonts->AddFontFromFileTTF(R"(.\Resources\Fonts\JetBrainsMono.ttf)", 18.0f, &imFontConfig, io.Fonts->GetGlyphRangesJapanese());

	UISettings settings = UISettings();
	winrt::Windows::UI::Color foreground = settings.GetColorValue(UIColorType::Foreground);
	if (IsColorLight(foreground)) {
		ImGui::StyleColorsDark();
	} else {
		ImGui::StyleColorsLight();
	}

	ImGui_ImplWin32_Init(window->GetHWND());

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

	ID3D12DescriptorHeap* imGuiHeap = renderer_->GetSRVDescriptorHeap();
	renderer_->GetCommandList()->SetDescriptorHeaps(1, &imGuiHeap);

	//実際のcommandListのImGuiの描画コマンドを積む
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), renderer_->GetCommandList());

	// ImGuiのエンドフレーム
	ImGui::EndFrame();
}

void ImGuiManager::Shutdown() {
	// ImGuiの終了処理。詳細はさして重要ではないので解説は省略する。
	// こういうもんである。初期化と逆順に行う
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	srvHeap_.Reset();
}

#endif