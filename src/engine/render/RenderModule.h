#pragma once
#include <cstdint>
#include <memory>
#include <string_view>

#include "Renderer.h"
#include "frame/RenderFrameInputs.h"

#include "core/math/Vec2.h"

namespace Unnamed {
	namespace Rhi {
		class IRhiDevice;
	}

	class AssetManager;
}

namespace Unnamed::Render {
	struct RenderFrameInputs;
	class RenderDevice;

	class RenderModule {
	public:
		RenderModule(AssetManager& assetManager, Rhi::IRhiDevice& rhiDevice);
		~RenderModule();

		void Init(ConsoleSystem* console);
		/// @brief Renderer/RenderDevice を明示的な順序で終了します。
		void Shutdown();
		void Tick(RenderFrameInputs& inputs) const;

		void OnResize(uint32_t width, uint32_t height) const;

		void SetUiCallbacks(
			Renderer::UiMainRenderCallback     mainRenderCallback,
			Renderer::UiPlatformRenderCallback platformRenderCallback
		) const;

		[[nodiscard]] SceneOutputView GetViewOutputView(
			std::string_view viewKey
		) const;
		[[nodiscard]] Vec2 GetViewOutputSize(std::string_view viewKey) const;

	private:
		AssetManager&    mAssetManager;
		Rhi::IRhiDevice& mRhiDevice;

		std::unique_ptr<RenderDevice> mRenderDevice;
		std::unique_ptr<Renderer>     mRenderer;
	};
}
