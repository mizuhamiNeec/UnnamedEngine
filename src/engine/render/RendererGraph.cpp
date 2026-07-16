#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

#include "RenderDevice.h"
#include "Renderer.h"

#include "core/math/Math.h"
#include "core/string/StrUtil.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "rendergraph/RenderGraphBuilder.h"
#include "rendergraph/RenderPassContext.h"

#include "shaders/RootSignatureSlots.h"

// ReSharper disable CppRedundantCastExpression

namespace Unnamed::Render {
	namespace {
		Rhi::FrameConstants BuildSceneFrameConstants(
			const RenderCameraInput& camera,
			const uint32_t           width,
			const uint32_t           height,
			const float              time
		) {
			Rhi::FrameConstants frame  = {};
			const float         aspect = height > 0 ?
				                     static_cast<float>(width) /
				                     static_cast<float>(height) :
				                     16.0f / 9.0f;
			const Mat4 fallbackView = Mat4::identity;
			const Mat4 fallbackProj = Mat4::PerspectiveFovD3D(
				90.0f * Math::deg2Rad,
				aspect,
				0.001f,
				10000.0f,
				ProjectionDepthMode::ReverseZ
			);
			frame.view      = camera.valid ? camera.view : fallbackView;
			frame.proj      = camera.valid ? camera.proj : fallbackProj;
			frame.viewProj  = frame.view * frame.proj;
			frame.cameraPos = camera.valid ? camera.cameraPos : Vec3::zero;
			frame.time      = time;
			return frame;
		}

		struct DirectionalShadowMatrices {
			Mat4 lightView         = Mat4::identity;
			Mat4 lightProj         = Mat4::identity;
			Mat4 lightViewProj     = Mat4::identity;
			Vec3 lightRayDirection = Vec3(0.0f, -1.0f, 0.0f);
			Vec3 directionToLight  = Vec3(0.0f, 1.0f, 0.0f);
		};

		DirectionalShadowMatrices BuildDirectionalShadowMatrices(
			const RenderCameraInput&     camera,
			const DirectionalLightInput& light
		) {
			Vec3 lightRayDirection = light.lightRayDirection;
			if (lightRayDirection.IsZero()) {
				lightRayDirection = Vec3(0.0f, -1.0f, 0.0f);
			}
			lightRayDirection = lightRayDirection.Normalized();
			const Vec3 directionToLight = lightRayDirection * -1.0f;
			const Vec3 center = camera.valid ? camera.cameraPos : Vec3::zero;
			const float shadowDistance = Math::HtoM(8192);
			const float orthoHalfSize = Math::HtoM(4096);
			const Vec3 eye = center - lightRayDirection * shadowDistance;
			DirectionalShadowMatrices result;
			result.lightRayDirection = lightRayDirection;
			result.directionToLight  = directionToLight;
			result.lightView         = Mat4::LookAtView(eye, center, Vec3::up);
			result.lightProj         = Mat4::OrthographicD3D(
				-orthoHalfSize,
				orthoHalfSize,
				orthoHalfSize,
				-orthoHalfSize,
				0.1f,
				shadowDistance * 2.0f,
				ProjectionDepthMode::ReverseZ
			);
			result.lightViewProj = result.lightView * result.lightProj;
			return result;
		}

		struct PostFxParamsConstants {
			Vec4 scalar0 = Vec4::zero;
			Vec4 scalar1 = Vec4::zero;
			Vec4 color0  = Vec4::one;
			Vec4 color1  = Vec4::zero;
		};

		struct BloomPyramidConstants {
			Vec4 params0 = Vec4::zero;
			// x=invSrcW, y=invSrcH, z=threshold, w=knee
			Vec4 params1 = Vec4::zero;
			// x=radius, y=intensity, z=firstPass
		};

		struct FitRect {
			float x      = 0.0f;
			float y      = 0.0f;
			float width  = 1.0f;
			float height = 1.0f;
		};

		[[nodiscard]] FitRect ComputeAspectFitRect(
			const uint32_t dstWidth,
			const uint32_t dstHeight,
			const uint32_t srcWidth,
			const uint32_t srcHeight
		) {
			const float safeDstWidth = static_cast<float>(
				std::max(1u, dstWidth));
			const float safeDstHeight = static_cast<float>(std::max(
				1u, dstHeight));
			const float safeSrcWidth = static_cast<float>(
				std::max(1u, srcWidth));
			const float safeSrcHeight = static_cast<float>(std::max(
				1u, srcHeight));
			const float srcAspect = safeSrcWidth / safeSrcHeight;

			float fitWidth  = safeDstWidth;
			float fitHeight = fitWidth / srcAspect;
			if (fitHeight > safeDstHeight) {
				fitHeight = safeDstHeight;
				fitWidth  = fitHeight * srcAspect;
			}

			FitRect rect;
			rect.width  = std::max(1.0f, fitWidth);
			rect.height = std::max(1.0f, fitHeight);
			rect.x      = (safeDstWidth - rect.width) * 0.5f;
			rect.y      = (safeDstHeight - rect.height) * 0.5f;
			return rect;
		}

		std::string CompactLowerKey(const std::string_view key) {
			std::string result;
			result.reserve(key.size());
			for (const unsigned char ch : key) {
				if (ch == '_' || ch == '-' || ch == '.' || ch == ' ') {
					continue;
				}
				result.push_back(static_cast<char>(std::tolower(ch)));
			}
			return result;
		}

		const PostFxPassOverride* FindPostFxPassOverride(
			const std::string_view                 passName,
			const std::vector<PostFxPassOverride>& overrides
		) {
			for (const auto& passOverride : overrides) {
				if (StrUtil::EqualsIgnoreCase(passOverride.passName,
				                              passName)) {
					return &passOverride;
				}
			}
			return nullptr;
		}

		float* ResolveScalarComponent(
			PostFxParamsConstants& params, const int vecIndex,
			const char             component
		) {
			Vec4* vec;
			switch (vecIndex) {
				case 0: vec = &params.scalar0;
					break;
				case 1: vec = &params.scalar1;
					break;
				default: return nullptr;
			}
			switch (component) {
				case 'x': return &vec->x;
				case 'y': return &vec->y;
				case 'z': return &vec->z;
				case 'w': return &vec->w;
				default: return nullptr;
			}
		}

		void ApplyScalarParam(
			const std::string_view name, const float value,
			PostFxParamsConstants& outParams
		) {
			// Common naming convention:
			// Intensity/Threshold/Radius/Amount -> scalar0.xyzw
			// Exposure/Saturation/Contrast/Gamma -> scalar1.xyzw
			// Scalar0X .. Scalar1W (or S0X .. S1W) -> explicit component mapping.
			const std::string key = CompactLowerKey(name);
			if (key.empty()) {
				return;
			}
			if (key == "intensity") {
				outParams.scalar0.x = value;
				return;
			}
			if (key == "threshold") {
				outParams.scalar0.y = value;
				return;
			}
			if (key == "radius") {
				outParams.scalar0.z = value;
				return;
			}
			if (key == "amount") {
				outParams.scalar0.w = value;
				return;
			}
			if (key == "knee") {
				outParams.scalar0.w = value;
				return;
			}
			if (key == "exposure") {
				outParams.scalar1.x = value;
				return;
			}
			if (key == "saturation") {
				outParams.scalar1.y = value;
				return;
			}
			if (key == "contrast") {
				outParams.scalar1.z = value;
				return;
			}
			if (key == "gamma") {
				outParams.scalar1.w = value;
				return;
			}
			// FXAA向けのパラメータ名を直接受け付ける。
			if (key == "edgethreshold") {
				outParams.scalar0.x = value;
				return;
			}
			if (key == "edgethresholdmin") {
				outParams.scalar0.y = value;
				return;
			}
			if (key == "subpixelblending" || key == "subpix") {
				outParams.scalar0.z = value;
				return;
			}
			if (key == "maxspan") {
				outParams.scalar0.w = value;
				return;
			}

			if (key.starts_with("scalar") && key.size() >= 8) {
				const int  vecIndex  = key[6] - '0';
				const char component = key[7];
				if (float* dst = ResolveScalarComponent(
						outParams, vecIndex, component
					);
					dst) {
					*dst = value;
				}
				return;
			}

			if (key.size() == 3 && key[0] == 's' && std::isdigit(key[1])) {
				const int  vecIndex  = key[1] - '0';
				const char component = key[2];
				if (float* dst = ResolveScalarComponent(
						outParams, vecIndex, component
					);
					dst) {
					*dst = value;
				}
			}
		}

		void ApplyColorParam(
			const std::string_view name, const Vec4& value,
			PostFxParamsConstants& outParams
		) {
			// Common naming convention:
			// Tint/Color/Color0 -> color0, Color1/SecondaryColor -> color1.
			const std::string key = CompactLowerKey(name);
			if (key.empty()) {
				return;
			}
			if (key == "tint" || key == "color" || key == "color0") {
				outParams.color0 = value;
				return;
			}
			if (key == "color1" || key == "secondarycolor") {
				outParams.color1 = value;
			}
		}
	}

