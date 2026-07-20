#include "Renderer.h"

#include <algorithm>
#include <unordered_set>

#include "RenderDevice.h"

#include "core/assets/types/TextureAssetData.h"

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

		uint32_t CreateSolidColorTexture(
			RenderDevice& renderDevice,
			const uint8_t r,
			const uint8_t g,
			const uint8_t b,
			const uint8_t a,
			const bool    isSrgb,
			const char*   debugName
		) {
			TextureAssetData texture = {};
			texture.width            = 1;
			texture.height           = 1;
			texture.arraySize        = 1;
			texture.mipLevels        = 1;
			texture.format           = isSrgb ?
				                 DXGI_FORMAT_R8G8B8A8_UNORM_SRGB :
				                 DXGI_FORMAT_R8G8B8A8_UNORM;
			texture.isSRGB    = isSrgb;
			texture.dimension = TEXTURE_DIMENSION::TEXTURE_2D;
			TextureMip mip    = {};
			mip.width         = 1;
			mip.height        = 1;
			mip.rowPitch      = 4;
			mip.bytes         = {r, g, b, a};
			texture.mips.emplace_back(std::move(mip));
			TextureSubresource subresource = {};
			subresource.width              = 1;
			subresource.height             = 1;
			subresource.rowPitch           = 4;
			subresource.slicePitch         = 4;
			subresource.mipLevel           = 0;
			subresource.arraySlice         = 0;
			subresource.bytes              = {r, g, b, a};
			texture.subresources.emplace_back(std::move(subresource));
			return renderDevice.GetRegistry().CreateTexture2DFromAsset(
				texture,
				debugName
			);
		}
	}

	void Renderer::Shutdown(RenderDevice& renderDevice) {
		mTextureResourceCache.ReleaseAll();
		ReleaseMaterialBindings(renderDevice);
		ReleaseDefaultMaterialTextures(renderDevice);
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
			// ホットリロード後に古い GPU メッシュ状態を次フレームへ持ち越さない
			for (const AssetID dirtyMesh : dirtyMeshAssets) {
				mSceneMeshesByAsset.erase(dirtyMesh);
			}
		}
		if (materialsDirty) {
			// Hot reload invalidates material constants, texture SRVs, pipeline handles, and resolved PSO pointers.
			ReleaseMaterialBindings(renderDevice);
		}
		if (postFxDirty) {
			(void)RebuildPipelineCatalog(
				renderDevice, dx, mStartupOptions
			);
		}

		// ワールドが提出した描画入力を、このフレームで実行するビュー一覧として確定する
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

		SynchronizeViewRuntimeStates(
			renderDevice, backBufferWidth, backBufferHeight
		);

		PrepareFrameResources(renderDevice, dx);

		rhi.BeginFrame();
		mTextureResourceCache.CollectGarbage();
		renderDevice.GetRegistry().CollectGarbage(dx.GetCompletedFenceValue());
		if (
			inputs.frameIndex == 0 ||
			inputs.frameIndex < mLastTextureCacheStatsLogFrame ||
			inputs.frameIndex - mLastTextureCacheStatsLogFrame >=
			kTextureCacheStatsLogIntervalFrames
		) {
			const TextureResourceCacheDebugStats cacheStats =
				mTextureResourceCache.GetDebugStats();
			const RgRegistryDebugStats registryStats =
				renderDevice.GetRegistry().GetDebugStats();
			DevMsg(
				kRenderChannel,
				"TextureCacheStats frame={} live={}, sprite={}, skybox={}, material={}, created={}, ttlReleased={}, versionRecreated={}, releaseAllReleased={}, failedResolve={}, frameTtlReleased={}, registryLiveTex={}, registryRetiredTex={}, registrySrvUavSlots={}, registryRtvSlots={}, registryDsvSlots={}, registryCpuSrvUavSlots={}",
				inputs.frameIndex,
				cacheStats.liveEntryCount,
				cacheStats.spriteEntryCount,
				cacheStats.skyboxEntryCount,
				cacheStats.materialEntryCount,
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
			// 全ビューのリソースを揃えてから、依存関係を持つ描画グラフを構築する
			Profiler::ScopeTimer scope(profiler, "Render.BuildGraph");
			mGraph.Reset();
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

	void Renderer::SynchronizeViewRuntimeStates(
		RenderDevice&  renderDevice,
		const uint32_t backBufferWidth,
		const uint32_t backBufferHeight
	) {
		mPresentViewKey.clear();
		std::unordered_set<std::string> activeViewKeys;
		activeViewKeys.reserve(mFrameViews.size());

		for (RenderViewInput& view : mFrameViews) {
			if (view.viewKey.empty()) {
				view.viewKey = "unnamed.view";
			}
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

			std::ranges::sort(view.worldBillboards, [](
			                  const WorldBillboardInput& a, const WorldBillboardInput& b
		                  ) {
				                  return a.sortKey < b.sortKey;
			                  });
			std::ranges::sort(view.worldSprites, [](
			                  const WorldSpriteInput& a, const WorldSpriteInput& b
		                  ) {
				                  return a.sortKey < b.sortKey;
			                  });
			std::ranges::sort(view.screenSprites, [](
			                  const ScreenSpriteInput& a, const ScreenSpriteInput& b
		                  ) {
				                  return a.sortKey < b.sortKey;
			                  });

			if (mPresentViewKey.empty() && view.output.presentToSwapChain) {
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
					                                     std::max(1u, view.sceneViewMode.allocationHintWidth)
				                                     ) :
				                                     logicalWidth;
			const uint32_t allocationHintHeight = allowGrowOnlyReuse ?
				                                      std::max(
					                                      logicalHeight,
					                                      std::max(1u, view.sceneViewMode.allocationHintHeight)
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
					                        std::max(state.allocatedWidth, logicalWidth);
				desiredAllocatedHeight = firstAllocation ?
					                         allocationHintHeight :
					                         std::max(state.allocatedHeight, logicalHeight);
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

		for (auto it = mViewStates.begin(); it != mViewStates.end();) {
			if (!activeViewKeys.contains(it->first)) {
				ReleaseViewRuntimeTextures(renderDevice, it->second);
				it = mViewStates.erase(it);
			} else {
				++it;
			}
		}
	}

	void Renderer::PrepareFrameResources(
		RenderDevice& renderDevice, Rhi::D3D12Device& dx
	) {
		bool requiresSpriteTextures = false;
		for (const RenderViewInput& view : mFrameViews) {
			if (
				!view.worldBillboards.empty() ||
				!view.worldSprites.empty() ||
				!view.screenSprites.empty()
			) {
				requiresSpriteTextures = true;
			}
			for (const auto& sprite : view.worldBillboards) {
				if (sprite.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, sprite.texture.textureAssetId
					);
				}
			}
			for (const auto& sprite : view.worldSprites) {
				if (sprite.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, sprite.texture.textureAssetId
					);
				}
			}
			for (const auto& sprite : view.screenSprites) {
				if (sprite.texture.source == SPRITE_TEXTURE_SOURCE::ASSET) {
					requiresSpriteTextures = true;
					(void)EnsureSpriteTextureLoaded(
						renderDevice, sprite.texture.textureAssetId
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
			for (const auto& object : view.visibleObjects) {
				if (object.meshAssetId != kInvalidAssetID) {
					(void)EnsureMeshResourceLoaded(
						renderDevice, dx, object.meshAssetId
					);
				}
			}
		}
		if (requiresSpriteTextures) {
			EnsureSpriteFallbackTexture(renderDevice);
		}
		EnsureDefaultMaterialTextures(renderDevice);

		// 今フレームで参照されるマテリアルを遅延登録します。
		LoadMaterialResources(renderDevice, dx);
		for (auto& [materialInstanceId, binding] : mMaterialBindings) {
			(void)materialInstanceId;
			EnsureMaterialTextureTable(renderDevice, binding);
		}
		(void)ResolveRegisteredPipelines(renderDevice);
	}

	std::pair<uint32_t, uint32_t> Renderer::ResolveSceneRenderExtent(
		const uint32_t             backBufferWidth,
		const uint32_t             backBufferHeight,
		const SceneViewRenderMode& request
	) {
		return ResolveSceneViewRenderExtent(
			backBufferWidth,
			backBufferHeight,
			request
		);
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
		for (auto& [materialInstanceId, binding] : mMaterialBindings) {
			(void)materialInstanceId;
			registry.ReleaseSrvDescriptorTable(binding.materialTextureTable);
		}
		mMaterialBindings.clear();
		mDefaultMaterialInstance = kInvalidAssetID;
	}

	void Renderer::EnsureDefaultMaterialTextures(RenderDevice& renderDevice) {
		// Material fallback textures are renderer-owned and shared by all
		// MaterialBinding instances until Shutdown.
		if (mDefaultMaterialTextures.baseColorTextureId == 0) {
			mDefaultMaterialTextures.baseColorTextureId =
				CreateSolidColorTexture(
					renderDevice, 255, 255, 255, 255, true,
					"MaterialDefaultBaseColorWhite"
				);
		}
		if (mDefaultMaterialTextures.normalTextureId == 0) {
			mDefaultMaterialTextures.normalTextureId =
				CreateSolidColorTexture(
					renderDevice, 128, 128, 255, 255, false,
					"MaterialDefaultNormalFlat"
				);
		}
		if (mDefaultMaterialTextures.ormTextureId == 0) {
			// ORM convention: R=AO, G=Perceptual Roughness, B=Metallic.
			mDefaultMaterialTextures.ormTextureId =
				CreateSolidColorTexture(
					renderDevice, 255, 255, 255, 255, false,
					"MaterialDefaultOrm"
				);
		}
		if (mDefaultMaterialTextures.emissiveTextureId == 0) {
			mDefaultMaterialTextures.emissiveTextureId =
				CreateSolidColorTexture(
					renderDevice, 255, 255, 255, 255, true,
					"MaterialDefaultEmissiveWhite"
				);
		}
	}

	void Renderer::ReleaseDefaultMaterialTextures(RenderDevice& renderDevice) {
		auto releaseTexture = [&](uint32_t& textureId) {
			if (textureId == 0) {
				return;
			}
			renderDevice.GetRegistry().ReleaseTexture(textureId);
			textureId = 0;
		};

		releaseTexture(mDefaultMaterialTextures.baseColorTextureId);
		releaseTexture(mDefaultMaterialTextures.normalTextureId);
		releaseTexture(mDefaultMaterialTextures.ormTextureId);
		releaseTexture(mDefaultMaterialTextures.emissiveTextureId);
	}

	void Renderer::EnsureMaterialTextureTable(
		RenderDevice& renderDevice, MaterialBinding& binding
	) const {
		auto& registry = renderDevice.GetRegistry();

		std::array<uint32_t, 4> textureIds = {
			binding.textures.baseColorTextureId != 0 ?
				binding.textures.baseColorTextureId :
				mDefaultMaterialTextures.baseColorTextureId,
			binding.textures.normalTextureId != 0 ?
				binding.textures.normalTextureId :
				mDefaultMaterialTextures.normalTextureId,
			binding.textures.ormTextureId != 0 ?
				binding.textures.ormTextureId :
				mDefaultMaterialTextures.ormTextureId,
			binding.textures.emissiveTextureId != 0 ?
				binding.textures.emissiveTextureId :
				mDefaultMaterialTextures.emissiveTextureId,
		};
		binding.resolvedTextureIds = textureIds;

		std::array<uint64_t, 4> revisions = {};
		for (size_t i = 0; i < textureIds.size(); ++i) {
			revisions[i] = registry.GetSrvRevision(textureIds[i]);
		}

		const bool needsCreate = !binding.materialTextureTable.IsValid();
		const bool needsUpdate =
			needsCreate ||
			binding.materialTextureSrvRevisions != revisions;
		if (!needsUpdate) {
			return;
		}

		if (needsCreate) {
			binding.materialTextureTable =
				registry.CreateSrvDescriptorTable(
					textureIds,
					"MaterialTextureTable"
				);
		} else {
			registry.UpdateSrvDescriptorTable(
				binding.materialTextureTable,
				textureIds,
				"MaterialTextureTable"
			);
		}
		binding.materialTextureSrvRevisions = revisions;
	}

	bool Renderer::ResolveRegisteredPipelines(
		RenderDevice& renderDevice, const PIPELINE_RESOLVE_SCOPE scope
	) {
		const PipelineResolveResult result = mPipelineRegistry.ResolveAll(
			renderDevice, scope
		);

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
		mShadowDepthFrontCullPass.resolved = mPipelineRegistry.GetGraphics(
			mShadowDepthFrontCullPass.pipeline
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

		if (result.newlyFailedCount != 0) {
			Error(
				kRenderChannel,
				"Pipeline resolution failed: resolved={}/{}",
				result.resolvedCount,
				result.requestedCount
			);
		}
		return result.Succeeded();
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

		const uint64_t bufferSize =
			sizeof(DebugLineVertex) * static_cast<uint64_t>(mLinePass.
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
			kMaxDebugLines
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
