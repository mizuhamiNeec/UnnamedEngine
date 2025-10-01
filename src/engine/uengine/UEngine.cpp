#include <pch.h>

//-----------------------------------------------------------------------------

#include <engine/gameframework/component/Camera/UCameraComponent.h>
#include <engine/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <engine/subsystem/console/ConsoleSystem.h>
#include <engine/subsystem/input/KeyNameTable.h>
#include <engine/subsystem/input/UInputSystem.h>
#include <engine/subsystem/input/device/keyboard/KeyboardDevice.h>
#include <engine/subsystem/input/device/mouse/MouseDevice.h>
#include <engine/subsystem/interface/ServiceLocator.h>
#include <engine/subsystem/time/TimeSystem.h>
#include <engine/subsystem/window/Win32/Win32WindowSystem.h>
#include <engine/uengine/UEngine.h>

#include <runtime/assets/core/UAssetManager.h>
#include <runtime/assets/loaders/DirectXTexTextureLoader.h>
#include <runtime/assets/loaders/MaterialLoader.h>
#include <runtime/assets/loaders/MeshLoader.h>
#include <runtime/assets/loaders/RawLoader.h>
#include <runtime/assets/loaders/ShaderLoader.h>

#include "engine/gameframework/component/Transform/TransformComponent.h"

namespace Unnamed {
	constexpr std::string_view kChannel = "Engine";

	UEngine::UEngine() = default;

	UEngine::~UEngine() = default;

	void UEngine::Run() {
		Init();
		Tick();
		Shutdown();
	}

	UMaterialRuntime mr;

