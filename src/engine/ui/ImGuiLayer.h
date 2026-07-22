#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <unordered_map>
#include <vector>

#include <imgui.h>

struct ImGui_ImplDX12_InitInfo;

namespace Unnamed {
	namespace Rhi {
		class D3D12Device;
	}

	namespace Render {
		class RenderPassContext;
	}

	class ImGuiLayer {
	public:
		ImGuiLayer(
			HWND              hwnd,
			Rhi::D3D12Device& device,
			uint32_t          framesInFlight,
			DXGI_FORMAT       rtvFormat
		);
		~ImGuiLayer();

		ImGuiLayer(const ImGuiLayer&)            = delete;
		ImGuiLayer& operator=(const ImGuiLayer&) = delete;

		void BeginFrame(uint32_t frameIndex) const;
		void EndFrame() const;

		void RenderMainDrawData(
			const Render::RenderPassContext& passContext
		) const;
		void RenderPlatformWindows() const;

		/// @brief 指定した RenderGraph テクスチャ用の ImGui SRV を取得します。
		/// @details このフレームで返した textureId は ConsumeSampledTextureIds() で回収し、
		///          ImGui pass の読み取り依存として RenderGraph へ渡す必要があります。
		ImTextureID GetOrCreateTextureId(
			uint32_t                    textureId,
			uint64_t                    revision,
			D3D12_CPU_DESCRIPTOR_HANDLE sourceSrv
		);

		/// @brief 現フレームの ImGui 描画データがサンプリングするテクスチャ ID を回収します。
		/// @details BeginFrame() で収集範囲を初期化し、呼び出し後は次フレームまで空になります。
		[[nodiscard]] std::vector<uint32_t> ConsumeSampledTextureIds();

		[[nodiscard]] ID3D12DescriptorHeap* GetDescriptorHeap() const;

	private:
		struct FrameTextureSlot {
			uint32_t                    slot            = UINT32_MAX;
			uint64_t                    revision        = UINT64_MAX;
			uint64_t                    pendingRevision = UINT64_MAX;
			D3D12_CPU_DESCRIPTOR_HANDLE pendingSource   = {};
			bool                        hasPendingCopy  = false;
		};

		void FlushPendingTextureCopies() const;
		[[nodiscard]] uint32_t AllocateSlot();
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleAt(
			uint32_t slot
		) const;
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleAt(
			uint32_t slot
		) const;

		Rhi::D3D12Device& mDevice;

		uint32_t mDescriptorSize = 0;
		uint32_t mCapacity       = 0;
		uint32_t mFramesInFlight = 1;
		uint32_t mFrameIndex     = 0;

		struct TextureSlots {
			std::vector<FrameTextureSlot> frameSlots;
		};

		std::unordered_map<uint64_t, TextureSlots> mTextureSlotsByKey;
		std::vector<uint32_t> mFrameSampledTextureIds;
	};
}

#endif