	void Renderer::BuildGraph(
		RenderDevice&                       renderDevice,
		const std::vector<RenderViewInput>& frameViews
	) {
		mGraphBuilt = true;
		mGraph.SetRenderDevice(renderDevice);

		for (const RenderViewInput& view : frameViews) {
			auto& state = mViewStates[view.viewKey];
			if (state.type == RENDER_VIEW_TYPE::SCENE) {
				if (state.colorTextureId == 0) {
					state.colorTextureId = mGraph.CreateTexture(
						{
							.width          = state.allocatedWidth,
							.height         = state.allocatedHeight,
							.resourceFormat = kSceneHdrColorFormat,
							.allowUav       = false,
							.allowRtv       = true,
							.debugName      = "ViewColor_" + view.viewKey,
							.extentMode     = RG_EXTENT_MODE::FIXED,
						}
					);
				}
				if (state.postFxTextureAId == 0) {
					state.postFxTextureAId = mGraph.CreateTexture(
						{
							.width          = state.allocatedWidth,
							.height         = state.allocatedHeight,
							.resourceFormat = kSceneHdrColorFormat,
							.allowRtv       = true,
							.debugName      = "ViewPostFxA_" + view.viewKey,
							.extentMode     = RG_EXTENT_MODE::FIXED,
						}
					);
				}
				if (state.postFxTextureBId == 0) {
					state.postFxTextureBId = mGraph.CreateTexture(
						{
							.width          = state.allocatedWidth,
							.height         = state.allocatedHeight,
							.resourceFormat = kSceneHdrColorFormat,
							.allowRtv       = true,
							.debugName      = "ViewPostFxB_" + view.viewKey,
							.extentMode     = RG_EXTENT_MODE::FIXED,
						}
					);
				}
				if (state.outputTextureId == 0) {
					state.outputTextureId = mGraph.CreateTexture(
						{
							.width          = state.allocatedWidth,
							.height         = state.allocatedHeight,
							.resourceFormat = kSceneLdrColorFormat,
							.allowRtv       = true,
							.debugName      = "ViewOutputLdr_" + view.viewKey,
							.extentMode     = RG_EXTENT_MODE::FIXED,
						}
					);
				}

				// TODO: vectorのリサイズって重いんかな?重そうだな...
				const size_t requestedBloomMipCount = static_cast<size_t>(
					std::max(
						mConsole->GetConVarValueOr("post_bloommipcount", 5),
						1
					));
				if (state.bloomMipTextureIds.size() > requestedBloomMipCount) {
					auto& registry = renderDevice.GetRegistry();
					for (size_t i = requestedBloomMipCount;
					     i < state.bloomMipTextureIds.size();
					     ++i) {
						if (state.bloomMipTextureIds[i] != 0) {
							registry.
								ReleaseTexture(state.bloomMipTextureIds[i]);
						}
					}
				}
				state.bloomMipTextureIds.resize(requestedBloomMipCount);

				for (uint32_t i = 0; i < state.bloomMipTextureIds.size(); ++i) {
					if (state.bloomMipTextureIds[i] != 0) {
						continue;
					}
					const uint32_t bloomWidth = std::max(
						1u,
						state.allocatedWidth >> static_cast<uint32_t>(i + 1)
					);
					const uint32_t bloomHeight = std::max(
						1u,
						state.allocatedHeight >> static_cast<uint32_t>(i + 1)
					);
					state.bloomMipTextureIds[i] = mGraph.CreateTexture(
						{
							.width          = bloomWidth,
							.height         = bloomHeight,
							.resourceFormat = kSceneHdrColorFormat,
							.allowRtv       = true,
							.debugName      =
							"ViewBloomMip" + std::to_string(i) + "_" +
							view.viewKey,
							.extentMode = RG_EXTENT_MODE::FIXED,
						}
					);
				}
				if (state.depthTextureId == 0) {
					state.depthTextureId = mGraph.CreateTexture(
						{
							.width = state.allocatedWidth,
							.height = state.allocatedHeight,
							.resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS,
							.allowDsv = true,
							.srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
							.dsvFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
							.debugName = "ViewDepth_" + view.viewKey,
							.optimizedClearDepth = 0.0f,
							.optimizedClearStencil = 0,
							.extentMode = RG_EXTENT_MODE::FIXED,
						}
					);
				}
			} else if (state.outputTextureId == 0) {
				state.outputTextureId = mGraph.CreateTexture(
					{
						.width          = state.allocatedWidth,
						.height         = state.allocatedHeight,
						.resourceFormat = kSceneLdrColorFormat,
						.allowRtv       = true,
						.debugName      = "SpriteOnly_" + view.viewKey,
						.extentMode     = RG_EXTENT_MODE::FIXED,
					}
				);
			}
		}

		size_t firstSceneViewIndex = frameViews.size();
		for (size_t i = 0; i < frameViews.size(); ++i) {
			if (frameViews[i].type == RENDER_VIEW_TYPE::SCENE) {
				firstSceneViewIndex = i;
				break;
			}
		}
		mDirectionalShadow.enabled = false;
		if (
			firstSceneViewIndex < frameViews.size() &&
			frameViews[firstSceneViewIndex].directionalLight.enabled &&
			frameViews[firstSceneViewIndex].directionalLight.castsShadow &&
			frameViews[firstSceneViewIndex].directionalLight.intensity > 0.0f
		) {
			const DirectionalLightInput& shadowLight =
				frameViews[firstSceneViewIndex].directionalLight;
			const int requestedShadowSize = mConsole ?
				                                mConsole->GetConVarValueOr(
					                                "r_shadowmap_size",
					                                1024
				                                ) :
				                                1024;
			const uint32_t shadowResolution = static_cast<uint32_t>(
				std::clamp(requestedShadowSize, 256, 16385)
			);
			if (mDirectionalShadow.resolution != shadowResolution) {
				if (mDirectionalShadow.shadowDepthTextureId != 0) {
					renderDevice.GetRegistry().ReleaseTexture(
						mDirectionalShadow.shadowDepthTextureId
					);
					mDirectionalShadow.shadowDepthTextureId = 0;
				}
				mDirectionalShadow.resolution = shadowResolution;
			}
			if (mDirectionalShadow.shadowDepthTextureId == 0) {
				mDirectionalShadow.shadowDepthTextureId = mGraph.CreateTexture(
					{
						.width               = shadowResolution,
						.height              = shadowResolution,
						.resourceFormat      = DXGI_FORMAT_R32_TYPELESS,
						.allowDsv            = true,
						.srvFormat           = DXGI_FORMAT_R32_FLOAT,
						.dsvFormat           = DXGI_FORMAT_D32_FLOAT,
						.debugName           = "DirectionalShadowMap",
						.optimizedClearDepth = 0.0f,
						.extentMode          = RG_EXTENT_MODE::FIXED,
					}
				);
			}
			const DirectionalShadowMatrices shadowMatrices =
				BuildDirectionalShadowMatrices(
					frameViews[firstSceneViewIndex].camera,
					shadowLight
				);
			mDirectionalShadow.enabled           = true;
			mDirectionalShadow.lightView         = shadowMatrices.lightView;
			mDirectionalShadow.lightProj         = shadowMatrices.lightProj;
			mDirectionalShadow.lightViewProj     = shadowMatrices.lightViewProj;
			mDirectionalShadow.lightRayDirection =
				shadowMatrices.lightRayDirection;
			mDirectionalShadow.directionToLight =
				shadowMatrices.directionToLight;
			mDirectionalShadow.color     = shadowLight.color;
			mDirectionalShadow.intensity = shadowLight.intensity;
		}

		for (size_t viewIndex = 0; viewIndex < frameViews.size(); ++viewIndex) {
			const RenderViewInput& view = frameViews[viewIndex];
			const auto             state = mViewStates[view.viewKey];
			const std::string      prefix = "View[" + view.viewKey + "] ";
			const auto             CollectTextureIds = [this, &renderDevice](
				const auto& sprites
			) {
				std::vector<uint32_t> ids;
				ids.reserve(sprites.size());
				std::unordered_set<uint32_t> seen;
				seen.reserve(sprites.size());
				for (const auto& sprite : sprites) {
					const uint32_t textureId =
						ResolveSpriteTexture(renderDevice, sprite.texture);
					if (textureId == 0) {
						continue;
					}
					if (seen.insert(textureId).second) {
						ids.emplace_back(textureId);
					}
				}
				return ids;
			};
			const std::vector<uint32_t> worldBillboardTextureIds =
				CollectTextureIds(view.worldBillboards);
			const std::vector<uint32_t> worldSpriteTextureIds =
				CollectTextureIds(
					view.worldSprites
				);
			const std::vector<uint32_t> screenSpriteTextureIds =
				CollectTextureIds(
					view.screenSprites
				);
			const uint32_t skyboxTextureId =
				view.type == RENDER_VIEW_TYPE::SCENE && view.skybox.enabled ?
					EnsureSkyboxTextureLoaded(
						renderDevice, view.skybox.textureAssetId
					) :
					0;

			const uint32_t colorId  = state.colorTextureId;
			const uint32_t depthId  = state.depthTextureId;
			uint32_t       outputId = state.outputTextureId;
			if (view.type == RENDER_VIEW_TYPE::SCENE) {
				if (
					viewIndex == firstSceneViewIndex &&
					mDirectionalShadow.enabled &&
					mDirectionalShadow.shadowDepthTextureId != 0
				) {
					AddShadowMapPass(
						renderDevice,
						viewIndex,
						mDirectionalShadow
					);
				}
				AddSceneClearPass(prefix, colorId, depthId);

				AddSkyboxPass(
					renderDevice, prefix, viewIndex, state, skyboxTextureId
				);
				AddGeometryPass(renderDevice, prefix, viewIndex, state);
				AddWorldBillboardDepthPass(
					renderDevice,
					prefix,
					viewIndex,
					state,
					worldBillboardTextureIds
				);
				AddWorldSpritePass(
					renderDevice,
					prefix,
					viewIndex,
					state,
					worldSpriteTextureIds
				);
				AddDebugLinePass(renderDevice, prefix, viewIndex, state);
				AddWorldBillboardFrontPass(
					renderDevice,
					prefix,
					viewIndex,
					state,
					worldBillboardTextureIds
				);
				AddScenePostProcessPasses(
					renderDevice, prefix, viewIndex, state, outputId
				);
			}

			if (view.type == RENDER_VIEW_TYPE::SPRITE_ONLY) {
				AddSpriteOnlyClearPass(prefix, outputId);
			}

			AddScreenSpritePass(
				renderDevice,
				prefix,
				viewIndex,
				state,
				outputId,
				screenSpriteTextureIds
			);

			mViewStates[view.viewKey].outputTextureId = outputId;
		}

		AddPrepareUiViewOutputsPass(frameViews);

		if (!mPresentViewKey.empty()) {
			AddPresentPass(renderDevice);
		} else {
			AddEditorBackBufferClearPass(frameViews);
		}

		AddShadowMapDebugPass(renderDevice);
		AddImGuiMainPass();
	}

	void Renderer::AddSceneClearPass(
		const std::string& prefix,
		const uint32_t     colorId,
		const uint32_t     depthId
	) {
		// パス: シーン クリア。
		// 入力: なし。
		// 出力: colorId と depthId がクリアされる。
		// PSO: なし。
		// ルートシグネチャ: なし。
		// レンダーターゲット: colorId。
		// 深度ステンシル: depthId。
		// ディスクリプタヒープ: なし。
		// リソースステート: WriteRt(colorId), WriteDepth(depthId)。
		// 注記: 既存のクリアカラーと逆Z深度クリア値を保持する。
		mGraph.AddPass(
			prefix + "Clear",
			[colorId, depthId](RenderGraphBuilder& b) {
				b.WriteRt(colorId);
				b.WriteDepth(depthId);
				if (true) {
					b.ClearColor(colorId, 0.1f, 0.1f, 0.2f, 1.0f);
				}
				b.ClearDepth(depthId, 0.0f, 0);
			},
			[](RenderPassContext&) {
			}
		);
	}

