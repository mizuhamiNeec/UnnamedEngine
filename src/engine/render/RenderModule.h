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

	/// @brief RenderModuleは、RendererとRHIの初期化・終了順序をmodule境界として集約します
	class RenderModule {
	public:
		RenderModule(AssetManager& assetManager, Rhi::IRhiDevice& rhiDevice);
		~RenderModule();

		/// @brief RenderDeviceとRendererを初期化します。
		/// @param console Rendererが使用するConsoleSystem。
		/// @param validationPolicy Renderer起動アセットの検証方針。
		/// @return 初期化に成功した場合true。
		[[nodiscard]] bool Init(
			ConsoleSystem* console,
			const RenderStartupOptions& startupOptions
		);
		/// @brief 起動シーンから到達可能なRenderer Pipelineを検証します。
		/// @return 検証に成功した場合true。
		[[nodiscard]] bool ValidateStartupResources() const;
		/// @brief Renderer/RenderDevice を明示的な順序で終了します。
		void Shutdown();
		void Tick(const RenderFrameInputs& inputs) const;

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
