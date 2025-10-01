#pragma once
#include <memory>
#include <vector>

#include <engine/gameframework/world/UWorld.h>
#include <engine/platform/PlatformEventsImpl.h>
#include <engine/subsystem/interface/ISubsystem.h>
#include <engine/subsystem/render/URenderSubsystem.h>
#include <engine/subsystem/window/Win32/Win32WindowSystem.h>
#include <engine/urenderer/GraphicsDevice.h>
#include <engine/urootsignaturecache/RootSignatureCache.h>
#include <engine/uuploadarena/UploadArena.h>

class TimeSystem;

namespace Unnamed {
	class UInputSystem;

	class UEngine {
	public:
		UEngine();
		~UEngine();

		void Run();

	private:
		void Init();
		void Tick();
		void Shutdown() const;

	private:
		std::vector<std::unique_ptr<ISubsystem>> mSubsystems;

		std::unique_ptr<PlatformEventsImpl> mPlatformEvents;

		std::unique_ptr<UAssetManager>         mAssetManager;
		std::unique_ptr<GraphicsDevice>        mGraphicsDevice;
		std::unique_ptr<RenderResourceManager> mRenderResourceManager;
		std::unique_ptr<ShaderLibrary>         mShaderLibrary;
		std::unique_ptr<RootSignatureCache>    mRootSignatureCache;
		std::unique_ptr<UPipelineCache>        mPipelineCache;
		std::unique_ptr<UploadArena>           mUploadArena;

		std::unique_ptr<UWorld> mWorld;

		TimeSystem*        mTime;
		Win32WindowSystem* mWindowSystem;
		UInputSystem*      mInputSystem;
		URenderSubsystem*  mRenderer;

		TransformComponent* mCameraTransform = nullptr;
		UCameraComponent*   mCamera          = nullptr;
	};
}