	void Renderer::AddSkyboxPass(
		RenderDevice&           renderDevice,
		const std::string&      prefix,
		const size_t            viewIndex,
		const ViewRuntimeState& state,
		const uint32_t          skyboxTextureId
	) {
		// パス: スカイボックス。
		// 入力: RenderViewInput::camera、skyboxTextureId SRV、スカイボックス cube VB/IB。
		// 出力: state.colorTextureId 内のスカイボックスピクセルと深度書き込み。
		// PSO: mSkyboxPass.geom.resolved->pso。
		// ルートシグネチャ: mSkyboxPass.geom.resolved->rootSignature (Geom)。
		// レンダーターゲット: state.colorTextureId。
		// 深度ステンシル: state.depthTextureId。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: WriteRt(state.colorTextureId), WriteDepth(state.depthTextureId), ReadSrvPs(skyboxTextureId 非ゼロ時)。
		// 注記: スカイボックスの強度として MaterialConstants::baseColor を使用する。
		const uint32_t colorId = state.colorTextureId;
		const uint32_t depthId = state.depthTextureId;
		mGraph.AddPass(
			prefix + "Skybox",
			[colorId, depthId, skyboxTextureId](RenderGraphBuilder& b) {
				b.WriteRt(colorId);
				b.WriteDepth(depthId);
				if (skyboxTextureId != 0) {
					b.ReadSrvPs(skyboxTextureId);
				}
			},
			[this, viewIndex, state, skyboxTextureId, &renderDevice](
			const RenderPassContext& pass
		) {
				const RenderViewInput& view = mFrameViews[viewIndex];
				if (!view.skybox.enabled || skyboxTextureId == 0) {
					return;
				}
				if (
					!mSkyboxPass.geom.resolved ||
					!mSkyboxPass.geom.resolved->pso ||
					!mSkyboxPass.geom.vb ||
					!mSkyboxPass.geom.ib
				) {
					return;
				}

				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				const Rhi::FrameConstants frame = BuildSceneFrameConstants(
					view.camera, state.logicalWidth,
					state.logicalHeight, 0.0f
				);
				Rhi::ObjectConstants object;
				object.world                 = Mat4::identity;
				object.world.m[3][0]         = frame.cameraPos.x;
				object.world.m[3][1]         = frame.cameraPos.y;
				object.world.m[3][2]         = frame.cameraPos.z;
				object.worldInverseTranspose =
					object.world.Inverse().Transpose();
				object.skinningInfo = Vec4::zero;

				Rhi::MaterialConstants material = {};
				material.baseColor              = Vec4(
					view.skybox.intensity,
					view.skybox.intensity,
					view.skybox.intensity,
					1.0f
				);
				material.opacity    = 1.0f;
				material.domainMode = 0.0f;

				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(
						&frame, sizeof(frame)
					);
				const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
					allocator.AllocateConstantBuffer(
						&object, sizeof(object)
					);
				const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
					allocator.AllocateConstantBuffer(
						&material, sizeof(material)
					);

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTargetAndDepth(
					std::span<const uint32_t>(&state.colorTextureId, 1),
					state.depthTextureId
				);
				pass.SetGraphicsPipeline(
					mSkyboxPass.geom.resolved->rootSignature,
					mSkyboxPass.geom.resolved->pso
				);
				pass.SetVertexBuffer(mSkyboxPass.geom.vbv);
				pass.SetIndexBuffer(mSkyboxPass.geom.ibv);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::OBJECT), objectCb
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::MATERIAL), materialCb
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::SKINNING), objectCb
				);
				pass.BindGraphicsSrvTable(
					ToRootIndex(GEOM_ROOT_SLOT::BASE_COLOR_TEXTURE),
					skyboxTextureId
				);
				pass.DrawIndexedTest(mSkyboxPass.geom.indexCount);
			}
		);
	}

	void Renderer::AddGeometryPass(
		RenderDevice&           renderDevice,
		const std::string&      prefix,
		size_t                  viewIndex,
		const ViewRuntimeState& state
	) {
		// パス: ジオメトリ。
		// 入力: RenderViewInput::visibleObjects、シーンメッシュバッファ、マテリアルバインディング、フォールバックテクスチャ。
		// 出力: シーンビューのメッシュカラー/深度書き込み。
		// PSO: 利用可能時は MaterialBinding::resolvedGeometryPipeline、それ以外は mGeometryPass.resolved->pso フォールバック。
		// ルートシグネチャ: Geom ルートシグネチャ、マテリアルシェーダは互換性があると想定される。
		// レンダーターゲット: state.colorTextureId。
		// 深度ステンシル: state.depthTextureId。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: WriteRt(state.colorTextureId), WriteDepth(state.depthTextureId), ReadSrvPs(有効時の mDirectionalShadow.shadowDepthTextureId)。 マテリアルテクスチャ SRV は MaterialBinding/フォールバックテクスチャを通じた描画時にバインドされる。
		// 注記: 静的メッシュとスキンメッシュはこのパスを共有する。不透明マテリアルシェーダ/深度/カルバリアントは描画ごとに選択される。透明/ブレンドマテリアル処理は意図的に未対応のまま。
		const uint32_t colorId       = state.colorTextureId;
		const uint32_t depthId       = state.depthTextureId;
		const uint32_t shadowDepthId =
			mDirectionalShadow.shadowDepthTextureId;
		const bool viewLightEnabled =
			viewIndex < mFrameViews.size() &&
			mFrameViews[viewIndex].directionalLight.enabled &&
			mFrameViews[viewIndex].directionalLight.intensity > 0.0f;
		const bool shadowEnabled =
			viewLightEnabled &&
			mDirectionalShadow.enabled &&
			shadowDepthId != 0 &&
			(!mConsole || mConsole->GetConVarValueOr(
				 "r_shadowmap_enabled",
				 true
			 ));

		mGraph.AddPass(
			prefix + "Geometry",
			[colorId, depthId, shadowDepthId, shadowEnabled](
			RenderGraphBuilder& b
		) {
				b.WriteRt(colorId);
				b.WriteDepth(depthId);
				if (shadowEnabled) {
					b.ReadSrvPs(shadowDepthId);
				}
			},
			[this, viewIndex, state, shadowDepthId, shadowEnabled,
				&renderDevice](
			RenderPassContext& pass
		) {
				const RenderViewInput& view = mFrameViews[viewIndex];
				if (view.visibleObjects.empty()) {
					return;
				}

				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				Rhi::FrameConstants frame = BuildSceneFrameConstants(
					view.camera, state.logicalWidth,
					state.logicalHeight, 0.0f
				);
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(
						&frame, sizeof(frame)
					);

				Rhi::SkinningPaletteConstants identityPalette = {};
				for (auto& bone : identityPalette.bones) {
					bone = Mat4::identity;
				}
				const D3D12_GPU_VIRTUAL_ADDRESS identitySkinCb =
					allocator.AllocateConstantBuffer(
						&identityPalette, sizeof(identityPalette)
					);

				Rhi::ShadowConstants shadow = {};
				shadow.lightViewProj = mDirectionalShadow.lightViewProj;
				const DirectionalLightInput& light = view.directionalLight;
				const bool                   directLightEnabled =
					light.enabled && light.intensity > 0.0f;
				shadow.params = Vec4(
					mConsole ?
						mConsole->GetConVarValueOr(
							"r_shadowmap_bias", 0.0005f
						) :
						0.0005f,
					mConsole ?
						mConsole->GetConVarValueOr(
							"r_shadowmap_strength", 0.65f
						) :
						0.65f,
					mDirectionalShadow.resolution > 0 ?
						1.0f / static_cast<float>(
							mDirectionalShadow.resolution) :
						0.0f,
					shadowEnabled ? 1.0f : 0.0f
				);
				shadow.filterParams = Vec4(
					mConsole && mConsole->GetConVarValueOr(
						"r_shadowmap_pcf_enabled", true
					) ?
						1.0f :
						0.0f,
					mConsole ?
						mConsole->GetConVarValueOr(
							"r_shadowmap_pcf_radius", 1.0f
						) :
						1.0f,
					mConsole ?
						mConsole->GetConVarValueOr(
							"r_shadowmap_normal_bias", 0.0f
						) :
						0.0f,
					0.0f
				);
				const Vec3 directionToLight =
					directLightEnabled ?
						light.directionToLight.Normalized() :
						Vec3(0.0f, 1.0f, 0.0f);
				shadow.directionToLight = Vec4(
					directionToLight.x,
					directionToLight.y,
					directionToLight.z,
					0.0f
				);
				shadow.lightColorIntensity = Vec4(
					light.color.x,
					light.color.y,
					light.color.z,
					directLightEnabled ? light.intensity : 0.0f
				);
				const D3D12_GPU_VIRTUAL_ADDRESS shadowCb =
					allocator.AllocateConstantBuffer(
						&shadow, sizeof(shadow)
					);

				Rhi::EnvironmentLightingConstants environment      = {};
				const EnvironmentLightInput&      environmentLight =
					view.environmentLight;
				environment.skyAmbientColor = Vec4(
					environmentLight.skyColor.x,
					environmentLight.skyColor.y,
					environmentLight.skyColor.z,
					1.0f
				);
				environment.groundAmbientColor = Vec4(
					environmentLight.groundColor.x,
					environmentLight.groundColor.y,
					environmentLight.groundColor.z,
					1.0f
				);
				environment.params = Vec4(
					environmentLight.enabled ?
						std::max(0.0f, environmentLight.intensity) :
						0.0f,
					0.0f,
					0.0f,
					0.0f
				);
				const D3D12_GPU_VIRTUAL_ADDRESS environmentCb =
					allocator.AllocateConstantBuffer(
						&environment, sizeof(environment)
					);

				uint32_t shadowSrvTextureId = shadowEnabled ?
					                              shadowDepthId :
					                              0;
				if (shadowSrvTextureId == 0) {
					EnsureSpriteFallbackTexture(renderDevice);
					shadowSrvTextureId = mSpriteFallbackTextureId;
				}

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTargetAndDepth(
					std::span<const uint32_t>(&state.colorTextureId, 1),
					state.depthTextureId
				);
				if (!mGeometryPass.resolved || !mGeometryPass.resolved->
				    pso) {
					return;
				}
				const ResolvedGraphicsPipeline* currentGeometryPipeline =
					nullptr;
				auto BindGeometryPipeline = [&](
					const MaterialBinding*
					materialBinding
				) {
					const ResolvedGraphicsPipeline* pipeline =
						mGeometryPass.resolved;
					if (
						materialBinding &&
						materialBinding->resolvedGeometryPipeline &&
						materialBinding->resolvedGeometryPipeline->pso
					) {
						pipeline = materialBinding->resolvedGeometryPipeline;
					}
					if (!pipeline || !pipeline->pso) {
						return false;
					}
					if (currentGeometryPipeline != pipeline) {
						pass.SetGraphicsPipeline(
							pipeline->rootSignature,
							pipeline->pso
						);
						currentGeometryPipeline = pipeline;
					}
					return true;
				};
				const auto BindSceneLightingInputs = [&] {
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::SHADOW_CONSTANTS),
						shadowCb
					);
					pass.BindGraphicsSrvTable(
						ToRootIndex(GEOM_ROOT_SLOT::SHADOW_MAP),
						shadowSrvTextureId
					);
					pass.BindGraphicsCbv(
						ToRootIndex(
							GEOM_ROOT_SLOT::ENVIRONMENT_LIGHTING),
						environmentCb
					);
				};

				const MaterialBinding* fallbackMaterial = nullptr;
				if (const auto it = mMaterialBindings.find(
						mDefaultMaterialInstance
					);
					it != mMaterialBindings.end()) {
					fallbackMaterial = &it->second;
				}
				auto ResolveMaterialTextureTable = [&renderDevice](
					const MaterialBinding* materialBinding
				) {
					if (
						materialBinding &&
						materialBinding->materialTextureTable.IsValid()
					) {
						return renderDevice.GetRegistry().
						                    GetSrvDescriptorTableGpu(
							                    materialBinding->
							                    materialTextureTable
						                    );
					}
					return D3D12_GPU_DESCRIPTOR_HANDLE{};
				};

				for (const auto& objectInput : view.visibleObjects) {
					const auto meshIt = mSceneMeshesByAsset.find(
						objectInput.meshAssetId
					);
					if (meshIt == mSceneMeshesByAsset.end()) {
						continue;
					}
					const MeshBuffer& mesh = meshIt->second;

					Rhi::ObjectConstants object  = {};
					object.world                 = objectInput.world;
					object.worldInverseTranspose =
						object.world.Inverse().Transpose();
					object.skinningInfo = Vec4(
						0.0f, objectInput.isSkinned ? 1.0f : 0.0f, 0.0f,
						0.0f
					);

					Rhi::SkinningPaletteConstants skinPalette =
						identityPalette;
					if (
						objectInput.isSkinned &&
						objectInput.skeletonPaletteId < view.
						skinningPalettes.size()
					) {
						const auto& sourcePalette = view.
							skinningPalettes[
								objectInput.skeletonPaletteId];
						const uint32_t maxBones = std::min<uint32_t>(
							static_cast<uint32_t>(sourcePalette.
							                      boneMatrices.size()),
							Rhi::SkinningPaletteConstants::kMaxBones
						);
						for (uint32_t i = 0; i < maxBones; ++i) {
							skinPalette.bones[i] = sourcePalette.
								boneMatrices[i];
						}
					}

					const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
						allocator.AllocateConstantBuffer(
							&object, sizeof(object)
						);
					const D3D12_GPU_VIRTUAL_ADDRESS skinningCb =
						objectInput.isSkinned ?
							allocator.AllocateConstantBuffer(
								&skinPalette, sizeof(skinPalette)
							) :
							identitySkinCb;

					pass.SetVertexBuffer(mesh.vbv);
					pass.SetIndexBuffer(mesh.ibv);

					if (mesh.submeshes.empty()) {
						Rhi::MaterialConstants material        = {};
						const MaterialBinding* materialBinding =
							fallbackMaterial;
						if (const auto matIt = mMaterialBindings.find(
								objectInput.materialInstanceId
							);
							matIt != mMaterialBindings.end()) {
							materialBinding = &matIt->second;
						}
						if (materialBinding) {
							material = materialBinding->constants;
						}
						const auto materialTextureTable =
							ResolveMaterialTextureTable(materialBinding);
						if (materialTextureTable.ptr == 0) {
							continue;
						}

						const D3D12_GPU_VIRTUAL_ADDRESS
							materialCbFallback =
								allocator.AllocateConstantBuffer(
									&material, sizeof(material)
								);
						if (!BindGeometryPipeline(materialBinding)) {
							continue;
						}
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::OBJECT), objectCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::SKINNING),
							skinningCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::MATERIAL),
							materialCbFallback
						);
						pass.BindGraphicsSrvTable(
							ToRootIndex(
								GEOM_ROOT_SLOT::MATERIAL_TEXTURES),
							materialTextureTable
						);
						BindSceneLightingInputs();
						pass.DrawIndexedTest(mesh.indexCount);
						continue;
					}

					for (const auto& submesh : mesh.submeshes) {
						if (submesh.indexCount == 0) {
							continue;
						}

						AssetID submeshMaterialId = objectInput.
							materialInstanceId;
						if (submesh.materialIndex < objectInput.
						                            materialInstanceIdsBySlot.
						                            size()) {
							const AssetID slotMaterialId = objectInput.
								materialInstanceIdsBySlot[
									submesh.materialIndex
								];
							if (slotMaterialId != kInvalidAssetID) {
								submeshMaterialId = slotMaterialId;
							}
						}

						Rhi::MaterialConstants material        = {};
						const MaterialBinding* materialBinding =
							fallbackMaterial;
						if (const auto matIt = mMaterialBindings.find(
								submeshMaterialId
							);
							matIt != mMaterialBindings.end()) {
							materialBinding = &matIt->second;
						}
						if (materialBinding) {
							material = materialBinding->constants;
						}
						const auto materialTextureTable =
							ResolveMaterialTextureTable(materialBinding);
						if (materialTextureTable.ptr == 0) {
							continue;
						}

						const D3D12_GPU_VIRTUAL_ADDRESS
							materialCbSubmesh =
								allocator.AllocateConstantBuffer(
									&material, sizeof(material)
								);
						if (!BindGeometryPipeline(materialBinding)) {
							continue;
						}
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::OBJECT), objectCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::SKINNING),
							skinningCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::MATERIAL),
							materialCbSubmesh
						);
						pass.BindGraphicsSrvTable(
							ToRootIndex(
								GEOM_ROOT_SLOT::MATERIAL_TEXTURES),
							materialTextureTable
						);
						BindSceneLightingInputs();
						pass.DrawIndexedTest(
							submesh.indexCount,
							submesh.indexStart,
							0
						);
					}
				}
			}
		);
	}

	void Renderer::AddShadowMapPass(
		RenderDevice&                        renderDevice,
		const size_t                         viewIndex,
		const DirectionalShadowRuntimeState& shadowState
	) {
		// パス: ディレクショナルシャドウマップ 深度のみ。
		// 入力: 最初のシーンの RenderViewInput::visibleObjects とスキニングパレット。
		// 出力: shadowState.shadowDepthTextureId 深度書き込み。
		// PSO: mShadowDepthPass、mShadowDepthFrontCullPass、または mShadowDepthDoubleSidedPass。
		// ルートシグネチャ: Geom ルートシグネチャ、DepthOnly シェーダは FRAME、OBJECT、SKINNING スロットを使用。
		// レンダーターゲット: なし。
		// 深度ステンシル: shadowState.shadowDepthTextureId。
		// ディスクリプタヒープ: なし。
		// リソースステート: WriteDepth(shadowState.shadowDepthTextureId)。
		// 注記: DirectionalLightInput 由来のライト視点で描画する。r_shadowmap_force_cull_none は室内/内向きメッシュ検証のためすべてのキャスターをダブルサイド PSO で強制できる。
		const uint32_t shadowDepthId = shadowState.shadowDepthTextureId;
		mGraph.AddPass(
			"DirectionalShadowMap",
			[shadowDepthId](RenderGraphBuilder& b) {
				b.WriteDepth(shadowDepthId);
				b.ClearDepth(shadowDepthId, 0.0f, 0);
			},
			[this, viewIndex, shadowState, &renderDevice](
			const RenderPassContext& pass
		) {
				const RenderViewInput& view = mFrameViews[viewIndex];
				if (view.visibleObjects.empty()) {
					return;
				}
				if (
					!mShadowDepthPass.resolved ||
					!mShadowDepthPass.resolved->pso
				) {
					return;
				}

				auto& allocator = static_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();

				Rhi::FrameConstants frame = {};
				frame.view = shadowState.lightView;
				frame.proj = shadowState.lightProj;
				frame.viewProj = shadowState.lightViewProj;
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(&frame, sizeof(frame));

				Rhi::SkinningPaletteConstants identityPalette = {};
				for (auto& bone : identityPalette.bones) {
					bone = Mat4::identity;
				}
				const D3D12_GPU_VIRTUAL_ADDRESS identitySkinCb =
					allocator.AllocateConstantBuffer(
						&identityPalette,
						sizeof(identityPalette)
					);

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(shadowState.resolution),
					static_cast<float>(shadowState.resolution)
				);
				pass.SetRenderTargetAndDepth(
					std::span<const uint32_t>{},
					shadowState.shadowDepthTextureId
				);

				const ResolvedGraphicsPipeline* currentPipeline    = nullptr;
				auto                            bindShadowPipeline = [&](
					const MaterialBinding*
					materialBinding
				) {
					const bool forceCullNone = mConsole &&
					                           mConsole->GetConVarValueOr(
						                           "r_shadowmap_force_cull_none",
						                           false
					                           );
					const GeometryPassRes* passRes = &mShadowDepthPass;
					if (forceCullNone) {
						passRes = &mShadowDepthDoubleSidedPass;
					} else if (materialBinding) {
						switch (materialBinding->renderState.shadowCullMode) {
							case MATERIAL_SHADOW_CULL_MODE::BACK
							: passRes = &mShadowDepthPass;
								break;
							case MATERIAL_SHADOW_CULL_MODE::FRONT
							: passRes = &mShadowDepthFrontCullPass;
								break;
							case MATERIAL_SHADOW_CULL_MODE::NONE
							: passRes = &mShadowDepthDoubleSidedPass;
								break;
							case MATERIAL_SHADOW_CULL_MODE::FOLLOW_MATERIAL:
							default: passRes =
							         materialBinding->renderState.cullBackFace ?
								         &mShadowDepthPass :
								         &mShadowDepthDoubleSidedPass;
								break;
						}
					}
					if (!passRes->resolved || !passRes->resolved->pso) {
						return false;
					}
					if (currentPipeline != passRes->resolved) {
						pass.SetGraphicsPipeline(
							passRes->resolved->rootSignature,
							passRes->resolved->pso
						);
						currentPipeline = passRes->resolved;
					}
					return true;
				};

				auto shouldDrawShadowCaster = [](
					const MaterialBinding*
					materialBinding
				) {
					if (!materialBinding) {
						return true;
					}
					const auto& rs = materialBinding->renderState;
					return rs.castsShadow && rs.depthEnable && rs.depthWrite &&
					       !rs.blendEnable;
				};

				const MaterialBinding* fallbackMaterial = nullptr;
				if (const auto it = mMaterialBindings.find(
						mDefaultMaterialInstance
					);
					it != mMaterialBindings.end()) {
					fallbackMaterial = &it->second;
				}

				for (const auto& objectInput : view.visibleObjects) {
					const auto meshIt = mSceneMeshesByAsset.find(
						objectInput.meshAssetId
					);
					if (meshIt == mSceneMeshesByAsset.end()) {
						continue;
					}
					const MeshBuffer& mesh = meshIt->second;

					Rhi::ObjectConstants object  = {};
					object.world                 = objectInput.world;
					object.worldInverseTranspose =
						object.world.Inverse().Transpose();
					object.skinningInfo = Vec4(
						0.0f,
						objectInput.isSkinned ? 1.0f : 0.0f,
						0.0f,
						0.0f
					);

					Rhi::SkinningPaletteConstants skinPalette =
						identityPalette;
					if (
						objectInput.isSkinned &&
						objectInput.skeletonPaletteId <
						view.skinningPalettes.size()
					) {
						const auto& sourcePalette =
							view.skinningPalettes[
								objectInput.skeletonPaletteId
							];
						const uint32_t maxBones = std::min<uint32_t>(
							static_cast<uint32_t>(
								sourcePalette.boneMatrices.size()
							),
							Rhi::SkinningPaletteConstants::kMaxBones
						);
						for (uint32_t i = 0; i < maxBones; ++i) {
							skinPalette.bones[i] =
								sourcePalette.boneMatrices[i];
						}
					}

					const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
						allocator.AllocateConstantBuffer(
							&object,
							sizeof(object)
						);
					const D3D12_GPU_VIRTUAL_ADDRESS skinningCb =
						objectInput.isSkinned ?
							allocator.AllocateConstantBuffer(
								&skinPalette,
								sizeof(skinPalette)
							) :
							identitySkinCb;

					pass.SetVertexBuffer(mesh.vbv);
					pass.SetIndexBuffer(mesh.ibv);

					auto resolveMaterial = [&](const AssetID materialId) {
						const MaterialBinding* materialBinding =
							fallbackMaterial;
						if (const auto matIt = mMaterialBindings.find(
								materialId
							);
							matIt != mMaterialBindings.end()) {
							materialBinding = &matIt->second;
						}
						return materialBinding;
					};

					auto drawSubmesh = [&](
						const uint32_t indexCount,
						const uint32_t indexStart,
						const AssetID  materialId
					) {
						const MaterialBinding* materialBinding =
							resolveMaterial(materialId);
						if (!shouldDrawShadowCaster(materialBinding)) {
							return;
						}
						if (!bindShadowPipeline(materialBinding)) {
							return;
						}
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::FRAME),
							frameCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::OBJECT),
							objectCb
						);
						pass.BindGraphicsCbv(
							ToRootIndex(GEOM_ROOT_SLOT::SKINNING),
							skinningCb
						);
						pass.DrawIndexedTest(indexCount, indexStart, 0);
					};

					if (mesh.submeshes.empty()) {
						drawSubmesh(
							mesh.indexCount,
							0,
							objectInput.materialInstanceId
						);
						continue;
					}

					for (const auto& submesh : mesh.submeshes) {
						if (submesh.indexCount == 0) {
							continue;
						}
						AssetID submeshMaterialId =
							objectInput.materialInstanceId;
						if (
							submesh.materialIndex <
							objectInput.materialInstanceIdsBySlot.size()
						) {
							const AssetID slotMaterialId =
								objectInput.materialInstanceIdsBySlot[
									submesh.materialIndex
								];
							if (slotMaterialId != kInvalidAssetID) {
								submeshMaterialId = slotMaterialId;
							}
						}
						drawSubmesh(
							submesh.indexCount,
							submesh.indexStart,
							submeshMaterialId
						);
					}
				}
			}
		);
	}

	void Renderer::AddWorldBillboardDepthPass(
		RenderDevice&                renderDevice,
		const std::string&           prefix,
		size_t                       viewIndex,
		const ViewRuntimeState&      state,
		const std::vector<uint32_t>& worldBillboardTextureIds
	) {
		// パス: ワールドビルボード 深度。
		// 入力: depthTest が true の RenderViewInput::worldBillboards、worldBillboardTextureIds SRV。
		// 出力: シーンビューのビルボードカラー/深度書き込み。
		// PSO: mBillboardPass.depthGeom.resolved->pso。
		// ルートシグネチャ: mBillboardPass.depthGeom.resolved->rootSignature (Geom)。
		// レンダーターゲット: state.colorTextureId。
		// 深度ステンシル: state.depthTextureId。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: WriteRt(state.colorTextureId), WriteDepth(state.depthTextureId), ReadSrvPs(worldBillboardTextureIds 各エントリ)。
		// 注記: カメラ向きビルボードを使用し、深度テスト描画フィルタリングを保持。
		const uint32_t colorId = state.colorTextureId;
		const uint32_t depthId = state.depthTextureId;

		mGraph.AddPass(
			prefix + "WorldBillboardDepth",
			[colorId, depthId, worldBillboardTextureIds](
			RenderGraphBuilder& b
		) {
				b.WriteRt(colorId);
				b.WriteDepth(depthId);
				for (const uint32_t texId : worldBillboardTextureIds) {
					b.ReadSrvPs(texId);
				}
			},
			[this, viewIndex, state, &renderDevice](
			const RenderPassContext& pass
		) {
				const RenderViewInput& view = mFrameViews[viewIndex];
				if (view.worldBillboards.empty()) {
					return;
				}

				if (
					!std::ranges::any_of(
						view.worldBillboards,
						[](const WorldBillboardInput& billboard) {
							return billboard.depthTest;
						}
					)) {
					return;
				}

				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				const Rhi::FrameConstants frame = BuildSceneFrameConstants(
					view.camera, state.logicalWidth,
					state.logicalHeight, 0.0f
				);
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(
						&frame, sizeof(frame)
					);

				const Mat4 cameraWorld = frame.view.Inverse();
				const Vec3 cameraRight = cameraWorld.GetRight().
					Normalized();
				const Vec3 cameraUp      = cameraWorld.GetUp().Normalized();
				const Vec3 cameraForward = cameraWorld.GetForward().
					Normalized();

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTargetAndDepth(
					std::span<const uint32_t>(&state.colorTextureId, 1),
					state.depthTextureId
				);
				if (
					!mBillboardPass.depthGeom.resolved ||
					!mBillboardPass.depthGeom.resolved->pso
				) {
					return;
				}
				pass.SetGraphicsPipeline(
					mBillboardPass.depthGeom.resolved->rootSignature,
					mBillboardPass.depthGeom.resolved->pso
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
				);
				pass.SetVertexBuffer(mBillboardPass.depthGeom.vbv);
				pass.SetIndexBuffer(mBillboardPass.depthGeom.ibv);

				for (const auto& billboard : view.worldBillboards) {
					if (!billboard.depthTest) {
						continue;
					}
					const float cosine = std::cos(
						billboard.rotationRad
					);
					const float sine         = std::sin(billboard.rotationRad);
					const Vec3  rotatedRight =
						cameraRight * cosine + cameraUp * sine;
					const Vec3 rotatedUp =
						cameraRight * -sine + cameraUp * cosine;

					Rhi::ObjectConstants object = {};
					object.world                = Mat4::identity;
					object.world.m[0][0]        =
						rotatedRight.x * billboard.sizeWorld.x * 0.5f;
					object.world.m[0][1] =
						rotatedRight.y * billboard.sizeWorld.x * 0.5f;
					object.world.m[0][2] =
						rotatedRight.z * billboard.sizeWorld.x * 0.5f;
					object.world.m[1][0] =
						rotatedUp.x * billboard.sizeWorld.y * 0.5f;
					object.world.m[1][1] =
						rotatedUp.y * billboard.sizeWorld.y * 0.5f;
					object.world.m[1][2] =
						rotatedUp.z * billboard.sizeWorld.y * 0.5f;
					object.world.m[2][0]         = cameraForward.x;
					object.world.m[2][1]         = cameraForward.y;
					object.world.m[2][2]         = cameraForward.z;
					object.world.m[3][0]         = billboard.worldPosition.x;
					object.world.m[3][1]         = billboard.worldPosition.y;
					object.world.m[3][2]         = billboard.worldPosition.z;
					object.worldInverseTranspose =
						object.world.Inverse().Transpose();
					const float uvMinY =
						billboard.uvFlipY ? 1.0f : 0.0f;
					const float uvMaxY =
						billboard.uvFlipY ? 0.0f : 1.0f;
					object.skinningInfo = Vec4(
						0.0f, uvMinY, 1.0f, uvMaxY);

					Rhi::MaterialConstants material = {};
					material.baseColor              = billboard.color;
					material.opacity                = billboard.color.w;
					material.domainMode             = 0.0f;

					const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
						allocator.AllocateConstantBuffer(
							&object, sizeof(object)
						);
					const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
						allocator.AllocateConstantBuffer(
							&material, sizeof(material)
						);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::OBJECT), objectCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::MATERIAL),
						materialCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::SKINNING), objectCb
					);
					pass.BindGraphicsSrvTable(
						ToRootIndex(GEOM_ROOT_SLOT::BASE_COLOR_TEXTURE),
						ResolveSpriteTexture(
							renderDevice, billboard.texture
						)
					);
					pass.DrawIndexedTest(
						mBillboardPass.depthGeom.indexCount
					);
				}
			}
		);
	}

	void Renderer::AddWorldSpritePass(
		RenderDevice&                renderDevice,
		const std::string&           prefix,
		size_t                       viewIndex,
		const ViewRuntimeState&      state,
		const std::vector<uint32_t>& worldSpriteTextureIds
	) {
		// パス: ワールドスプライト。
		// 入力: RenderViewInput::worldSprites、worldSpriteTextureIds SRV。
		// 出力: シーンビューのワールドスプライトカラー/深度書き込み。
		// PSO: mBillboardPass.depthGeom.resolved->pso。
		// ルートシグネチャ: mBillboardPass.depthGeom.resolved->rootSignature (Geom)。
		// レンダーターゲット: state.colorTextureId。
		// 深度ステンシル: state.depthTextureId。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: WriteRt(state.colorTextureId), WriteDepth(state.depthTextureId), ReadSrvPs(worldSpriteTextureIds 各エントリ)。
		// 注記: カメラ向きビルボードではなく明示的な worldRight/worldUp ベクトルを使用。
		const uint32_t colorId = state.colorTextureId;
		const uint32_t depthId = state.depthTextureId;

		mGraph.AddPass(
			prefix + "WorldSprite",
			[colorId, depthId, worldSpriteTextureIds](
			RenderGraphBuilder& b
		) {
				b.WriteRt(colorId);
				b.WriteDepth(depthId);
				for (const uint32_t texId : worldSpriteTextureIds) {
					b.ReadSrvPs(texId);
				}
			},
			[this, viewIndex, state, &renderDevice](
			const RenderPassContext& pass
		) {
				const RenderViewInput& view = mFrameViews[viewIndex];
				if (view.worldSprites.empty()) {
					return;
				}

				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				const Rhi::FrameConstants frame = BuildSceneFrameConstants(
					view.camera, state.logicalWidth,
					state.logicalHeight, 0.0f
				);
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(
						&frame, sizeof(frame)
					);

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTargetAndDepth(
					std::span<const uint32_t>(&state.colorTextureId, 1),
					state.depthTextureId
				);
				if (
					!mBillboardPass.depthGeom.resolved ||
					!mBillboardPass.depthGeom.resolved->pso
				) {
					return;
				}
				pass.SetGraphicsPipeline(
					mBillboardPass.depthGeom.resolved->rootSignature,
					mBillboardPass.depthGeom.resolved->pso
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
				);
				pass.SetVertexBuffer(mBillboardPass.depthGeom.vbv);
				pass.SetIndexBuffer(mBillboardPass.depthGeom.ibv);

				for (const auto& sprite : view.worldSprites) {
					Vec3 right = sprite.worldRight;
					Vec3 up    = sprite.worldUp;
					if (right.SqrLength() < 1e-6f) {
						right = Vec3::right;
					}
					if (up.SqrLength() < 1e-6f) {
						up = Vec3::up;
					}
					right             = right.Normalized();
					up                = up.Normalized();
					const Vec3 normal = right.Cross(up).Normalized();

					const float cosine       = std::cos(sprite.rotationRad);
					const float sine         = std::sin(sprite.rotationRad);
					const Vec3  rotatedRight =
						right * cosine + up * sine;
					const Vec3 rotatedUp = right * -sine + up * cosine;

					Rhi::ObjectConstants object = {};
					object.world                = Mat4::identity;
					object.world.m[0][0]        =
						rotatedRight.x * sprite.sizeWorld.x * 0.5f;
					object.world.m[0][1] =
						rotatedRight.y * sprite.sizeWorld.x * 0.5f;
					object.world.m[0][2] =
						rotatedRight.z * sprite.sizeWorld.x * 0.5f;
					object.world.m[1][0] =
						rotatedUp.x * sprite.sizeWorld.y * 0.5f;
					object.world.m[1][1] =
						rotatedUp.y * sprite.sizeWorld.y * 0.5f;
					object.world.m[1][2] =
						rotatedUp.z * sprite.sizeWorld.y * 0.5f;
					object.world.m[2][0]         = normal.x;
					object.world.m[2][1]         = normal.y;
					object.world.m[2][2]         = normal.z;
					object.world.m[3][0]         = sprite.worldPosition.x;
					object.world.m[3][1]         = sprite.worldPosition.y;
					object.world.m[3][2]         = sprite.worldPosition.z;
					object.worldInverseTranspose =
						object.world.Inverse().Transpose();
					const float uvMinY  = sprite.uvFlipY ? 1.0f : 0.0f;
					const float uvMaxY  = sprite.uvFlipY ? 0.0f : 1.0f;
					object.skinningInfo = Vec4(
						0.0f, uvMinY, 1.0f, uvMaxY);

					Rhi::MaterialConstants material = {};
					material.baseColor              = sprite.color;
					material.opacity                = sprite.color.w;
					material.domainMode             = 0.0f;

					const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
						allocator.AllocateConstantBuffer(
							&object, sizeof(object)
						);
					const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
						allocator.AllocateConstantBuffer(
							&material, sizeof(material)
						);

					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::OBJECT), objectCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::MATERIAL),
						materialCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::SKINNING), objectCb
					);
					pass.BindGraphicsSrvTable(
						ToRootIndex(GEOM_ROOT_SLOT::BASE_COLOR_TEXTURE),
						ResolveSpriteTexture(
							renderDevice, sprite.texture
						)
					);
					pass.DrawIndexedTest(
						mBillboardPass.depthGeom.indexCount
					);
				}
			}
		);
	}

	void Renderer::AddDebugLinePass(
		RenderDevice&           renderDevice,
		const std::string&      prefix,
		size_t                  viewIndex,
		const ViewRuntimeState& state
	) {
		// パス: デバッグライン。
		// 入力: UploadDebugLinesForFrame でアップロードされた mLinePass.frameVbv とアクティブなシーンカメラ。
		// 出力: シーンビューのデバッグラインカラー/深度書き込み。
		// PSO: mLinePass.resolved->pso。
		// ルートシグネチャ: mLinePass.resolved->rootSignature (Geom ルートスロット)。
		// レンダーターゲット: state.colorTextureId。
		// 深度ステンシル: state.depthTextureId。
		// ディスクリプタヒープ: このパスで設定されることなし。
		// リソースステート: WriteRt(state.colorTextureId), WriteDepth(state.depthTextureId)。
		// 注記: LINELIST トポロジーと現在のフレーム頂点カウントを使用。
		const uint32_t colorId = state.colorTextureId;
		const uint32_t depthId = state.depthTextureId;

		mGraph.AddPass(
			prefix + "DebugLines",
			[colorId, depthId](RenderGraphBuilder& b) {
				b.WriteRt(colorId);
				b.WriteDepth(depthId);
			},
			[this, state, viewIndex, &renderDevice](
			const RenderPassContext& pass
		) {
				if (
					mLinePass.frameVertexCount == 0 ||
					!mLinePass.resolved ||
					!mLinePass.resolved->pso
				) {
					return;
				}

				const RenderViewInput& view = mFrameViews[viewIndex];
				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				const Rhi::FrameConstants frame = BuildSceneFrameConstants(
					view.camera, state.logicalWidth,
					state.logicalHeight, 0.0f
				);
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(
						&frame, sizeof(frame)
					);

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetRenderTargetAndDepth(
					std::span<const uint32_t>(&state.colorTextureId, 1),
					state.depthTextureId
				);
				pass.SetGraphicsPipeline(
					mLinePass.resolved->rootSignature,
					mLinePass.resolved->pso
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
				);
				pass.SetVertexBuffer(mLinePass.frameVbv);
				pass.SetPrimitiveTopology(
					D3D_PRIMITIVE_TOPOLOGY_LINELIST
				);
				pass.DrawInstanced(mLinePass.frameVertexCount, 1);
			}
		);
	}

	void Renderer::AddWorldBillboardFrontPass(
		RenderDevice&                renderDevice,
		const std::string&           prefix,
		size_t                       viewIndex,
		const ViewRuntimeState&      state,
		const std::vector<uint32_t>& worldBillboardTextureIds
	) {
		// パス: ワールドビルボード フロント。
		// 入力: depthTest が false の RenderViewInput::worldBillboards、worldBillboardTextureIds SRV。
		// 出力: シーンビューのフロントビルボードカラー書き込み。
		// PSO: mBillboardPass.frontGeom.resolved->pso。
		// ルートシグネチャ: mBillboardPass.frontGeom.resolved->rootSignature (Geom)。
		// レンダーターゲット: state.colorTextureId。
		// 深度ステンシル: このパスでバインドなし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: WriteRt(state.colorTextureId), ReadSrvPs(worldBillboardTextureIds 各エントリ)。
		// 注記: 既存の深度なしフロントビルボードパスを保持。
		const uint32_t colorId = state.colorTextureId;

		mGraph.AddPass(
			prefix + "WorldBillboardFront",
			[colorId, worldBillboardTextureIds](RenderGraphBuilder& b) {
				b.WriteRt(colorId);
				for (const uint32_t texId : worldBillboardTextureIds) {
					b.ReadSrvPs(texId);
				}
			},
			[this, viewIndex, state, &renderDevice](
			const RenderPassContext& pass
		) {
				const RenderViewInput& view = mFrameViews[viewIndex];
				if (view.worldBillboards.empty()) {
					return;
				}
				if (
					!std::ranges::any_of(
						view.worldBillboards,
						[](const WorldBillboardInput& billboard) {
							return !billboard.depthTest;
						}
					)) {
					return;
				}

				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				const Rhi::FrameConstants frame = BuildSceneFrameConstants(
					view.camera, state.logicalWidth,
					state.logicalHeight, 0.0f
				);
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(
						&frame, sizeof(frame));

				const Mat4 cameraWorld = frame.view.Inverse();
				const Vec3 cameraRight = cameraWorld.GetRight().
					Normalized();
				const Vec3 cameraUp      = cameraWorld.GetUp().Normalized();
				const Vec3 cameraForward = cameraWorld.GetForward().
					Normalized();

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTarget(state.colorTextureId);
				if (
					!mBillboardPass.frontGeom.resolved ||
					!mBillboardPass.frontGeom.resolved->pso
				) {
					return;
				}
				pass.SetGraphicsPipeline(
					mBillboardPass.frontGeom.resolved->rootSignature,
					mBillboardPass.frontGeom.resolved->pso
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
				);
				pass.SetVertexBuffer(mBillboardPass.frontGeom.vbv);
				pass.SetIndexBuffer(mBillboardPass.frontGeom.ibv);

				for (const auto& billboard : view.worldBillboards) {
					if (billboard.depthTest) {
						continue;
					}

					const float cosine =
						std::cos(billboard.rotationRad);
					const float sine         = std::sin(billboard.rotationRad);
					const Vec3  rotatedRight =
						cameraRight * cosine + cameraUp * sine;
					const Vec3 rotatedUp =
						cameraRight * -sine + cameraUp * cosine;

					Rhi::ObjectConstants object = {};
					object.world                = Mat4::identity;
					object.world.m[0][0]        =
						rotatedRight.x * billboard.sizeWorld.x * 0.5f;
					object.world.m[0][1] =
						rotatedRight.y * billboard.sizeWorld.x * 0.5f;
					object.world.m[0][2] =
						rotatedRight.z * billboard.sizeWorld.x * 0.5f;
					object.world.m[1][0] =
						rotatedUp.x * billboard.sizeWorld.y * 0.5f;
					object.world.m[1][1] =
						rotatedUp.y * billboard.sizeWorld.y * 0.5f;
					object.world.m[1][2] =
						rotatedUp.z * billboard.sizeWorld.y * 0.5f;
					object.world.m[2][0]         = cameraForward.x;
					object.world.m[2][1]         = cameraForward.y;
					object.world.m[2][2]         = cameraForward.z;
					object.world.m[3][0]         = billboard.worldPosition.x;
					object.world.m[3][1]         = billboard.worldPosition.y;
					object.world.m[3][2]         = billboard.worldPosition.z;
					object.worldInverseTranspose =
						object.world.Inverse().Transpose();
					const float uvMinY =
						billboard.uvFlipY ? 1.0f : 0.0f;
					const float uvMaxY =
						billboard.uvFlipY ? 0.0f : 1.0f;
					object.skinningInfo = Vec4(
						0.0f, uvMinY, 1.0f, uvMaxY);

					Rhi::MaterialConstants material = {};
					material.baseColor              = billboard.color;
					material.opacity                = billboard.color.w;
					material.domainMode             = 0.0f;

					const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
						allocator.AllocateConstantBuffer(
							&object, sizeof(object));
					const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
						allocator.AllocateConstantBuffer(
							&material,
							sizeof(material)
						);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::OBJECT), objectCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::MATERIAL),
						materialCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::SKINNING), objectCb
					);
					pass.BindGraphicsSrvTable(
						ToRootIndex(GEOM_ROOT_SLOT::BASE_COLOR_TEXTURE),
						ResolveSpriteTexture(
							renderDevice, billboard.texture)
					);
					pass.DrawIndexedTest(
						mBillboardPass.frontGeom.indexCount);
				}
			}
		);
	}

	void Renderer::AddScenePostProcessPasses(
		RenderDevice&           renderDevice,
		const std::string&      prefix,
		const size_t            viewIndex,
		const ViewRuntimeState& state,
		uint32_t&               outputId
	) {
		// パス: シーン ポストプロセス チェーン。
		// 入力: state.colorTextureId、ポストFX ピンポンテクスチャ、ブルームミップ、ビュー ポストFX オーバーライド。
		// 出力: ToneMapExposure 後の state.outputTextureId。
		// PSO: mBloomDownsamplePass/mBloomUpsamplePass/mHdrCopyPass/mBloomCombinePass、 各 PostFxRuntimePass、mToneMapPass。
		// ルートシグネチャ: FS_ROOT_SLOT バインディングを使用したフルスクリーンパスルートシグネチャ。
		// レンダーターゲット: ポストFX ピンポンテクスチャ、ブルームミップテクスチャ、その後 state.outputTextureId。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: 各内部パスは ReadSrvPs(ソーステクスチャ) と WriteRt(デスティネーションテクスチャ) を宣言。ToneMapExposure が state.outputTextureId を書き込み。
		// 注記: Bloom、汎用 PostFx、ToneMap の順序を BuildGraph と全く同じに保持。
		const RenderViewInput& view = mFrameViews[viewIndex];

		uint32_t postFxInputId             = state.colorTextureId;
		uint32_t postFxOutputId            = state.postFxTextureAId;
		auto     BuildResolvedPostFxParams =
			[&view](const PostFxRuntimePass& passRes) {
			PostFxParamsConstants     params       = {};
			const PostFxPassOverride* viewOverride =
				FindPostFxPassOverride(
					passRes.name, view.postFxPassOverrides
				);
			for (const auto& [name, value] : passRes.scalarDefaults) {
				ApplyScalarParam(name, value, params);
			}
			for (const auto& [name, value] : passRes.colorDefaults) {
				ApplyColorParam(name, value, params);
			}
			if (viewOverride) {
				for (const auto& [name, value] : viewOverride->scalarParams) {
					ApplyScalarParam(name, value, params);
				}
				for (const auto& [name, value] : viewOverride->colorParams) {
					ApplyColorParam(name, value, params);
				}
			}
			return std::pair{params, viewOverride};
		};

		for (const auto& passRes : mPostFxPasses) {
			const auto [resolvedParams, viewOverride] =
				BuildResolvedPostFxParams(passRes);
			bool passEnabled = passRes.enabled;
			if (viewOverride && viewOverride->hasEnabledOverride) {
				passEnabled = viewOverride->enabled;
			}
			if (!passEnabled) {
				continue;
			}

			if (StrUtil::EqualsIgnoreCase(passRes.name, "Bloom")) {
				const int mipCount = static_cast<int>(
					state.bloomMipTextureIds.size()
				);

				const float bloomIntensity = std::max(
					resolvedParams.scalar0.x, 0.0f
				);
				if (bloomIntensity <= 0.0f) {
					continue;
				}
				const float bloomThreshold = resolvedParams.scalar0.y;
				const float bloomRadius    = std::max(
					resolvedParams.scalar0.z, 0.0f
				);
				const float bloomKnee = std::max(
					resolvedParams.scalar0.w, 0.0f
				);

				AddBloomDownsamplePasses(
					renderDevice,
					prefix,
					state,
					mipCount,
					bloomIntensity,
					bloomThreshold,
					bloomRadius,
					bloomKnee,
					postFxInputId
				);
				AddBloomUpsamplePasses(
					renderDevice,
					prefix,
					state,
					mipCount,
					bloomIntensity,
					bloomRadius
				);

				const uint32_t bloomBaseId        = state.bloomMipTextureIds[0];
				const uint32_t baseCopyInId       = postFxInputId;
				const uint32_t bloomCombinedOutId = postFxOutputId;

				AddBloomBaseCopyPass(
					renderDevice,
					prefix,
					state,
					baseCopyInId,
					bloomCombinedOutId
				);
				AddBloomCompositePass(
					renderDevice,
					prefix,
					state,
					bloomBaseId,
					bloomCombinedOutId,
					bloomIntensity,
					bloomRadius
				);

				postFxInputId  = bloomCombinedOutId;
				postFxOutputId = postFxOutputId == state.postFxTextureAId ?
					                 state.postFxTextureBId :
					                 state.postFxTextureAId;
				continue;
			}

			AddGenericPostFxPasses(
				renderDevice,
				prefix,
				state,
				passRes,
				resolvedParams.scalar0,
				resolvedParams.scalar1,
				resolvedParams.color0,
				resolvedParams.color1,
				postFxInputId,
				postFxOutputId
			);
		}

		AddToneMapExposurePass(
			renderDevice, prefix, state, view, postFxInputId, outputId
		);
	}

	void Renderer::AddBloomDownsamplePasses(
		const RenderDevice&     renderDevice,
		const std::string&      prefix,
		const ViewRuntimeState& state,
		const int               mipCount,
		const float             bloomIntensity,
		const float             bloomThreshold,
		const float             bloomRadius,
		const float             bloomKnee,
		const uint32_t          postFxInputId
	) {
		// パス: ブルーム ダウンサンプル。
		// 入力: レベル 0 の postFxInputId、その後は各前のブルームミップ。
		// 出力: state.bloomMipTextureIds[level]。
		// PSO: mBloomDownsamplePass.resolved->pso。
		// ルートシグネチャ: mBloomDownsamplePass.resolved->rootSignature。
		// レンダーターゲット: state.bloomMipTextureIds[level]。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: ReadSrvPs(downsampleSrcId), WriteRt(dstId)。
		// 注記: 最初のパスは現在のポストFX 入力を読み込み、後のパスは前のブルームミップを読み込む。
		uint32_t srcId = postFxInputId;
		for (uint32_t level = 0; std::cmp_less(level, mipCount); ++level) {
			const uint32_t dstId    = state.bloomMipTextureIds[level];
			const uint32_t srcWidth = std::max(
				1u, state.logicalWidth >> level
			);
			const uint32_t srcHeight = std::max(
				1u, state.logicalHeight >> level
			);
			const uint32_t dstWidth = std::max(
				1u, state.logicalWidth >> static_cast<uint32_t>(level + 1)
			);
			const uint32_t dstHeight = std::max(
				1u, state.logicalHeight >> static_cast<uint32_t>(level + 1)
			);

			BloomPyramidConstants bloomCbData = {};
			bloomCbData.params0               = Vec4(
				1.0f / static_cast<float>(srcWidth),
				1.0f / static_cast<float>(srcHeight),
				bloomThreshold,
				bloomKnee
			);
			bloomCbData.params1 = Vec4(
				bloomRadius, bloomIntensity,
				level == 0 ? 1.0f : 0.0f, 0.0f
			);

			auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
				renderDevice.GetRhiDevice()
			).GetFrameUploadAllocator();
			const D3D12_GPU_VIRTUAL_ADDRESS bloomCb =
				allocator.AllocateConstantBuffer(
					&bloomCbData, sizeof(bloomCbData)
				);

			const uint32_t downsampleSrcId = srcId;
			// ブルーム ダウンサンプル: ReadSrvPs(downsampleSrcId) -> WriteRt(dstId)。
			mGraph.AddPass(
				prefix + "BloomDownsample[" + std::to_string(level) + "]",
				[downsampleSrcId, dstId](RenderGraphBuilder& b) {
					b.ReadSrvPs(downsampleSrcId);
					b.WriteRt(dstId);
				},
				[this, dstId, dstWidth, dstHeight, bloomCb, downsampleSrcId](
				const RenderPassContext& pass
			) {
					pass.SetViewportAndScissor(
						0.0f,
						0.0f,
						static_cast<float>(dstWidth),
						static_cast<float>(dstHeight)
					);
					pass.SetSrvUavHeap();
					pass.SetRenderTarget(dstId);
					if (
						!mBloomDownsamplePass.resolved ||
						!mBloomDownsamplePass.resolved->pso
					) {
						return;
					}
					pass.SetGraphicsPipeline(
						mBloomDownsamplePass.resolved->rootSignature,
						mBloomDownsamplePass.resolved->pso
					);
					pass.BindGraphicsCbv(
						ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS),
						bloomCb
					);
					pass.BindGraphicsSrvTable(
						ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE),
						downsampleSrcId
					);
					pass.DrawFullscreenTriangle();
				}
			);

			srcId = dstId;
		}
	}

	void Renderer::AddBloomUpsamplePasses(
		const RenderDevice&     renderDevice,
		const std::string&      prefix,
		const ViewRuntimeState& state,
		const int               mipCount,
		const float             bloomIntensity,
		const float             bloomRadius
	) {
		// パス: ブルーム アップサンプル。
		// 入力: state.bloomMipTextureIds[level] の低解像度ブルームミップ。
		// 出力: state.bloomMipTextureIds[level - 1] の高解像度ブルームミップ。
		// PSO: mBloomUpsamplePass.resolved->pso。
		// ルートシグネチャ: mBloomUpsamplePass.resolved->rootSignature。
		// レンダーターゲット: state.bloomMipTextureIds[level - 1]。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: ReadSrvPs(srcLowId), WriteRt(dstHighId)。
		// 注記: 最低ミップからブルームミップ 0 に向けて実行。
		for (uint32_t level = mipCount - 1; level > 0; --level) {
			const uint32_t srcLowId  = state.bloomMipTextureIds[level];
			const uint32_t dstHighId = state.bloomMipTextureIds[level - 1];
			const uint32_t srcWidth  = std::max(
				1u, state.logicalWidth >> static_cast<uint32_t>(level + 1)
			);
			const uint32_t srcHeight = std::max(
				1u, state.logicalHeight >> static_cast<uint32_t>(level + 1)
			);
			const uint32_t dstWidth = std::max(
				1u, state.logicalWidth >> level
			);
			const uint32_t dstHeight = std::max(
				1u, state.logicalHeight >> level
			);

			BloomPyramidConstants bloomCbData = {};
			bloomCbData.params0               = Vec4(
				1.0f / static_cast<float>(srcWidth),
				1.0f / static_cast<float>(srcHeight),
				0.0f,
				0.0f
			);
			bloomCbData.params1 = Vec4(
				bloomRadius, bloomIntensity, 0.0f, 0.0f
			);

			auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
				renderDevice.GetRhiDevice()
			).GetFrameUploadAllocator();
			const D3D12_GPU_VIRTUAL_ADDRESS bloomCb =
				allocator.AllocateConstantBuffer(
					&bloomCbData, sizeof(bloomCbData)
				);

			// ブルーム アップサンプル: ReadSrvPs(srcLowId) -> WriteRt(dstHighId)。
			mGraph.AddPass(
				prefix + "BloomUpsample[" + std::to_string(level) + "]",
				[srcLowId, dstHighId](RenderGraphBuilder& b) {
					b.ReadSrvPs(srcLowId);
					b.WriteRt(dstHighId);
				},
				[this, dstHighId, dstWidth, dstHeight, bloomCb, srcLowId](
				const RenderPassContext& pass
			) {
					pass.SetViewportAndScissor(
						0.0f,
						0.0f,
						static_cast<float>(dstWidth),
						static_cast<float>(dstHeight)
					);
					pass.SetSrvUavHeap();
					pass.SetRenderTarget(dstHighId);
					if (
						!mBloomUpsamplePass.resolved ||
						!mBloomUpsamplePass.resolved->pso
					) {
						return;
					}
					pass.SetGraphicsPipeline(
						mBloomUpsamplePass.resolved->rootSignature,
						mBloomUpsamplePass.resolved->pso
					);
					pass.BindGraphicsCbv(
						ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS),
						bloomCb
					);
					pass.BindGraphicsSrvTable(
						ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE),
						srcLowId
					);
					pass.DrawFullscreenTriangle();
				}
			);
		}
	}

	void Renderer::AddBloomBaseCopyPass(
		RenderDevice&           renderDevice,
		const std::string&      prefix,
		const ViewRuntimeState& state,
		const uint32_t          baseCopyInId,
		const uint32_t          bloomCombinedOutId
	) {
		// パス: ブルーム 基本コピー。
		// 入力: baseCopyInId。
		// 出力: bloomCombinedOutId。
		// PSO: mHdrCopyPass.resolved->pso。
		// ルートシグネチャ: mHdrCopyPass.resolved->rootSignature。
		// レンダーターゲット: bloomCombinedOutId。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: ReadSrvPs(baseCopyInId), WriteRt(bloomCombinedOutId)。
		// 注記: 合成前にピンポンターゲットに事前ブルーム HDR 入力をコピー。
		mGraph.AddPass(
			prefix + "BloomBaseCopy",
			[baseCopyInId, bloomCombinedOutId](RenderGraphBuilder& b) {
				b.ReadSrvPs(baseCopyInId);
				b.WriteRt(bloomCombinedOutId);
			},
			[this, state, baseCopyInId, bloomCombinedOutId, &renderDevice](
			const RenderPassContext& pass
		) {
				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTarget(bloomCombinedOutId);
				if (!mHdrCopyPass.resolved || !mHdrCopyPass.resolved->pso) {
					return;
				}
				pass.SetGraphicsPipeline(
					mHdrCopyPass.resolved->rootSignature,
					mHdrCopyPass.resolved->pso
				);
				pass.BindGraphicsSrvTable(
					ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE),
					baseCopyInId
				);
				PostFxParamsConstants copyParams = {};
				copyParams.scalar0.x             = std::clamp(
					static_cast<float>(std::max(1u, state.logicalWidth)) /
					static_cast<float>(std::max(1u, state.allocatedWidth)),
					0.0f,
					1.0f
				);
				copyParams.scalar0.y = std::clamp(
					static_cast<float>(std::max(1u, state.logicalHeight)) /
					static_cast<float>(std::max(1u, state.allocatedHeight)),
					0.0f,
					1.0f
				);
				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				const D3D12_GPU_VIRTUAL_ADDRESS copyCb =
					allocator.AllocateConstantBuffer(
						&copyParams, sizeof(copyParams)
					);
				pass.BindGraphicsCbv(
					ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS),
					copyCb
				);
				pass.DrawFullscreenTriangle();
			}
		);
	}

	void Renderer::AddBloomCompositePass(
		const RenderDevice&           renderDevice,
		const std::string&      prefix,
		const ViewRuntimeState& state,
		const uint32_t          bloomBaseId,
		const uint32_t          bloomCombinedOutId,
		const float             bloomIntensity,
		const float             bloomRadius
	) {
		// パス: ブルーム 合成。
		// 入力: bloomBaseId。
		// 出力: bloomCombinedOutId。
		// PSO: mBloomCombinePass.resolved->pso。
		// ルートシグネチャ: mBloomCombinePass.resolved->rootSignature。
		// レンダーターゲット: bloomCombinedOutId。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: ReadSrvPs(bloomBaseId), WriteRt(bloomCombinedOutId)。
		// 注記: ブルームミップ 0 をコピーされた HDR 基本に同じ出力ターゲットで追加。
		BloomPyramidConstants bloomCompositeCbData;
		const uint32_t        bloomBaseLogicalWidth = std::max(
			1u, state.logicalWidth >> 1u
		);
		const uint32_t bloomBaseLogicalHeight = std::max(
			1u, state.logicalHeight >> 1u
		);
		bloomCompositeCbData.params0 = Vec4(
			1.0f / static_cast<float>(bloomBaseLogicalWidth),
			1.0f / static_cast<float>(bloomBaseLogicalHeight),
			0.0f,
			0.0f
		);
		bloomCompositeCbData.params1 = Vec4(
			bloomRadius, bloomIntensity, 0.0f, 0.0f
		);
		auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
			renderDevice.GetRhiDevice()
		).GetFrameUploadAllocator();
		const D3D12_GPU_VIRTUAL_ADDRESS bloomCompositeCb =
			allocator.AllocateConstantBuffer(
				&bloomCompositeCbData,
				sizeof(bloomCompositeCbData)
			);

		mGraph.AddPass(
			prefix + "BloomComposite",
			[bloomBaseId, bloomCombinedOutId](RenderGraphBuilder& b) {
				b.ReadSrvPs(bloomBaseId);
				b.WriteRt(bloomCombinedOutId);
			},
			[this, state, bloomCombinedOutId, bloomBaseId, bloomCompositeCb](
			const RenderPassContext& pass
		) {
				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTarget(bloomCombinedOutId);
				if (
					!mBloomCombinePass.resolved ||
					!mBloomCombinePass.resolved->pso
				) {
					return;
				}
				pass.SetGraphicsPipeline(
					mBloomCombinePass.resolved->rootSignature,
					mBloomCombinePass.resolved->pso
				);
				pass.BindGraphicsCbv(
					ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS),
					bloomCompositeCb
				);
				pass.BindGraphicsSrvTable(
					ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE),
					bloomBaseId
				);
				pass.DrawFullscreenTriangle();
			}
		);
	}

	void Renderer::AddGenericPostFxPasses(
		const RenderDevice&            renderDevice,
		const std::string&       prefix,
		const ViewRuntimeState&  state,
		const PostFxRuntimePass& passRes,
		const Vec4&              scalar0,
		const Vec4&              scalar1,
		const Vec4&              color0,
		const Vec4&              color1,
		uint32_t&                postFxInputId,
		uint32_t&                postFxOutputId
	) {
		// パス: 汎用 PostFx チェーン。
		// 入力: postFxInputId。
		// 出力: postFxOutputId。
		// PSO: passRes.pass.resolved->pso。
		// ルートシグネチャ: passRes.pass.resolved->rootSignature。
		// レンダーターゲット: postFxOutputId。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: ReadSrvPs(inId), WriteRt(outId)。
		// 注記: パスの追加後にシーン ポストFX ピンポンターゲットを進める。
		PostFxParamsConstants resolvedParams;
		resolvedParams.scalar0 = scalar0;
		resolvedParams.scalar1 = scalar1;
		resolvedParams.color0  = color0;
		resolvedParams.color1  = color1;

		auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
			renderDevice.GetRhiDevice()
		).GetFrameUploadAllocator();
		const D3D12_GPU_VIRTUAL_ADDRESS postFxCb =
			allocator.AllocateConstantBuffer(
				&resolvedParams, sizeof(resolvedParams)
			);

		const auto inId  = postFxInputId;
		const auto outId = postFxOutputId;

		mGraph.AddPass(
			prefix + "PostFx_" + passRes.name,
			[inId, outId](RenderGraphBuilder& b) {
				b.ReadSrvPs(inId);
				b.WriteRt(outId);
			},
			[this, passRes, inId, outId, state, postFxCb](
			const RenderPassContext& pass
		) {
				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				if (!passRes.pass.resolved || !passRes.pass.resolved->pso) {
					return;
				}
				pass.SetGraphicsPipeline(
					passRes.pass.resolved->rootSignature,
					passRes.pass.resolved->pso
				);
				pass.SetRenderTarget(outId);
				pass.BindGraphicsCbv(
					ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS),
					postFxCb
				);
				pass.BindGraphicsSrvTable(
					ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE), inId
				);
				pass.DrawFullscreenTriangle();
			}
		);

		postFxInputId  = outId;
		postFxOutputId = postFxOutputId == state.postFxTextureAId ?
			                 state.postFxTextureBId :
			                 state.postFxTextureAId;
	}

	void Renderer::AddToneMapExposurePass(
		const RenderDevice&           renderDevice,
		const std::string&      prefix,
		const ViewRuntimeState& state,
		const RenderViewInput&  view,
		const uint32_t          postFxInputId,
		uint32_t&               outputId
	) {
		// Pass: ToneMapExposure.
		// Input: postFxInputId.
		// Output: state.outputTextureId.
		// PSO: mToneMapPass.resolved->pso.
		// RootSignature: mToneMapPass.resolved->rootSignature.
		// RenderTarget: state.outputTextureId.
		// DepthStencil: none.
		// DescriptorHeap: D3D12Device SRV/UAV heap via RenderPassContext::SetSrvUavHeap.
		// ResourceState: ReadSrvPs(toneMapInputId), WriteRt(toneMapOutputId).
		// Notes: Writes the final LDR output texture and updates outputId.
		PostFxParamsConstants toneMapParams = {};
		toneMapParams.scalar1.x             = view.camera.exposureEv;
		auto& allocator                     = dynamic_cast<Rhi::D3D12Device&>(
			renderDevice.GetRhiDevice()
		).GetFrameUploadAllocator();
		const D3D12_GPU_VIRTUAL_ADDRESS toneMapCb =
			allocator.AllocateConstantBuffer(
				&toneMapParams, sizeof(toneMapParams)
			);
		const uint32_t toneMapInputId  = postFxInputId;
		const uint32_t toneMapOutputId = state.outputTextureId;
		mGraph.AddPass(
			prefix + "ToneMapExposure",
			[toneMapInputId, toneMapOutputId](RenderGraphBuilder& b) {
				b.ReadSrvPs(toneMapInputId);
				b.WriteRt(toneMapOutputId);
			},
			[this, state, toneMapInputId, toneMapOutputId, toneMapCb](
			const RenderPassContext& pass
		) {
				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTarget(toneMapOutputId);
				if (!mToneMapPass.resolved || !mToneMapPass.resolved->pso) {
					return;
				}
				pass.SetGraphicsPipeline(
					mToneMapPass.resolved->rootSignature,
					mToneMapPass.resolved->pso
				);
				pass.BindGraphicsCbv(
					ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS), toneMapCb
				);
				pass.BindGraphicsSrvTable(
					ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE),
					toneMapInputId
				);
				pass.DrawFullscreenTriangle();
			}
		);

		outputId = toneMapOutputId;
	}

	void Renderer::AddSpriteOnlyClearPass(
		const std::string& prefix,
		const uint32_t     outputId
	) {
		// Pass: Sprite-only clear.
		// Input: none.
		// Output: sprite-only output texture is cleared.
		// PSO: none.
		// RootSignature: none.
		// RenderTarget: outputId.
		// DepthStencil: none.
		// DescriptorHeap: none.
		// ResourceState: WriteRt(outputId).
		// Notes: Keeps transparent black clear for sprite-only views.
		mGraph.AddPass(
			prefix + "Clear",
			[outputId](RenderGraphBuilder& b) {
				b.WriteRt(outputId);
				b.ClearColor(outputId, 0.0f, 0.0f, 0.0f, 0.0f);
			},
			[](RenderPassContext&) {
			}
		);
	}

	void Renderer::AddScreenSpritePass(
		RenderDevice&                renderDevice,
		const std::string&           prefix,
		size_t                       viewIndex,
		const ViewRuntimeState&      state,
		const uint32_t               outputId,
		const std::vector<uint32_t>& screenSpriteTextureIds
	) {
		// Pass: Screen sprites.
		// Input: RenderViewInput::screenSprites, screenSpriteTextureIds SRVs.
		// Output: screen sprite writes into the view output texture.
		// PSO: mSpritePass.geom.resolved->pso.
		// RootSignature: mSpritePass.geom.resolved->rootSignature (Geom).
		// RenderTarget: outputId.
		// DepthStencil: none.
		// DescriptorHeap: D3D12Device SRV/UAV heap via RenderPassContext::SetSrvUavHeap.
		// ResourceState: WriteRt(outputId), ReadSrvPs(each screenSpriteTextureIds entry).
		// Notes: Keeps UI sampler convar logging and fallback behavior unchanged.
		mGraph.AddPass(
			prefix + "ScreenSprites",
			[outputId, screenSpriteTextureIds](RenderGraphBuilder& b) {
				b.WriteRt(outputId);
				for (const uint32_t texId : screenSpriteTextureIds) {
					b.ReadSrvPs(texId);
				}
			},
			[this, viewIndex, state, outputId, &renderDevice](
			const RenderPassContext& pass
		) {
				const RenderViewInput& view = mFrameViews[viewIndex];
				if (view.screenSprites.empty()) {
					return;
				}
				static bool sLoggedScreenSpritePassCount = false;
				if (!sLoggedScreenSpritePassCount) {
					DevMsg(
						"Renderer",
						"ScreenSprites pass pre-draw: viewKey='{}', inputCount={}.",
						view.viewKey,
						view.screenSprites.size()
					);
					sLoggedScreenSpritePassCount = true;
				}

				auto& allocator = dynamic_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();

				Rhi::FrameConstants frame = {};
				frame.view                = Mat4::identity;
				frame.proj                = Mat4::MakeOrthographicMat(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight),
					0.0f,
					1.0f
				);
				frame.viewProj = frame.view * frame.proj;
				const D3D12_GPU_VIRTUAL_ADDRESS frameCb =
					allocator.AllocateConstantBuffer(&frame, sizeof(frame));

				pass.SetViewportAndScissor(
					0.0f,
					0.0f,
					static_cast<float>(state.logicalWidth),
					static_cast<float>(state.logicalHeight)
				);
				pass.SetSrvUavHeap();
				pass.SetRenderTarget(outputId);

				int textSamplerMode = 0;
				if (mConsole != nullptr) {
					const auto* samplerModeVar = mConsole->GetConVarAs<
						ConVar<int>>(
						"r_ui_text_sampler_mode"
					);
					if (samplerModeVar != nullptr) {
						textSamplerMode =
							std::clamp(samplerModeVar->GetValue(), 0, 2);
					}
				}

				const GeometryPassRes* spriteGeom = &mSpritePass.geom;
				if (textSamplerMode != 0) {
					static bool sLoggedSamplerFallback = false;
					if (!sLoggedSamplerFallback) {
						Warning(
							"Renderer",
							"r_ui_text_sampler_mode={} requested, but sampler comparison path is temporarily disabled. Falling back to Default PSO.",
							textSamplerMode
						);
						sLoggedSamplerFallback = true;
					}
				}

				static int sLoggedSamplerMode = -1;
				if (sLoggedSamplerMode != textSamplerMode) {
					DevMsg(
						"Renderer",
						"ScreenSprite sampler mode changed: {} (0=Default, 1=LinearClamp, 2=PointClamp).",
						textSamplerMode
					);
					sLoggedSamplerMode = textSamplerMode;
				}

				if (!spriteGeom->resolved || !spriteGeom->resolved->pso) {
					Warning(
						"Renderer",
						"ScreenSprite pipeline is invalid. UI sprite rendering skipped for this pass."
					);
					return;
				}
				pass.SetGraphicsPipeline(
					spriteGeom->resolved->rootSignature,
					spriteGeom->resolved->pso
				);
				pass.BindGraphicsCbv(
					ToRootIndex(GEOM_ROOT_SLOT::FRAME), frameCb
				);
				pass.SetVertexBuffer(mSpritePass.geom.vbv);
				pass.SetIndexBuffer(mSpritePass.geom.ibv);

				for (const auto& sprite : view.screenSprites) {
					const auto center = Vec2(
						sprite.positionPx.x +
						(0.5f - sprite.anchor.x) * sprite.sizePx.x,
						sprite.positionPx.y +
						(0.5f - sprite.anchor.y) * sprite.sizePx.y
					);

					Rhi::ObjectConstants object = {};
					object.world                = Mat4::Scale(
						               Vec3(
							               sprite.sizePx.x *
							               0.5f,
							               sprite.sizePx.y *
							               0.5f,
							               1.0f
						               )
					               ) * Mat4::RotateZ(
						               sprite.rotationRad
					               ) * Mat4::Translate(
						               Vec3(center.x, center.y,
						                    0.0f)
					               );
					object.worldInverseTranspose = Mat4::identity;
					// 使わんので単位
					const float uvMinY = sprite.uvFlipY ?
						                     sprite.uvMax.y :
						                     sprite.uvMin.y;
					const float uvMaxY = sprite.uvFlipY ?
						                     sprite.uvMin.y :
						                     sprite.uvMax.y;
					const float uvMinX  = sprite.uvMin.x;
					const float uvMaxX  = sprite.uvMax.x;
					object.skinningInfo = Vec4(
						uvMinX, uvMinY, uvMaxX, uvMaxY);

					Rhi::MaterialConstants material = {};
					material.baseColor              = sprite.color;
					material.opacity                = sprite.color.w;
					material.domainMode             = 0.0f;

					const D3D12_GPU_VIRTUAL_ADDRESS objectCb =
						allocator.AllocateConstantBuffer(
							&object, sizeof(object)
						);
					const D3D12_GPU_VIRTUAL_ADDRESS materialCb =
						allocator.AllocateConstantBuffer(
							&material, sizeof(material)
						);

					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::OBJECT), objectCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::MATERIAL), materialCb
					);
					pass.BindGraphicsCbv(
						ToRootIndex(GEOM_ROOT_SLOT::SKINNING), objectCb
					);
					pass.BindGraphicsSrvTable(
						ToRootIndex(GEOM_ROOT_SLOT::BASE_COLOR_TEXTURE),
						ResolveSpriteTexture(
							renderDevice, sprite.texture
						)
					);
					pass.DrawIndexedTest(mSpritePass.geom.indexCount);
				}
			}
		);
	}

	void Renderer::AddPrepareUiViewOutputsPass(
		const std::vector<RenderViewInput>& frameViews
	) {
		// Pass: Prepare UI view outputs.
		// Input: exposed mViewStates outputTextureId values.
		// Output: no draw output; declares SRV reads for UI-visible view textures.
		// PSO: none.
		// RootSignature: none.
		// RenderTarget: none.
		// DepthStencil: none.
		// DescriptorHeap: none.
		// ResourceState: ReadSrvPs(each outputTextureId for views with output.exposeToUi).
		// Notes: This is a graph state/preparation pass for editor UI sampling.
		std::vector<uint32_t> uiReadableOutputs;
		uiReadableOutputs.reserve(frameViews.size());
		for (const RenderViewInput& view : frameViews) {
			if (!view.output.exposeToUi) {
				continue;
			}
			const auto it = mViewStates.find(view.viewKey);
			if (it == mViewStates.end() || it->second.outputTextureId == 0) {
				continue;
			}
			uiReadableOutputs.emplace_back(it->second.outputTextureId);
		}
		if (!uiReadableOutputs.empty()) {
			mGraph.AddPass(
				"PrepareUiViewOutputs",
				[uiReadableOutputs](RenderGraphBuilder& b) {
					for (const uint32_t texId : uiReadableOutputs) {
						b.ReadSrvPs(texId);
					}
				},
				[](RenderPassContext&) {
				}
			);
		}
	}

	void Renderer::AddPresentPass(RenderDevice& renderDevice) {
		// パス: フルスクリーン サンプルの提示。
		// 入力: mPresentViewKey outputTextureId SRV。
		// 出力: スワップチェーン バックバッファ レンダーターゲット。
		// PSO: mFullscreenPass.resolved->pso。
		// ルートシグネチャ: mFullscreenPass.resolved->rootSignature (フルスクリーン FS_ROOT_SLOT バインディング)。
		// レンダーターゲット: RenderGraph バックバッファ。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: ReadSrvPs(presentTexture), WriteBackBufferRt()。
		// 注記: バックバッファをクリア、選択されたビューをアスペクトフィット、フルスクリーン三角形を描画。
		if (!mPresentViewKey.empty()) {
			const auto presentIt = mViewStates.find(mPresentViewKey);
			if (
				presentIt != mViewStates.end() &&
				presentIt->second.outputTextureId != 0
			) {
				const uint32_t presentTexture = presentIt->second.
					outputTextureId;
				const uint32_t presentLogicalWidth = std::max(
					1u, presentIt->second.logicalWidth
				);
				const uint32_t presentLogicalHeight = std::max(
					1u, presentIt->second.logicalHeight
				);
				const uint32_t presentAllocatedWidth = std::max(
					1u, presentIt->second.allocatedWidth
				);
				const uint32_t presentAllocatedHeight = std::max(
					1u, presentIt->second.allocatedHeight
				);
				mGraph.AddPass(
					"FullscreenSampleSrv",
					[presentTexture](RenderGraphBuilder& b) {
						b.ReadSrvPs(presentTexture);
						b.WriteBackBufferRt();
					},
					[
						this,
						presentTexture,
						presentLogicalWidth,
						presentLogicalHeight,
						presentAllocatedWidth,
						presentAllocatedHeight,
						&renderDevice
					](
					const RenderPassContext& pass
				) {
						pass.ClearBackBuffer(0.0f, 0.0f, 0.0f, 1.0f);
						pass.SetSrvUavHeap();

						PostFxParamsConstants params = {};
						params.scalar0.x             = std::clamp(
							static_cast<float>(presentLogicalWidth) /
							static_cast<float>(presentAllocatedWidth),
							0.0f,
							1.0f
						);
						params.scalar0.y = std::clamp(
							static_cast<float>(presentLogicalHeight) /
							static_cast<float>(presentAllocatedHeight),
							0.0f,
							1.0f
						);
						auto& allocator = static_cast<Rhi::D3D12Device&>(
							renderDevice.GetRhiDevice()
						).GetFrameUploadAllocator();
						const D3D12_GPU_VIRTUAL_ADDRESS postFxCb =
							allocator.AllocateConstantBuffer(
								&params, sizeof(params)
							);
						const FitRect fitRect = ComputeAspectFitRect(
							pass.GetBackBufferWidth(),
							pass.GetBackBufferHeight(),
							presentLogicalWidth,
							presentLogicalHeight
						);
						pass.SetViewportAndScissor(
							fitRect.x,
							fitRect.y,
							fitRect.width,
							fitRect.height
						);

						if (!mFullscreenPass.resolved || !mFullscreenPass.
						    resolved->pso) {
							return;
						}
						pass.SetGraphicsPipeline(
							mFullscreenPass.resolved->rootSignature,
							mFullscreenPass.resolved->pso
						);

						pass.BindGraphicsCbv(
							ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS), postFxCb
						);
						pass.BindGraphicsSrvTable(
							ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE),
							presentTexture
						);
						pass.DrawFullscreenTriangle();
					}
				);
			}
		}
	}

	void Renderer::AddShadowMapDebugPass(RenderDevice& renderDevice) {
		// パス: シャドウマップ デバッグ オーバーレイ。
		// 入力: mDirectionalShadow.shadowDepthTextureId SRV。
		// 出力: スワップチェーン バックバッファ レンダーターゲット オーバーレイ。
		// PSO: mDepthVisPass.resolved->pso。
		// ルートシグネチャ: mDepthVisPass.resolved->rootSignature。
		// レンダーターゲット: RenderGraph バックバッファ。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: RenderPassContext::SetSrvUavHeap 経由の D3D12Device SRV/UAV ヒープ。
		// リソースステート: ReadSrvPs(mDirectionalShadow.shadowDepthTextureId), WriteBackBufferRt()。
		// 注記: デフォルトでは無効。r_shadowmap_debug で有効化。
		if (
			!mConsole ||
			!mConsole->GetConVarValueOr("r_shadowmap_debug", false) ||
			!mDirectionalShadow.enabled ||
			mDirectionalShadow.shadowDepthTextureId == 0
		) {
			return;
		}
		const uint32_t shadowDepthId =
			mDirectionalShadow.shadowDepthTextureId;
		const uint32_t requestedSize = static_cast<uint32_t>(
			std::clamp(
				mConsole->GetConVarValueOr("r_shadowmap_debug_size", 256),
				64,
				1024
			)
		);
		mGraph.AddPass(
			"ShadowMapDebugOverlay",
			[shadowDepthId](RenderGraphBuilder& b) {
				b.ReadSrvPs(shadowDepthId);
				b.WriteBackBufferRt();
			},
			[this, shadowDepthId, requestedSize, &renderDevice](
			const RenderPassContext& pass
		) {
				if (!mDepthVisPass.resolved || !mDepthVisPass.resolved->pso) {
					return;
				}
				pass.SetSrvUavHeap();
				const float size = static_cast<float>(
					std::min(
						requestedSize,
						std::max(
							1u,
							std::min(
								pass.GetBackBufferWidth(),
								pass.GetBackBufferHeight()
							)
						)
					)
				);
				pass.SetViewportAndScissor(16.0f, 16.0f, size, size);
				pass.SetGraphicsPipeline(
					mDepthVisPass.resolved->rootSignature,
					mDepthVisPass.resolved->pso
				);

				const PostFxParamsConstants params = {};
				auto& allocator = static_cast<Rhi::D3D12Device&>(
					renderDevice.GetRhiDevice()
				).GetFrameUploadAllocator();
				const D3D12_GPU_VIRTUAL_ADDRESS paramsCb =
					allocator.AllocateConstantBuffer(
						&params,
						sizeof(params)
					);
				pass.BindGraphicsCbv(
					ToRootIndex(FS_ROOT_SLOT::POST_FX_PARAMS),
					paramsCb
				);
				pass.BindGraphicsSrvTable(
					ToRootIndex(FS_ROOT_SLOT::SOURCE_TEXTURE),
					shadowDepthId
				);
				pass.DrawFullscreenTriangle();
			}
		);
	}

	void Renderer::AddEditorBackBufferClearPass(
		const std::vector<RenderViewInput>& frameViews
	) {
		// パス: エディタ バックバッファ クリア。
		// 入力: フレーム ビュー出力フラグ。
		// 出力: スワップチェーン バックバッファ クリア。
		// PSO: なし。
		// ルートシグネチャ: なし。
		// レンダーターゲット: RenderGraph バックバッファ。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: なし。
		// リソースステート: WriteBackBufferRt()。
		// 注記: ビューが選択されていない場合にのみ実行され、ビューがスワップチェーンクリアをリクエストする場合のみ。
		bool clearBackBuffer = false;
		for (const RenderViewInput& view : frameViews) {
			if (view.output.clearSwapChainWhenNotPresenting) {
				clearBackBuffer = true;
				break;
			}
		}
		if (clearBackBuffer) {
			mGraph.AddPass(
				"EditorBackBufferClearPass",
				[](RenderGraphBuilder& b) {
					b.WriteBackBufferRt();
					b.ClearColor(
						RenderGraph::kBackBufferId,
						0.02f,
						0.02f,
						0.02f,
						1.0f
					);
				},
				[](RenderPassContext&) {
				}
			);
		}
	}

	void Renderer::AddImGuiMainPass() {
		// パス: ImGui メイン。
		// 入力: mUiMainRenderCallback ImGui 描画データ コールバック。
		// 出力: スワップチェーン バックバッファ 書き込み。
		// PSO: コールバック オーナーの ImGui パイプライン ステート。
		// ルートシグネチャ: コールバック オーナーの ImGui ルートシグネチャ。
		// レンダーターゲット: RenderGraph バックバッファ。
		// 深度ステンシル: なし。
		// ディスクリプタヒープ: コールバック オーナーの ImGui ディスクリプタ バインディング。
		// リソースステート: WriteBackBufferRt()。
		// 注記: RendererGraph はパスをスケジュール化し、コールバック を呼び出すのみ。
		mGraph.AddPass(
			"ImGuiMainPass",
			[](RenderGraphBuilder& b) {
				b.WriteBackBufferRt();
			},
			[this](RenderPassContext& pass) {
				if (mUiMainRenderCallback) {
					mUiMainRenderCallback(pass);
				}
			}
		);
	}
}
