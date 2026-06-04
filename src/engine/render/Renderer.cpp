#include "Renderer.h"

#include <algorithm>
#include <unordered_set>

#include "RenderDevice.h"

#include "engine/profiler/Profiler.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"
#include "engine/rhi/interface/IRhiDevice.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::Render {
	namespace {
		constexpr std::string_view kRenderChannel = "Renderer";
		constexpr uint64_t         kTextureCacheStatsLogIntervalFrames = 120;
	}

	Renderer::~Renderer() = default;

	void Renderer::Shutdown(RenderDevice& renderDevice) {
		mTextureResourceCache.ReleaseAll();
		ReleaseMaterialBindings(renderDevice);
		if (mDirectionalShadow.shadowDepthTextureId != 0) {
			renderDevice.GetRegistry().ReleaseTexture(
				mDirectionalShadow.shadowDepthTextureId
			);
			mDirectionalShadow.shadowDepthTextureId = 0;
		}
		if (mSpriteFallbackTextureId != 0) {
			renderDevice.GetRegistry().ReleaseTexture(mSpriteFallbackTextureId);
			mSpriteFallbackTextureId = 0;
		}
		renderDevice.FlushGpuAndCollectGarbage();
	}

	void Renderer::RenderFrame(
		RenderDevice& renderDevice, const RenderFrameInputs& inputs
	) {
		Profiler*      profiler         = ServiceLocator::Get<Profiler>();
		auto&          rhi              = renderDevice.GetRhiDevice();
		auto&          dx               = static_cast<Rhi::D3D12Device&>(rhi);
		const auto&    swapChain        = rhi.GetSwapChain();
		const uint32_t backBufferWidth  = swapChain.GetWidth();
		const uint32_t backBufferHeight = swapChain.GetHeight();
		mTextureResourceCache.BeginFrame(inputs.frameIndex);

		std::unordered_set<AssetID> dirtyMeshAssets;
		bool                        materialsDirty = false;
		bool                        postFxDirty    = false;
		renderDevice.ConsumeDirtyAssets(
			dirtyMeshAssets, materialsDirty, postFxDirty
		);
		if (!dirtyMeshAssets.empty()) {
			for (const AssetID dirtyMesh : dirtyMeshAssets) {
				mSceneMeshesByAsset.erase(dirtyMesh);
				if (mLoadedMeshAsset == dirtyMesh) {
					mLoadedMeshAsset = kInvalidAssetID;
				}
			}
		}
		if (materialsDirty) {
			// Hot reload invalidates material constants, texture SRVs, pipeline handles, and resolved PSO pointers.
			ReleaseMaterialBindings(renderDevice);
		}
		if (postFxDirty) {
			RebuildPipelineCatalog(renderDevice, dx);
		}

		mFrameViews      = inputs.views;
		mFrameDebugLines = inputs.debugDraw.lines;
		if (mFrameViews.empty()) {
			RenderViewInput fallback = {};
			fallback.viewKey = "default.main";
			fallback.type = RENDER_VIEW_TYPE::SCENE;
			fallback.output.sizeMode = RENDER_VIEW_SIZE_MODE::MATCH_BACK_BUFFER;
			fallback.output.presentToSwapChain = true;
			fallback.sceneViewMode.mode = SCENE_RENDER_MODE::FIT_VIEWPORT;
			mFrameViews.emplace_back(std::move(fallback));
		}

		UploadDebugLinesForFrame();

		mViewExecutionOrder.clear();
		mPresentViewKey.clear();
		std::unordered_set<std::string> activeViewKeys;
		activeViewKeys.reserve(mFrameViews.size());

		for (RenderViewInput& view : mFrameViews) {
			if (view.viewKey.empty()) {
				view.viewKey = "unnamed.view";
			}
			mViewExecutionOrder.emplace_back(view.viewKey);
			activeViewKeys.emplace(view.viewKey);

			if (view.type == RENDER_VIEW_TYPE::SCENE) {
				const auto [sceneWidth, sceneHeight] = ResolveSceneRenderExtent(
					backBufferWidth, backBufferHeight, view.sceneViewMode
				);
				view.output.width  = sceneWidth;
				view.output.height = sceneHeight;
			} else if (
				view.output.sizeMode == RENDER_VIEW_SIZE_MODE::MATCH_BACK_BUFFER
			) {
				view.output.width  = std::max(1u, backBufferWidth);
				view.output.height = std::max(1u, backBufferHeight);
			} else {
				view.output.width  = std::max(1u, view.output.width);
				view.output.height = std::max(1u, view.output.height);
			}

			std::ranges::sort(view.worldBillboards
			                  ,
			                  [](
			                  const WorldBillboardInput& a,
			                  const WorldBillboardInput& b
		                  ) {
				                  return a.sortKey < b.sortKey;
			                  }
			);
			std::ranges::sort(view.worldSprites
			                  ,
			                  [](
			                  const WorldSpriteInput& a,
			                  const WorldSpriteInput& b
		                  ) {
				                  return a.sortKey < b.sortKey;
			                  }
			);
			std::ranges::sort(view.screenSprites
			                  ,
			                  [](
			                  const ScreenSpriteInput& a,
			                  const ScreenSpriteInput& b
		                  ) {
				                  return a.sortKey < b.sortKey;
			                  }
			);

			if (
				mPresentViewKey.empty() &&
				view.output.presentToSwapChain
			) {
				mPresentViewKey = view.viewKey;
			}

			auto&          state = mViewStates[view.viewKey];
			const bool     typeChanged = state.type != view.type;
			const uint32_t logicalWidth = std::max(1u, view.output.width);
			const uint32_t logicalHeight = std::max(1u, view.output.height);
			const bool     allowGrowOnlyReuse =
				view.type == RENDER_VIEW_TYPE::SCENE &&
				view.output.exposeToUi &&
				!view.output.presentToSwapChain &&
				view.sceneViewMode.preferRealtimeResize &&
				view.sceneViewMode.mode == SCENE_RENDER_MODE::FIT_VIEWPORT;
			const uint32_t allocationHintWidth = allowGrowOnlyReuse ?
				                                     std::max(
					                                     logicalWidth,
					                                     std::max(
						                                     1u,
						                                     view.sceneViewMode.
						                                     allocationHintWidth
					                                     )
				                                     ) :
				                                     logicalWidth;
			const uint32_t allocationHintHeight = allowGrowOnlyReuse ?
				                                      std::max(
					                                      logicalHeight,
					                                      std::max(
						                                      1u,
						                                      view.sceneViewMode
						                                      .
						                                      allocationHintHeight
					                                      )
				                                      ) :
				                                      logicalHeight;

			state.type          = view.type;
			state.output        = view.output;
			state.logicalWidth  = logicalWidth;
			state.logicalHeight = logicalHeight;

			uint32_t desiredAllocatedWidth  = logicalWidth;
			uint32_t desiredAllocatedHeight = logicalHeight;
			if (allowGrowOnlyReuse) {
				const bool firstAllocation =
					typeChanged ||
					state.allocatedWidth <= 1 ||
					state.allocatedHeight <= 1;
				desiredAllocatedWidth = firstAllocation ?
					                        allocationHintWidth :
					                        std::max(
						                        state.allocatedWidth,
						                        logicalWidth
					                        );
				desiredAllocatedHeight = firstAllocation ?
					                         allocationHintHeight :
					                         std::max(
						                         state.allocatedHeight,
						                         logicalHeight
					                         );
			}

			const bool sizeChanged =
				typeChanged ||
				state.allocatedWidth != desiredAllocatedWidth ||
				state.allocatedHeight != desiredAllocatedHeight;
			state.allocatedWidth  = desiredAllocatedWidth;
			state.allocatedHeight = desiredAllocatedHeight;

			if (sizeChanged || typeChanged) {
				ReleaseViewRuntimeTextures(renderDevice, state);
				state.colorTextureId   = 0;
				state.depthTextureId   = 0;
				state.postFxTextureAId = 0;
				state.postFxTextureBId = 0;
				for (uint32_t& bloomMipTextureId : state.bloomMipTextureIds) {
					bloomMipTextureId = 0;
				}
				state.outputTextureId = 0;
			}
		}

		for (
			auto it = mViewStates.begin();
			it != mViewStates.end();
		) {
			if (!activeViewKeys.contains(it->first)) {
				ReleaseViewRuntimeTextures(renderDevice, it->second);
				it = mViewStates.erase(it);
			} else {
				++it;
			}
		}

		bool requiresSpriteTextures = false;
		for (const RenderViewInput& view : mFrameViews) {
			if (
				!view.worldBillboards.empty() ||
				!view.worldSprites.empty() ||
				!view.screenSprites.empty()
			) {
				requiresSpriteTextures = true;
			}
			for (const auto& s : view.worldBillboards) {
				if (s.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, s.texture.textureAssetId
					);
				}
			}
			for (const auto& s : view.worldSprites) {
				if (s.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, s.texture.textureAssetId
					);
				}
			}
			for (const auto& s : view.screenSprites) {
				if (s.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, s.texture.textureAssetId
					);
				}
			}
			if (
				view.type == RENDER_VIEW_TYPE::SCENE &&
				view.skybox.enabled &&
				view.skybox.textureAssetId != kInvalidAssetID
			) {
				(void)EnsureSkyboxTextureLoaded(
					renderDevice, view.skybox.textureAssetId
				);
			}
			if (view.type != RENDER_VIEW_TYPE::SCENE) {
				continue;
			}
			for (const auto& obj : view.visibleObjects) {
				if (obj.meshAssetId != kInvalidAssetID) {
					(void)EnsureMeshResourceLoaded(
						renderDevice, dx, obj.meshAssetId
					);
				}
			}
		}
		if (requiresSpriteTextures) {
			EnsureSpriteFallbackTexture(renderDevice);
		}

		// 今フレームで参照されるマテリアルを遅延登録します。
		LoadMaterialResources(renderDevice, dx);
		ResolveRegisteredPipelines(renderDevice);

		rhi.BeginFrame();
		mAdvancedFoundation.BeginFrame();
		mTextureResourceCache.CollectGarbage();
		renderDevice.GetRegistry().CollectGarbage(dx.GetCompletedFenceValue());
		if (
			inputs.frameIndex == 0 ||
			inputs.frameIndex < mLastTextureCacheStatsLogFrame ||
			(inputs.frameIndex - mLastTextureCacheStatsLogFrame) >=
			kTextureCacheStatsLogIntervalFrames
		) {
			const TextureResourceCacheDebugStats cacheStats =
				mTextureResourceCache.GetDebugStats();
			const RgRegistryDebugStats registryStats =
				renderDevice.GetRegistry().GetDebugStats();
			DevMsg(
				kRenderChannel,
				"TextureCacheStats frame={} live={}, sprite={}, skybox={}, created={}, ttlReleased={}, versionRecreated={}, releaseAllReleased={}, failedResolve={}, frameTtlReleased={}, registryLiveTex={}, registryRetiredTex={}, registrySrvUavSlots={}, registryRtvSlots={}, registryDsvSlots={}, registryCpuSrvUavSlots={}",
				inputs.frameIndex,
				cacheStats.liveEntryCount,
				cacheStats.spriteEntryCount,
				cacheStats.skyboxEntryCount,
				cacheStats.createdTextureCount,
				cacheStats.ttlReleaseCount,
				cacheStats.versionRecreateCount,
				cacheStats.releaseAllReleaseCount,
				cacheStats.failedResolveCount,
				cacheStats.lastFrameReleasedByTtl,
				registryStats.activeTextureCount,
				registryStats.retiredResourceCount,
				registryStats.srvUavActiveSlots,
				registryStats.rtvActiveSlots,
				registryStats.dsvActiveSlots,
				registryStats.cpuSrvUavActiveSlots
			);
			mLastTextureCacheStatsLogFrame = inputs.frameIndex;
		}

		{
			Profiler::ScopeTimer scope(profiler, "Render.BuildGraph");
			mGraph.Reset();
			mGraphBuilt = false;
			BuildGraph(renderDevice, mFrameViews);
		}
		{
			Profiler::ScopeTimer scope(profiler, "Render.GraphExecute");
			mGraph.Execute(rhi);
		}
		if (mUiPlatformRenderCallback) {
			mUiPlatformRenderCallback();
		}

		rhi.EndFrame();
	}

	void Renderer::OnResize(const uint32_t width, const uint32_t height) {
		mAdvancedFoundation.OnResize(width, height);
		DevMsg(
			kRenderChannel,
			"OnResize: {}x{}",
			width,
			height
		);
	}

	void Renderer::SetUiCallbacks(
		UiMainRenderCallback     mainRenderCallback,
		UiPlatformRenderCallback platformRenderCallback
	) {
		mUiMainRenderCallback     = std::move(mainRenderCallback);
		mUiPlatformRenderCallback = std::move(platformRenderCallback);
	}

	SceneOutputView Renderer::GetViewOutputView(
		const RenderDevice& renderDevice, const std::string_view viewKey
	) const {
		SceneOutputView view = {};
		const auto      it   = mViewStates.find(std::string(viewKey));
		if (it == mViewStates.end()) {
			return view;
		}

		view.textureId = it->second.outputTextureId;
		if (view.textureId == 0) {
			return view;
		}

		const auto& registry =
			const_cast<RenderDevice&>(renderDevice).GetRegistry();
		view.srvCpu = registry.GetSrvCpu(view.textureId);
		view.srvRevision = registry.GetSrvRevision(view.textureId);
		view.uvMin = Vec2(0.0f, 0.0f);
		const float safeAllocatedWidth = static_cast<float>(std::max(
			1u, it->second.allocatedWidth
		));
		const float safeAllocatedHeight = static_cast<float>(std::max(
			1u, it->second.allocatedHeight
		));
		view.uvMax = Vec2(
			std::clamp(
				static_cast<float>(std::max(1u, it->second.logicalWidth)) /
				safeAllocatedWidth,
				0.0f,
				1.0f
			),
			std::clamp(
				static_cast<float>(std::max(1u, it->second.logicalHeight)) /
				safeAllocatedHeight,
				0.0f,
				1.0f
			)
		);
		return view;
	}

	Vec2 Renderer::GetViewOutputSize(const std::string_view viewKey) const {
		const auto it = mViewStates.find(std::string(viewKey));
		if (it == mViewStates.end()) {
			return Vec2::zero;
		}
		return Vec2{
			static_cast<float>(std::max(1u, it->second.logicalWidth)),
			static_cast<float>(std::max(1u, it->second.logicalHeight))
		};
	}

	std::pair<uint32_t, uint32_t> Renderer::ResolveSceneRenderExtent(
		const uint32_t             backBufferWidth,
		const uint32_t             backBufferHeight,
		const SceneViewRenderMode& request
	) {
		uint32_t width  = backBufferWidth;
		uint32_t height = backBufferHeight;

		const uint32_t panelWidth = request.viewportPanelWidth != 0 ?
			                            request.viewportPanelWidth :
			                            std::max(1u, backBufferWidth);
		const uint32_t panelHeight = request.viewportPanelHeight != 0 ?
			                             request.viewportPanelHeight :
			                             std::max(1u, backBufferHeight);

		switch (request.mode) {
			case SCENE_RENDER_MODE::FIT_VIEWPORT: {
				width  = panelWidth;
				height = panelHeight;
				break;
			}
			case SCENE_RENDER_MODE::FIXED_ASPECT_16X9: {
				width  = panelWidth;
				height = panelHeight;
				if (width * 9 > height * 16) {
					width = height * 16 / 9;
				} else {
					height = width * 9 / 16;
				}
				break;
			}
			case SCENE_RENDER_MODE::FIXED_ASPECT_4X3: {
				width  = panelWidth;
				height = panelHeight;
				if (width * 3 > height * 4) {
					width = height * 4 / 3;
				} else {
					height = width * 3 / 4;
				}
				break;
			}
			case SCENE_RENDER_MODE::HD_720P: {
				width  = 1280;
				height = 720;
				break;
			}
			case SCENE_RENDER_MODE::FHD_1080P: {
				width  = 1920;
				height = 1080;
				break;
			}
			case SCENE_RENDER_MODE::UHD_4K: {
				width  = 3840;
				height = 2160;
				break;
			}
			default: break;
		}

		width  = std::clamp(width, 2u, 8192u);
		height = std::clamp(height, 2u, 8192u);

		if ((width & 1u) != 0u) {
			--width;
		}
		if ((height & 1u) != 0u) {
			--height;
		}

		width  = std::max(2u, width);
		height = std::max(2u, height);
		return {width, height};
	}

	void Renderer::ReleaseMaterialBindings(RenderDevice& renderDevice) {
		auto& registry = renderDevice.GetRegistry();
		if (!mMaterialBindings.empty()) {
			DevMsg(
				kRenderChannel,
				"Releasing {} material bindings and invalidating material geometry pipeline variants.",
				mMaterialBindings.size()
			);
		}
		for (const auto& [materialInstanceId, binding] : mMaterialBindings) {
			(void)materialInstanceId;
			if (binding.albedoTextureId != 0) {
				registry.ReleaseTexture(binding.albedoTextureId);
			}
		}
		mMaterialBindings.clear();
		mDefaultMaterialInstance = kInvalidAssetID;
	}

	void Renderer::ResolveRegisteredPipelines(RenderDevice& renderDevice) {
		mPipelineRegistry.ResolveAll(renderDevice);

		mFullscreenPass.resolved = mPipelineRegistry.GetGraphics(
			mFullscreenPass.pipeline
		);
		mHdrCopyPass.resolved = mPipelineRegistry.GetGraphics(
			mHdrCopyPass.pipeline
		);
		mToneMapPass.resolved = mPipelineRegistry.GetGraphics(
			mToneMapPass.pipeline
		);
		mBloomDownsamplePass.resolved = mPipelineRegistry.GetGraphics(
			mBloomDownsamplePass.pipeline
		);
		mBloomUpsamplePass.resolved = mPipelineRegistry.GetGraphics(
			mBloomUpsamplePass.pipeline
		);
		mBloomCombinePass.resolved = mPipelineRegistry.GetGraphics(
			mBloomCombinePass.pipeline
		);
		mDepthVisPass.resolved = mPipelineRegistry.GetGraphics(
			mDepthVisPass.pipeline
		);
		mComputePass.resolved = mPipelineRegistry.GetCompute(
			mComputePass.pipeline
		);

		mGeometryPass.resolved = mPipelineRegistry.GetGraphics(
			mGeometryPass.pipeline
		);
		mShadowDepthPass.resolved = mPipelineRegistry.GetGraphics(
			mShadowDepthPass.pipeline
		);
		mShadowDepthDoubleSidedPass.resolved = mPipelineRegistry.GetGraphics(
			mShadowDepthDoubleSidedPass.pipeline
		);
		mSkyboxPass.geom.resolved = mPipelineRegistry.GetGraphics(
			mSkyboxPass.geom.pipeline
		);
		mSpritePass.geom.resolved = mPipelineRegistry.GetGraphics(
			mSpritePass.geom.pipeline
		);
		mSpritePass.geomLinearClamp.resolved = mPipelineRegistry.GetGraphics(
			mSpritePass.geomLinearClamp.pipeline
		);
		mSpritePass.geomPointClamp.resolved = mPipelineRegistry.GetGraphics(
			mSpritePass.geomPointClamp.pipeline
		);
		mBillboardPass.depthGeom.resolved = mPipelineRegistry.GetGraphics(
			mBillboardPass.depthGeom.pipeline
		);
		mBillboardPass.frontGeom.resolved = mPipelineRegistry.GetGraphics(
			mBillboardPass.frontGeom.pipeline
		);
		mLinePass.resolved = mPipelineRegistry.GetGraphics(mLinePass.pipeline);

		for (auto& [materialInstanceId, binding] : mMaterialBindings) {
			binding.resolvedGeometryPipeline = mPipelineRegistry.GetGraphics(
				binding.geometryPipeline
			);
			if (
				(!binding.resolvedGeometryPipeline ||
				 !binding.resolvedGeometryPipeline->pso) &&
				!binding.pipelineResolveWarningEmitted
			) {
				Warning(
					kRenderChannel,
					"Failed to resolve geometry pipeline for material instance {}. Falling back to default geometry pipeline.",
					materialInstanceId
				);
				binding.pipelineResolveWarningEmitted = true;
			}
		}

		for (auto& pass : mPostFxPasses) {
			pass.pass.resolved = mPipelineRegistry.GetGraphics(
				pass.pass.pipeline
			);
		}
	}

	uint32_t Renderer::ResolveSpriteTexture(
		RenderDevice& renderDevice, const SpriteTextureRef& textureRef
	) {
		if (textureRef.source == SPRITE_TEXTURE_SOURCE::VIEW_OUTPUT) {
			if (const auto it = mViewStates.find(textureRef.viewKey);
				it != mViewStates.end() && it->second.outputTextureId != 0) {
				return it->second.outputTextureId;
			}
			EnsureSpriteFallbackTexture(renderDevice);
			return mSpriteFallbackTextureId;
		}
		return EnsureSpriteTextureLoaded(
			renderDevice, textureRef.textureAssetId
		);
	}

	void Renderer::InitializeDebugLineResources(const Rhi::D3D12Device& dx) {
		mLinePass.vertexCapacity   = kMaxDebugLines * 2;
		mLinePass.frameVertexCount = 0;
		mLinePass.mappedVertices   = nullptr;
		mLinePass.dynamicVb.Reset();
		mLinePass.frameVbv = {};

		const uint64_t bufferSize = static_cast<uint64_t>(
			                            sizeof(DebugLineVertex)
		                            ) * static_cast<uint64_t>(mLinePass.
			                            vertexCapacity);

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type                  = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width               = bufferSize;
		desc.Height              = 1;
		desc.DepthOrArraySize    = 1;
		desc.MipLevels           = 1;
		desc.SampleDesc.Count    = 1;
		desc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		Rhi::Throw(
			dx.GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(mLinePass.dynamicVb.ReleaseAndGetAddressOf())
			)
		);

		void*                 mapped    = nullptr;
		constexpr D3D12_RANGE readRange = {.Begin = 0, .End = 0};
		Rhi::Throw(mLinePass.dynamicVb->Map(0, &readRange, &mapped));
		mLinePass.mappedVertices = static_cast<DebugLineVertex*>(mapped);

		mLinePass.frameVbv.BufferLocation = mLinePass.dynamicVb->
			GetGPUVirtualAddress();
		mLinePass.frameVbv.SizeInBytes = static_cast<UINT>(
			sizeof(DebugLineVertex) * mLinePass.vertexCapacity
		);
		mLinePass.frameVbv.StrideInBytes = sizeof(DebugLineVertex);
		mLinePass.dynamicVb->SetName(L"DebugLineDynamicVB");
	}

	void Renderer::UploadDebugLinesForFrame() {
		mLinePass.frameVertexCount = 0;
		if (mLinePass.dynamicVb.Get() == nullptr || mLinePass.mappedVertices ==
		    nullptr) {
			return;
		}
		if (mFrameDebugLines.empty()) {
			return;
		}

		const size_t requestedLines = mFrameDebugLines.size();
		if (requestedLines > kMaxDebugLines) {
			Warning(
				kRenderChannel,
				"Debug line count exceeded limit. requested={}, limit={}, clipped={}",
				requestedLines,
				kMaxDebugLines,
				requestedLines - kMaxDebugLines
			);
		}

		const uint32_t lineCount = static_cast<uint32_t>(std::min<size_t>(
			requestedLines,
			static_cast<size_t>(kMaxDebugLines)
		));
		const uint32_t vertexCount = std::min<uint32_t>(
			lineCount * 2u,
			mLinePass.vertexCapacity
		);

		for (uint32_t i = 0; i < vertexCount / 2u; ++i) {
			const DebugLineInput& srcLine = mFrameDebugLines[i];
			DebugLineVertex&      v0 = mLinePass.mappedVertices[i * 2u + 0u];
			DebugLineVertex&      v1 = mLinePass.mappedVertices[i * 2u + 1u];

			v0.px = srcLine.start.x;
			v0.py = srcLine.start.y;
			v0.pz = srcLine.start.z;
			v0.r  = srcLine.color.x;
			v0.g  = srcLine.color.y;
			v0.b  = srcLine.color.z;
			v0.a  = srcLine.color.w;

			v1.px = srcLine.end.x;
			v1.py = srcLine.end.y;
			v1.pz = srcLine.end.z;
			v1.r  = srcLine.color.x;
			v1.g  = srcLine.color.y;
			v1.b  = srcLine.color.z;
			v1.a  = srcLine.color.w;
		}

		mLinePass.frameVertexCount     = vertexCount;
		mLinePass.frameVbv.SizeInBytes = static_cast<UINT>(
			sizeof(DebugLineVertex) * vertexCount
		);
	}

	void Renderer::ReleaseViewRuntimeTextures(
		RenderDevice& renderDevice, ViewRuntimeState& state
	) {
		auto& registry = renderDevice.GetRegistry();
		if (state.colorTextureId != 0) {
			registry.ReleaseTexture(state.colorTextureId);
		}
		if (state.depthTextureId != 0) {
			registry.ReleaseTexture(state.depthTextureId);
		}
		if (state.postFxTextureAId != 0) {
			registry.ReleaseTexture(state.postFxTextureAId);
		}
		if (state.postFxTextureBId != 0) {
			registry.ReleaseTexture(state.postFxTextureBId);
		}
		for (uint32_t& bloomMipTextureId : state.bloomMipTextureIds) {
			if (bloomMipTextureId != 0) {
				registry.ReleaseTexture(bloomMipTextureId);
			}
			bloomMipTextureId = 0;
		}
		if (state.outputTextureId != 0) {
			registry.ReleaseTexture(state.outputTextureId);
		}
		state.colorTextureId   = 0;
		state.depthTextureId   = 0;
		state.postFxTextureAId = 0;
		state.postFxTextureBId = 0;
		state.outputTextureId  = 0;
	}
}