	void UEngine::Init() {
		Msg(kChannel, "Hello!");
		// サブシステムの作成
		mSubsystems.emplace_back(std::make_unique<ConsoleSystem>());
		mSubsystems.emplace_back(std::make_unique<TimeSystem>());
		mSubsystems.emplace_back(std::make_unique<Win32WindowSystem>());
		mSubsystems.emplace_back(std::make_unique<UInputSystem>());

		// サブシステムの登録
		for (const auto& subsystem : mSubsystems) {
			if (subsystem->Init()) {
				auto name = std::string(subsystem->GetName());
				SpecialMsg(
					LogLevel::Success,
					"Engine",
					"Subsystem initialized: {}",
					subsystem->GetName()
				);
			} else {
				UASSERT(false && "Failed to initialize subsystem");
			}
		}

		// サブシステムをメンバに持っておく
		mTime         = ServiceLocator::Get<TimeSystem>();
		mWindowSystem = ServiceLocator::Get<Win32WindowSystem>();
		mInputSystem  = ServiceLocator::Get<UInputSystem>();

		// ウィンドウの作成
		const IWindow::WindowInfo info = {
			.title =
			"Main Window  -  " + kEngineBuildDate + " " + kEngineBuildTime,
			.clWidth = 1280,
			.clHeight = 720,
			.bIsResizable = true,
			.bCreateAtCenter = false,
		};
		mWindowSystem->CreateNewWindow(info);

		const auto* mainWindow = mWindowSystem->GetWindows().front().get();

		// 作成したウィンドウに合わせてグラフィックスデバイスを初期化
		mGraphicsDevice                 = std::make_unique<GraphicsDevice>();
		const GraphicsDeviceInfo gdInfo = {
			.hWnd = mainWindow->GetNativeHandle(),
			.width = mainWindow->GetInfo().clWidth,
			.height = mainWindow->GetInfo().clHeight,
#ifdef _DEBUG
			.bEnableDebug = true,
#else
			.bEnableDebug = false,
#endif
		};
		mGraphicsDevice->Init(gdInfo);

		// プラットフォームイベントの作成
		mPlatformEvents = std::make_unique<PlatformEventsImpl>();

		// 入力システムをリスナーに登録
		Win32WindowSystem::RegisterPlatformEvent(mPlatformEvents.get());
		mPlatformEvents->AddListener(mInputSystem);

		// 入力システムにデバイスを登録
		{
			const auto keyboardDevice = std::make_shared<KeyboardDevice>(
				static_cast<HWND>(mainWindow->GetNativeHandle())
			);
			const auto mouseDevice = std::make_shared<MouseDevice>(
				static_cast<HWND>(mainWindow->GetNativeHandle())
			);
			mInputSystem->RegisterDevice(keyboardDevice);
			mInputSystem->RegisterDevice(mouseDevice);
		}

		// 適当にキーボードとマウスを割り当て
		{
			auto w = KeyNameTable::FromString("w");
			mInputSystem->BindAxis2D(
				"move",
				{
					.device = w->device,
					.code = w->code,
				},
				INPUT_AXIS::Y,
				1.0f
			);

			auto s = KeyNameTable::FromString("s");
			mInputSystem->BindAxis2D(
				"move",
				{
					.device = s->device,
					.code = s->code,
				},
				INPUT_AXIS::Y,
				-1.0f
			);

			auto d = KeyNameTable::FromString("d");
			mInputSystem->BindAxis2D(
				"move",
				{
					.device = d->device,
					.code = d->code,
				},
				INPUT_AXIS::X,
				1.0f
			);

			auto a = KeyNameTable::FromString("a");
			mInputSystem->BindAxis2D(
				"move",
				{
					.device = a->device,
					.code = a->code,
				},
				INPUT_AXIS::X,
				-1.0f
			);

			mInputSystem->BindAxis2D(
				"mouse",
				{
					.device = InputDeviceType::MOUSE,
					.code = VM_X
				},
				INPUT_AXIS::X,
				1.0f
			);
			mInputSystem->BindAxis2D(
				"mouse",
				{
					.device = InputDeviceType::MOUSE,
					.code = VM_Y
				},
				INPUT_AXIS::Y,
				1.0f
			);

			mInputSystem->BindAxis1D(
				"wheel",
				{
					.device = InputDeviceType::MOUSE,
					.code = VM_WHEEL
				},
				1.0f
			);
		}

		// AMの作成と初期化
		mAssetManager = std::make_unique<UAssetManager>();
		auto matLoader = std::make_unique<MaterialLoader>(mAssetManager.get());
		auto texLoader = std::make_unique<DirectXTexTextureLoader>();
		auto shaderLoader = std::make_unique<ShaderLoader>(mAssetManager.get());
		auto rawFileLoader = std::make_unique<RawLoader>();
		auto meshLoader = std::make_unique<MeshLoader>();
		mAssetManager->RegisterLoader(std::move(matLoader));
		mAssetManager->RegisterLoader(std::move(texLoader));
		mAssetManager->RegisterLoader(std::move(shaderLoader));
		mAssetManager->RegisterLoader(std::move(rawFileLoader));
		mAssetManager->RegisterLoader(std::move(meshLoader));

		mUploadArena = std::make_unique<UploadArena>();
		mUploadArena->Init(
			mGraphicsDevice->Device(),
			128ull * 1024 * 1024 // 128MB/Frame
		);

		// RRMの作成と初期化
		mRenderResourceManager = std::make_unique<RenderResourceManager>(
			mGraphicsDevice.get(), mAssetManager.get(), mUploadArena.get()
		);

		// ShaderLibraryの作成と初期化
		mShaderLibrary = std::make_unique<ShaderLibrary>(
			mGraphicsDevice.get(), mAssetManager.get()
		);

		// RootSignatureCacheの作成と初期化
		mRootSignatureCache = std::make_unique<RootSignatureCache>(
			mGraphicsDevice.get()
		);

		// PipelineCacheの作成と初期化
		mPipelineCache = std::make_unique<UPipelineCache>(
			mGraphicsDevice.get(), mRootSignatureCache.get()
		);

		mSubsystems.emplace_back(
			std::make_unique<URenderSubsystem>(
				mGraphicsDevice.get(),
				mRenderResourceManager.get(),
				mShaderLibrary.get(),
				mRootSignatureCache.get(),
				mPipelineCache.get()
			)
		);

		mRenderer = ServiceLocator::Get<URenderSubsystem>();
		mRenderer->Init();

		mWorld = std::make_unique<UWorld>("Main");

		{
			mr.materialAsset = mAssetManager->LoadFromFile(
				"./content/core/materials/dev.mat", UASSET_TYPE::MATERIAL
			);
			mAssetManager->AddRef(mr.materialAsset);
			bool ok = mr.BuildCPU(
				mAssetManager.get(),
				mShaderLibrary.get(),
				mRootSignatureCache.get(),
				mPipelineCache.get(),
				mGraphicsDevice.get()
			);
			UASSERT(ok && "Failed to build material.");

			const auto meshAsset = mAssetManager->LoadFromFile(
				"./content/core/models/error.obj", UASSET_TYPE::MESH
			);
			mAssetManager->AddRef(meshAsset);

			auto* e            = mWorld->SpawnEmpty("OBJ");
			auto* tr           = e->GetOrAddComponent<TransformComponent>();
			auto* meshRenderer = e->GetOrAddComponent<MeshRendererComponent>();

			meshRenderer->meshAsset     = meshAsset;
			meshRenderer->material      = mr;
			meshRenderer->materialAsset = mr.materialAsset;

			tr->SetPosition(Vec3::forward * 4.0f);
		}

		{
			auto* eCam       = mWorld->SpawnEmpty("Camera");
			mCameraTransform = eCam->GetOrAddComponent<TransformComponent>();
			mCamera          = eCam->GetOrAddComponent<UCameraComponent>();
			mCameraTransform->SetPosition(Vec3::backward * 4.0f);
		}

		for (const auto id : mAssetManager->AllAssets()) {
			const auto& meta = mAssetManager->Meta(id);
			Msg(
				kChannelNone,
				"Asset: {}, {}",
				meta.name.c_str(),
				ToString(meta.type)
			);

			for (const auto dep : mAssetManager->Dependencies(id)) {
				Msg(
					kChannelNone,
					"Dependency: {} -> {}",
					meta.name.c_str(),
					mAssetManager->Meta(dep).name.c_str()
				);
			}
		}

		auto camera = std::make_unique<UCameraComponent>();

		{
			RECT client;
			GetClientRect(
				static_cast<HWND>(mainWindow->GetNativeHandle()), &client
			);

			// クライアント座標の中心をスクリーン座標に変換
			POINT center;
			center.x = (client.right - client.left) / 2;
			center.y = (client.bottom - client.top) / 2;
			ClientToScreen(
				static_cast<HWND>(mainWindow->GetNativeHandle()), &center
			);

			RECT clip;
			clip.left   = center.x;
			clip.top    = center.y;
			clip.right  = center.x + 1;
			clip.bottom = center.y + 1;

			ClipCursor(&clip);
		}
	}

