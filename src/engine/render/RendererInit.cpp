#include "Renderer.h"

#include <string>

#include "RenderDevice.h"
#include "RendererPipelineCatalog.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/ShaderProgramAssetData.h"
#include "core/assets/types/ShaderSourceAssetData.h"
#include "core/filesystem/VirtualPath.h"

#include "engine/content/ContentMountDefinitions.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	namespace {
		Rhi::VertexLayoutDesc BuildGeometryVertexLayout() {
			return Rhi::VertexLayoutDesc{
				.stride   = sizeof(float) * 20,
				.elements = {
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::POSITION,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT3,
						.offset           = 0,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::NORMAL,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT3,
						.offset           = sizeof(float) * 3,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::TEXCOORD,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT2,
						.offset           = sizeof(float) * 6,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::TANGENT,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT4,
						.offset           = sizeof(float) * 8,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::TEXCOORD,
						.semanticIndex    = 1,
						.format           = Rhi::VertexFormat::FLOAT4,
						.offset           = sizeof(float) * 12,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::TEXCOORD,
						.semanticIndex    = 2,
						.format           = Rhi::VertexFormat::FLOAT4,
						.offset           = sizeof(float) * 16,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
				}
			};
		}

		Rhi::VertexLayoutDesc BuildSpriteVertexLayout() {
			return Rhi::VertexLayoutDesc{
				.stride   = sizeof(float) * 5,
				.elements = {
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::POSITION,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT3,
						.offset           = 0,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::TEXCOORD,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT2,
						.offset           = sizeof(float) * 3,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
				}
			};
		}

		Rhi::VertexLayoutDesc BuildLineVertexLayout() {
			return Rhi::VertexLayoutDesc{
				.stride   = sizeof(float) * 7,
				.elements = {
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::POSITION,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT3,
						.offset           = 0,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::COLOR,
						.semanticIndex    = 0,
						.format           = Rhi::VertexFormat::FLOAT4,
						.offset           = sizeof(float) * 3,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
				}
			};
		}
	}

	AssetID Renderer::LoadCoreAsset(
		AssetManager&          assetManager,
		const std::string_view virtualPathText,
		const ASSET_TYPE       type
	) {
		const std::optional<VirtualPath> virtualPath =
			VirtualPath::ParseContentReference(virtualPathText);
		if (!virtualPath.has_value()) {
			Error(
				"Renderer",
				"Invalid Core renderer asset virtual path: path={}, type={}",
				virtualPathText,
				ToString(type)
			);
			return kInvalidAssetID;
		}

		return assetManager.LoadAssetFromMount(
			*virtualPath,
			ContentMountId::kCore,
			type
		);
	}

	bool Renderer::ValidateShaderProgramStages(
		const AssetManager&         assetManager,
		const AssetID              shaderProgramId,
		const RequiredShaderStages requiredStages,
		const std::string_view      debugName
	) {
		const auto* shaderProgram = assetManager.Get<ShaderProgramAssetData>(
			shaderProgramId
		);
		if (!shaderProgram) {
			Error(
				"Renderer",
				"Required ShaderProgram is unavailable: name='{}' assetId={}",
				debugName,
				shaderProgramId
			);
			return false;
		}

		auto validateStage = [&](
			const std::optional<ShaderProgramStage>& stage,
			const std::string_view stageName
		) {
			if (
				!stage.has_value() ||
				stage->shaderSourceAssetId == kInvalidAssetID ||
				!assetManager.Get<ShaderSourceAssetData>(
					stage->shaderSourceAssetId)
			) {
				Error(
					"Renderer",
					"Required ShaderProgram stage is unavailable: name='{}' assetId={} stage='{}'",
					debugName,
					shaderProgramId,
					stageName
				);
				return false;
			}
			return true;
		};

		if (requiredStages == RequiredShaderStages::Compute) {
			return validateStage(shaderProgram->cs, "cs");
		}
		const bool vsValid = validateStage(shaderProgram->vs, "vs");
		const bool psValid = validateStage(shaderProgram->ps, "ps");
		return vsValid && psValid;
	}

	Renderer::Renderer(ConsoleSystem* console) : mConsole(console) {
	}

	bool Renderer::Init(
		RenderDevice& renderDevice,
		const RendererStartupValidationPolicy validationPolicy
	) {
		mStartupValidationPolicy = validationPolicy;
		auto& dx = dynamic_cast<Rhi::D3D12Device&>(renderDevice.GetRhiDevice());
		mTextureResourceCache.Initialize(
			&renderDevice.GetAssetManager(), &renderDevice.GetRegistry()
		);
		mTextureResourceCache.SetUnusedFrameThreshold(120);
		mLastTextureCacheStatsLogFrame = 0;
		if (!RebuildPipelineCatalog(renderDevice, dx, validationPolicy)) {
			return false;
		}

		mFrameCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight(), L"FrameConstants"
		);
		mObjectCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight() * kMaxDrawObjects,
			L"ObjectCB"
		);
		mMaterialCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight() * kMaxDrawObjects,
			L"MaterialCB"
		);
		mSkinningCb.Init(
			dx.GetDevice(), dx.GetFramesInFlight() * kMaxDrawObjects,
			L"SkinningPaletteCB"
		);
		InitializeDebugLineResources(dx);

		CreateTriangleTestResources(dx);
		CreateQuadResources(dx);
		CreateSkyboxCubeResources(dx);
		mBillboardPass.depthGeom.vb         = mSpritePass.geom.vb;
		mBillboardPass.depthGeom.ib         = mSpritePass.geom.ib;
		mBillboardPass.depthGeom.vbv        = mSpritePass.geom.vbv;
		mBillboardPass.depthGeom.ibv        = mSpritePass.geom.ibv;
		mBillboardPass.depthGeom.indexCount = mSpritePass.geom.indexCount;
		mBillboardPass.frontGeom.vb         = mSpritePass.geom.vb;
		mBillboardPass.frontGeom.ib         = mSpritePass.geom.ib;
		mBillboardPass.frontGeom.vbv        = mSpritePass.geom.vbv;
		mBillboardPass.frontGeom.ibv        = mSpritePass.geom.ibv;
		mBillboardPass.frontGeom.indexCount = mSpritePass.geom.indexCount;
		LoadSceneMeshResources(renderDevice, dx);
		LoadMaterialResources(renderDevice, dx);
		renderDevice.GetRegistry().OnResize(
			dx.GetSwapChain().GetWidth(),
			dx.GetSwapChain().GetHeight(),
			dx.GetSwapChain().GetCurrentBackBufferIndex()
		);
		mGraph.Reset();
		mGraphBuilt = false;
		return true;
	}

	bool Renderer::RebuildPipelineCatalog(
		RenderDevice& renderDevice,
		Rhi::D3D12Device& dx,
		const RendererStartupValidationPolicy validationPolicy
	) {
		auto& assetManager = renderDevice.GetAssetManager();

		mPipelineRegistry.Clear();
		// Pipeline handles become invalid after catalog rebuild; material bindings rebuild their variants on next load.
		ReleaseMaterialBindings(renderDevice);

		const AssetID fullscreenProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/fullscreen_copy.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID depthVisProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/depth_vis.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID depthOnlyProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/depth_only.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID geomProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/pbr.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID skyboxProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/skybox.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID csProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/cs_write_uav.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID spriteOverlayProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/sprite_overlay.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID debugLineProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/DebugLine.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID bloomDownsampleProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/bloom_downsample.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID bloomUpsampleProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/bloom_upsample.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID bloomCombineProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/bloom_combine.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID toneMapExposureProgramId = LoadCoreAsset(
			assetManager,
			"shaders/programs/tonemap_exposure.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);

		bool requiredShadersValid = true;
		auto validateGraphics = [&](
			const AssetID id, const std::string_view name
		) {
			requiredShadersValid = ValidateShaderProgramStages(
				assetManager, id, RequiredShaderStages::Graphics, name
			) && requiredShadersValid;
		};
		validateGraphics(fullscreenProgramId, "FullscreenCopy");
		validateGraphics(depthVisProgramId, "DepthVis");
		validateGraphics(depthOnlyProgramId, "DepthOnly");
		validateGraphics(geomProgramId, "Geometry");
		validateGraphics(skyboxProgramId, "Skybox");
		validateGraphics(spriteOverlayProgramId, "SpriteOverlay");
		validateGraphics(debugLineProgramId, "DebugLine");
		validateGraphics(bloomDownsampleProgramId, "BloomDownsample");
		validateGraphics(bloomUpsampleProgramId, "BloomUpsample");
		validateGraphics(bloomCombineProgramId, "BloomCombine");
		validateGraphics(toneMapExposureProgramId, "ToneMapExposure");
		requiredShadersValid = ValidateShaderProgramStages(
			assetManager,
			csProgramId,
			RequiredShaderStages::Compute,
			"ComputeWriteUav"
		) && requiredShadersValid;
		if (!requiredShadersValid) {
			Error("Renderer", "Required Renderer shader assets are invalid.");
			return false;
		}

		const DXGI_FORMAT swapChainFormat = Rhi::ToDxgiFormat(
			dx.GetSwapChain().GetFormat()
		);
		const Rhi::VertexLayoutDesc geometryLayout =
			BuildGeometryVertexLayout();
		const Rhi::VertexLayoutDesc spriteLayout = BuildSpriteVertexLayout();
		const Rhi::VertexLayoutDesc lineLayout   = BuildLineVertexLayout();
		mGeometryShaderProgramId                 = geomProgramId;
		mGeometryVertexLayout                    = geometryLayout;

		mFullscreenPass.pipeline = mPipelineRegistry.RegisterGraphics(
			RendererPipelineCatalog::MakeFullscreenPreset(
				"FullscreenCopy",
				fullscreenProgramId,
				dx.GetFsRootSignature(),
				swapChainFormat
			)
		);
		mFullscreenPass.resolved = nullptr;

		mHdrCopyPass.pipeline = mPipelineRegistry.RegisterGraphics(
			RendererPipelineCatalog::MakeFullscreenPreset(
				"HdrCopy",
				fullscreenProgramId,
				dx.GetFsRootSignature(),
				kSceneHdrColorFormat
			)
		);
		mHdrCopyPass.resolved = nullptr;

		mToneMapPass.pipeline = mPipelineRegistry.RegisterGraphics(
			RendererPipelineCatalog::MakeFullscreenPreset(
				"ToneMapExposure",
				toneMapExposureProgramId,
				dx.GetFsRootSignature(),
				kSceneLdrColorFormat
			)
		);
		mToneMapPass.resolved = nullptr;

		auto bloomDownsampleSpec =
			RendererPipelineCatalog::MakeFullscreenPreset(
				"BloomDownsample",
				bloomDownsampleProgramId,
				dx.GetFsRootSignature(),
				kSceneHdrColorFormat
			);
		mBloomDownsamplePass.pipeline = mPipelineRegistry.RegisterGraphics(
			bloomDownsampleSpec
		);
		mBloomDownsamplePass.resolved = nullptr;

		auto bloomUpsampleSpec = RendererPipelineCatalog::MakeFullscreenPreset(
			"BloomUpsample",
			bloomUpsampleProgramId,
			dx.GetFsRootSignature(),
			kSceneHdrColorFormat
		);
		bloomUpsampleSpec.psoTemplate.blendEnable = true;
		bloomUpsampleSpec.psoTemplate.srcBlend = D3D12_BLEND_ONE;
		bloomUpsampleSpec.psoTemplate.destBlend = D3D12_BLEND_ONE;
		bloomUpsampleSpec.psoTemplate.srcBlendAlpha = D3D12_BLEND_ONE;
		bloomUpsampleSpec.psoTemplate.destBlendAlpha = D3D12_BLEND_ONE;
		mBloomUpsamplePass.pipeline = mPipelineRegistry.RegisterGraphics(
			bloomUpsampleSpec
		);
		mBloomUpsamplePass.resolved = nullptr;

		auto bloomCombineSpec = RendererPipelineCatalog::MakeFullscreenPreset(
			"BloomCombine",
			bloomCombineProgramId,
			dx.GetFsRootSignature(),
			kSceneHdrColorFormat
		);
		bloomCombineSpec.psoTemplate.blendEnable = true;
		bloomCombineSpec.psoTemplate.srcBlend = D3D12_BLEND_ONE;
		bloomCombineSpec.psoTemplate.destBlend = D3D12_BLEND_ONE;
		bloomCombineSpec.psoTemplate.srcBlendAlpha = D3D12_BLEND_ONE;
		bloomCombineSpec.psoTemplate.destBlendAlpha = D3D12_BLEND_ONE;
		mBloomCombinePass.pipeline = mPipelineRegistry.RegisterGraphics(
			bloomCombineSpec
		);
		mBloomCombinePass.resolved = nullptr;

		mDepthVisPass.pipeline = mPipelineRegistry.RegisterGraphics(
			RendererPipelineCatalog::MakeFullscreenPreset(
				"DepthVis",
				depthVisProgramId,
				dx.GetFsRootSignature(),
				swapChainFormat
			)
		);
		mDepthVisPass.resolved = nullptr;

		mComputePass.pipeline = mPipelineRegistry.RegisterCompute(
			RendererPipelineCatalog::MakeComputePreset(
				"ComputeWriteUav",
				csProgramId,
				dx.GetCsRootSignature()
			)
		);
		mComputePass.resolved = nullptr;

		mGeometryPass.pipeline = mPipelineRegistry.RegisterGraphics(
			RendererPipelineCatalog::MakeGeometryPreset(
				"Geometry",
				geomProgramId,
				dx.GetGeomRootSignature(),
				kSceneHdrColorFormat,
				DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
				geometryLayout
			)
		);
		mGeometryPass.resolved = nullptr;

		auto shadowDepthSpec = RendererPipelineCatalog::MakeGeometryPreset(
			"ShadowDepthOnly",
			depthOnlyProgramId,
			dx.GetGeomRootSignature(),
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_D32_FLOAT,
			geometryLayout
		);
		shadowDepthSpec.psoTemplate.numRenderTargets = 0;
		shadowDepthSpec.psoTemplate.rtvFormat        =
			DXGI_FORMAT_UNKNOWN;
		shadowDepthSpec.psoTemplate.depthEnable      = true;
		shadowDepthSpec.psoTemplate.depthWriteEnable = true;
		shadowDepthSpec.psoTemplate.depthFunc        =
			D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		shadowDepthSpec.psoTemplate.blendEnable = false;
		mShadowDepthPass.pipeline = mPipelineRegistry.RegisterGraphics(
			shadowDepthSpec
		);
		mShadowDepthPass.resolved = nullptr;

		auto shadowDepthFrontCullSpec = shadowDepthSpec;
		shadowDepthFrontCullSpec.debugName = "ShadowDepthOnlyFrontCull";
		shadowDepthFrontCullSpec.psoTemplate.cullMode =
			D3D12_CULL_MODE_FRONT;
		mShadowDepthFrontCullPass.pipeline =
			mPipelineRegistry.RegisterGraphics(shadowDepthFrontCullSpec);
		mShadowDepthFrontCullPass.resolved = nullptr;

		auto shadowDepthDoubleSidedSpec = shadowDepthSpec;
		shadowDepthDoubleSidedSpec.debugName = "ShadowDepthOnlyDoubleSided";
		shadowDepthDoubleSidedSpec.psoTemplate.cullMode =
			D3D12_CULL_MODE_NONE;
		mShadowDepthDoubleSidedPass.pipeline =
			mPipelineRegistry.RegisterGraphics(shadowDepthDoubleSidedSpec);
		mShadowDepthDoubleSidedPass.resolved = nullptr;

		auto skyboxSpec = RendererPipelineCatalog::MakeGeometryPreset(
			"Skybox",
			skyboxProgramId,
			dx.GetGeomRootSignature(),
			kSceneHdrColorFormat,
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
			geometryLayout
		);
		skyboxSpec.psoTemplate.cullMode = D3D12_CULL_MODE_NONE;
		mSkyboxPass.geom.pipeline       = mPipelineRegistry.RegisterGraphics(
			skyboxSpec);
		mSkyboxPass.geom.resolved = nullptr;

		auto spriteSpec = RendererPipelineCatalog::MakeSpritePreset(
			"ScreenSprite",
			spriteOverlayProgramId,
			dx.GetGeomRootSignature(),
			kSceneLdrColorFormat,
			spriteLayout
		);
		mSpritePass.geom.pipeline = mPipelineRegistry.RegisterGraphics(
			spriteSpec);
		mSpritePass.geom.resolved = nullptr;

		auto spriteLinearClampSpec          = spriteSpec;
		spriteLinearClampSpec.debugName     = "ScreenSpriteLinearClamp";
		spriteLinearClampSpec.rootSignature =
			dx.GetGeomRootSignatureLinearClamp();
		mSpritePass.geomLinearClamp.pipeline = mPipelineRegistry.
			RegisterGraphics(
				spriteLinearClampSpec
			);
		mSpritePass.geomLinearClamp.resolved = nullptr;

		auto spritePointClampSpec          = spriteSpec;
		spritePointClampSpec.debugName     = "ScreenSpritePointClamp";
		spritePointClampSpec.rootSignature =
			dx.GetGeomRootSignaturePointClamp();
		mSpritePass.geomPointClamp.pipeline = mPipelineRegistry.
			RegisterGraphics(
				spritePointClampSpec
			);
		mSpritePass.geomPointClamp.resolved = nullptr;

		auto billboardDepthSpec                         = spriteSpec;
		billboardDepthSpec.debugName                    = "WorldBillboardDepth";
		billboardDepthSpec.psoTemplate.rtvFormat        = kSceneHdrColorFormat;
		billboardDepthSpec.psoTemplate.depthEnable      = true;
		billboardDepthSpec.psoTemplate.depthWriteEnable = true;
		billboardDepthSpec.psoTemplate.dsvFormat        =
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		billboardDepthSpec.psoTemplate.depthFunc =
			D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		mBillboardPass.depthGeom.pipeline = mPipelineRegistry.RegisterGraphics(
			billboardDepthSpec
		);
		mBillboardPass.depthGeom.resolved = nullptr;

		auto billboardFrontSpec                         = billboardDepthSpec;
		billboardFrontSpec.debugName                    = "WorldBillboardFront";
		billboardFrontSpec.psoTemplate.depthEnable      = false;
		billboardFrontSpec.psoTemplate.depthWriteEnable = false;
		billboardFrontSpec.psoTemplate.dsvFormat        = DXGI_FORMAT_UNKNOWN;
		billboardFrontSpec.psoTemplate.depthFunc        =
			D3D12_COMPARISON_FUNC_ALWAYS;
		mBillboardPass.frontGeom.pipeline = mPipelineRegistry.RegisterGraphics(
			billboardFrontSpec
		);
		mBillboardPass.frontGeom.resolved = nullptr;

		mLinePass.pipeline = mPipelineRegistry.RegisterGraphics(
			RendererPipelineCatalog::MakeLinePreset(
				"DebugLine",
				debugLineProgramId,
				dx.GetGeomRootSignature(),
				kSceneHdrColorFormat,
				DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
				lineLayout
			)
		);
		mLinePass.resolved = nullptr;

		if (!LoadPostFxChain(renderDevice)) {
			if (validationPolicy == RendererStartupValidationPolicy::Strict) {
				Error(
					"Renderer",
					"Default PostFxChain is required by strict startup validation."
				);
				return false;
			}
			Warning(
				"Renderer",
				"Default PostFxChain is unavailable; continuing without post effects."
			);
		}
		return true;
	}
}
