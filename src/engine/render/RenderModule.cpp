#include "RenderModule.h"

#include <utility>

#include "RenderDevice.h"
#include "Renderer.h"

namespace Unnamed::Render {
	RenderModule::RenderModule(
		AssetManager& assetManager, Rhi::IRhiDevice& rhiDevice
	) : mAssetManager(assetManager),
	    mRhiDevice(rhiDevice) {
	}

	RenderModule::~RenderModule() {
		Shutdown();
	}

	bool RenderModule::Init(
		ConsoleSystem* console,
		const RendererStartupValidationPolicy validationPolicy
	) {
		mRenderDevice = std::make_unique<RenderDevice>(
			mRhiDevice, mAssetManager
		);
		mRenderer = std::make_unique<Renderer>(console);
		return mRenderer->Init(*mRenderDevice, validationPolicy);
	}

	void RenderModule::Shutdown() {
		if (mRenderer && mRenderDevice) {
			mRenderer->Shutdown(*mRenderDevice);
		}
		mRenderer.reset();
		mRenderDevice.reset();
	}

	void RenderModule::Tick(const RenderFrameInputs& inputs) const {
		mRenderer->RenderFrame(*mRenderDevice, inputs);
	}

	void RenderModule::OnResize(
		const uint32_t width, const uint32_t height
	) const {
		if (mRenderDevice) {
			mRenderDevice->OnResize(width, height);
			mRenderer->OnResize(width, height);
		}
	}

	void RenderModule::SetUiCallbacks(
		Renderer::UiMainRenderCallback     mainRenderCallback,
		Renderer::UiPlatformRenderCallback platformRenderCallback
	) const {
		if (!mRenderer) {
			return;
		}
		mRenderer->SetUiCallbacks(
			std::move(mainRenderCallback), std::move(platformRenderCallback)
		);
	}

	SceneOutputView RenderModule::GetViewOutputView(
		const std::string_view viewKey
	) const {
		if (!mRenderer || !mRenderDevice) {
			return {};
		}
		return mRenderer->GetViewOutputView(*mRenderDevice, viewKey);
	}

	Vec2 RenderModule::GetViewOutputSize(const std::string_view viewKey) const {
		if (!mRenderer || !mRenderDevice) {
			return Vec2::zero;
		}
		return mRenderer->GetViewOutputSize(viewKey);
	}
}