	void UEngine::Tick() {
		while (!mWindowSystem->AllClosed()) {
			mTime->BeginFrame();
			const float deltaTime = mTime->GetGameTime()->DeltaTime<float>();

			// サブシステムの更新
			for (const auto& subsystem : mSubsystems) {
				subsystem->Update(deltaTime);
			}

			//-----------------------------------------------------------------
			// 更新処理↓
			//-----------------------------------------------------------------

			Vec2 delta = mInputSystem->Axis2D("mouse");

			// 感度と回転値を計算
			const float sensitivity  = 1.25f;
			const float m_pitch      = 0.022f;
			const float m_yaw        = 0.022f;
			const float cl_pitchdown = 89.0f;
			const float cl_pitchup   = 89.0f;

			static float pitch = 0.0f;
			static float yaw   = 0.0f;
			static float speed = 1.0f;

			speed += mInputSystem->Axis1D("wheel");
			Msg(kChannelNone, "Speed: {}", speed);

			pitch += delta.y * sensitivity * m_pitch;
			yaw += delta.x * sensitivity * m_yaw;

			// ピッチをクランプ（上下回転の制限）
			pitch = std::clamp(pitch, -cl_pitchup, cl_pitchdown);

			// クォータニオンを生成（回転順序: ヨー → ピッチ）
			Quaternion yawRotation = Quaternion::AxisAngle(
				Vec3::up, yaw * Math::deg2Rad);
			Quaternion pitchRotation = Quaternion::AxisAngle(
				Vec3::right, pitch * Math::deg2Rad);

			// 回転を合成して設定
			Quaternion finalRotation = yawRotation * pitchRotation;
			mCameraTransform->SetRotation(finalRotation);

			auto prevPos = mCameraTransform->Position();
			auto mat     = mCameraTransform->WorldMat();
			auto forward = mat.GetForward();
			auto right   = mat.GetRight();

			prevPos += forward * mInputSystem->Axis2D("move").y * speed *
				deltaTime;
			prevPos += right * mInputSystem->Axis2D("move").x * speed *
				deltaTime;

			mCameraTransform->SetPosition(prevPos);

			mWorld->Tick(deltaTime);

			//-----------------------------------------------------------------

			// const uint32_t reload = mr.DetectChanges();
			// if (reload & RELOAD_CPU) {
			// 	mAssetManager->Reload(mr.materialAsset);
			//
			// 	mr.BuildCPU(
			// 		mAssetManager.get(),
			// 		mShaderLibrary.get(),
			// 		mRootSignatureCache.get(),
			// 		mPipelineCache.get(),
			// 		mGraphicsDevice.get()
			// 	);
			// }
			//
			// if (reload & RELOAD_TEXTURE) {
			// 	if (mLastSubmit.valid) {
			// 		mr.InvalidateGPU(
			// 			mRenderResourceManager.get(),
			// 			mLastSubmit.fence.Get(),
			// 			mLastSubmit.value
			// 		);
			// 	} else {
			// 		mr.InvalidateGPU(
			// 			mRenderResourceManager.get(),
			// 			nullptr,
			// 			0
			// 		);
			// 	}
			// }

			// bool ok      = mUploadArena->BeginFrame(context.backIndex);
			// UASSERT(ok);
			// 描画処理↓
			//-----------------------------------------------------------------
			//
			// if (!mr.IsGPUReady()) {
			// 	mr.RealizeGPU(mRenderResourceManager.get(), context.cmd);
			// }
			//
			// context.cmd->SetGraphicsRootSignature(
			// 	mRootSignatureCache->Get(mr.root));
			// context.cmd->SetPipelineState(mPipelineCache->Get(mr.pso));
			//
			// mr.Apply(
			// 	context.cmd,
			// 	mRenderResourceManager.get(),
			// 	context.backIndex,
			// 	static_cast<float>(mTime->GetGameTime()->TotalTime())
			// );
			//
			// mRenderResourceManager->ProcessDeferredMipUploads(context.cmd);
			//
			// mGraphicsDevice->DrawTriangleTest(context.cmd);

			const auto* cam  = mWorld->MainCamera();
			RenderView  view = {
				.view = UCameraComponent::View(mCameraTransform),
				.proj = cam->Proj(
					mWindowSystem->GetWindows().front()->GetInfo().clWidth /
					static_cast<float>(mWindowSystem->GetWindows().front()->
						GetInfo().clHeight
					)
				),
				.cameraPos = mCameraTransform->Position()
			};
			view.viewProj = view.view * view.proj;
			mRenderer->SetView(view);
			mRenderer->BeginFrame();
			mRenderer->RenderWorld(*mWorld);
			mRenderer->EndFrame();
			//-----------------------------------------------------------------

			// auto buffer = mGraphicsDevice->GetFrameBuffer(context.backIndex);
			// mUploadArena->OnFrameSubmitted(
			// 	context.backIndex,
			// 	buffer.fence.Get(),
			// 	buffer.fenceValue
			// );
			// mRenderResourceManager->FlushUploads(
			// 	buffer.fence.Get(), buffer.fenceValue
			// );
			// mRenderResourceManager->GarbageCollect();
			//
			// mLastSubmit = {
			// 	.fence = buffer.fence,
			// 	.value = buffer.fenceValue,
			// 	.valid = true
			// };

			mTime->EndFrame();
		}
	}

	void UEngine::Shutdown() const {
		mPlatformEvents->RemoveListener(mInputSystem);

		Msg(
			kChannelNone,
			"アリーヴェ帰ルチ! (さよナランチャ"
		);
	}
}
