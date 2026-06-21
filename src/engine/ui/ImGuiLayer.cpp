#ifdef _DEBUG
#include "ImGuiLayer.h"

#include <algorithm>

#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "engine/ImGui/ImGuiUtil.h"
#include "engine/platform/WindowsUtils.h"
#include "engine/render/rendergraph/RenderPassContext.h"
#include "engine/rhi/d3d12/D3D12Device.h"

namespace Unnamed {
	ImGuiLayer::ImGuiLayer(
		const HWND        hwnd,
		Rhi::D3D12Device& device,
		const uint32_t    framesInFlight,
		const DXGI_FORMAT rtvFormat
	) : mDevice(device) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io    = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

		ImGuiStyle* style     = &ImGui::GetStyle();
		style->WindowRounding = 8.0f;
		style->FrameRounding  = 4.0f;

		ImFontConfig fontCfg     = {};
		fontCfg.OversampleH      = 4;
		fontCfg.OversampleV      = 4;
		fontCfg.PixelSnapH       = true;
		fontCfg.SizePixels       = 128.0f;
		fontCfg.GlyphMinAdvanceX = 2.0f;

		io.Fonts->AddFontFromFileTTF(
			R"(.\content\core\fonts\JetBrainsMono.ttf)",
			16.0f,
			&fontCfg,
			io.Fonts->GetGlyphRangesDefault()
		);
		fontCfg.MergeMode = true;
		io.Fonts->AddFontFromFileTTF(
			R"(.\content\core\fonts\NotoSansJP.ttf)",
			16.0f,
			&fontCfg,
			io.Fonts->GetGlyphRangesJapanese()
		);

		fontCfg.GlyphOffset                    = ImVec2(0.0f, 2.0f);
		static constexpr ImWchar kIconRanges[] = {0xe003, 0xf8ff, 0};
		io.Fonts->AddFontFromFileTTF(
			R"(.\content\core\fonts\MaterialSymbolsRounded_Filled_28pt-Regular.ttf)",
			16.0f,
			&fontCfg,
			kIconRanges
		);

		if (WindowsUtils::IsAppDarkTheme()) {
			ImGuiUtil::StyleColorsDark();
		} else {
			ImGuiUtil::StyleColorsLight();
		}

		mCapacity       = device.GetImGuiSrvHeapCapacity();
		mFramesInFlight = std::max(1u, framesInFlight);
		mFrameIndex     = 0;
		mDescriptorSize = device.GetSrvUavDescriptorSize();

		ImGui_ImplWin32_Init(hwnd);

		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device                  = device.GetDevice();
		initInfo.CommandQueue            = device.GetGraphicsQueue();
		initInfo.NumFramesInFlight       = static_cast<int>(framesInFlight);
		initInfo.RTVFormat               = rtvFormat;
		initInfo.DSVFormat               = DXGI_FORMAT_UNKNOWN;
		initInfo.SrvDescriptorHeap       = device.GetSrvUavHeap();
		initInfo.UserData                = this;

		initInfo.SrvDescriptorAllocFn = [](
			ImGui_ImplDX12_InitInfo*     info,
			D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
			D3D12_GPU_DESCRIPTOR_HANDLE* outGpu
		) {
				auto*          layer = static_cast<ImGuiLayer*>(info->UserData);
				const uint32_t slot  = layer->AllocateSlot();
				*outCpu              = layer->CpuHandleAt(slot);
				*outGpu              = layer->GpuHandleAt(slot);
			};
		initInfo.SrvDescriptorFreeFn = [](
			ImGui_ImplDX12_InitInfo*,
			D3D12_CPU_DESCRIPTOR_HANDLE,
			D3D12_GPU_DESCRIPTOR_HANDLE
		) {
			};

		ImGui_ImplDX12_Init(&initInfo);
	}

	ImGuiLayer::~ImGuiLayer() {
		// TODO: ↓ あるとクラッシュする Why?
		//if (ImGui::GetCurrentContext()) { ImGui::DestroyPlatformWindows(); }
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::BeginFrame(const uint32_t frameIndex) const {
		auto& self       = const_cast<ImGuiLayer&>(*this);
		self.mFrameIndex = self.mFramesInFlight > 0 ?
			                   frameIndex % self.mFramesInFlight :
			                   0;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::EndFrame() const {
		ImGui::Render();
	}

	void ImGuiLayer::RenderMainDrawData(
		const Render::RenderPassContext& passContext
	) const {
		if (!ImGui::GetCurrentContext()) {
			return;
		}
		ImDrawData* drawData = ImGui::GetDrawData();
		if (!drawData || drawData->CmdListsCount <= 0) {
			return;
		}

		FlushPendingTextureCopies();

		ID3D12DescriptorHeap* heaps[] = {mDevice.GetSrvUavHeap()};
		passContext.GetCommandList()->SetDescriptorHeaps(1, heaps);
		ImGui_ImplDX12_RenderDrawData(drawData, passContext.GetCommandList());
	}

	void ImGuiLayer::RenderPlatformWindows() const {
		if (
			(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ==
			0
		) {
			return;
		}

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	ImTextureID ImGuiLayer::GetOrCreateTextureId(
		const uint64_t                    key,
		const uint64_t                    revision,
		const D3D12_CPU_DESCRIPTOR_HANDLE sourceSrv
	) {
		if (sourceSrv.ptr == 0) {
			return 0;
		}

		auto& textureSlots = mTextureSlotsByKey[key];
		auto& frameSlots   = textureSlots.frameSlots;
		if (frameSlots.empty()) {
			frameSlots.resize(mFramesInFlight);
		}

		const uint32_t frameSlotIndex = mFrameIndex % mFramesInFlight;
		auto&          frameSlot      = frameSlots[frameSlotIndex];
		if (frameSlot.slot == UINT32_MAX) {
			frameSlot.slot = AllocateSlot();
		}

		if (frameSlot.revision != revision) {
			frameSlot.pendingSource   = sourceSrv;
			frameSlot.pendingRevision = revision;
			frameSlot.hasPendingCopy  = true;
		}

		const D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(frameSlot.slot);
		return gpu.ptr;
	}

	ID3D12DescriptorHeap* ImGuiLayer::GetDescriptorHeap() const {
		return mDevice.GetSrvUavHeap();
	}

	void ImGuiLayer::FlushPendingTextureCopies() const {
		auto& self = const_cast<ImGuiLayer&>(*this);
		for (auto& [_, textureSlots] : self.mTextureSlotsByKey) {
			if (textureSlots.frameSlots.empty()) {
				continue;
			}

			auto& frameSlot = textureSlots.frameSlots[
				self.mFrameIndex % self.mFramesInFlight
			];
			if (!frameSlot.hasPendingCopy || frameSlot.pendingSource.ptr == 0 ||
			    frameSlot.slot == UINT32_MAX) {
				continue;
			}

			const D3D12_CPU_DESCRIPTOR_HANDLE dst = CpuHandleAt(frameSlot.slot);
			mDevice.GetDevice()->CopyDescriptorsSimple(
				1,
				dst,
				frameSlot.pendingSource,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
			frameSlot.revision       = frameSlot.pendingRevision;
			frameSlot.hasPendingCopy = false;
			frameSlot.pendingSource  = {};
		}
	}

	uint32_t ImGuiLayer::AllocateSlot() {
		return mDevice.AllocateImGuiSrvSlot();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ImGuiLayer::CpuHandleAt(
		const uint32_t slot
	) const {
		return mDevice.GetSrvUavCpuHandle(slot);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE ImGuiLayer::GpuHandleAt(
		const uint32_t slot
	) const {
		return mDevice.GetSrvUavGpuHandle(slot);
	}
}

#endif
