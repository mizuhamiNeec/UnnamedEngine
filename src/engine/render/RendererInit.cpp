#include "Renderer.h"

#include <string>

#include "RenderDevice.h"
#include "RendererPipelineCatalog.h"

#include "core/assets/AssetManager.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"

namespace Unnamed::Render {
	namespace {
		AssetID LoadAsset(
			AssetManager&      assetManager,
			const std::string& path,
			const ASSET_TYPE   type
		) {
			return assetManager.LoadFromFile(path, type);
		}

		Rhi::VertexLayoutDesc BuildGeometryVertexLayout() {
			return Rhi::VertexLayoutDesc{
				.stride   = sizeof(float) * 16,
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
						.semantic         = Rhi::VertexSemantic::TEXCOORD,
						.semanticIndex    = 1,
						.format           = Rhi::VertexFormat::FLOAT4,
						.offset           = sizeof(float) * 8,
						.inputSlot        = 0,
						.perInstance      = false,
						.instanceStepRate = 0,
					},
					Rhi::VertexElementDesc{
						.semantic         = Rhi::VertexSemantic::TEXCOORD,
						.semanticIndex    = 2,
						.format           = Rhi::VertexFormat::FLOAT4,
						.offset           = sizeof(float) * 12,
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

	Renderer::Renderer(ConsoleSystem* console) : mConsole(console) {
	}

	void Renderer::Init(RenderDevice& renderDevice) {
		auto& dx = dynamic_cast<Rhi::D3D12Device&>(renderDevice.GetRhiDevice());
		mTextureResourceCache.Initialize(
			&renderDevice.GetAssetManager(), &renderDevice.GetRegistry()
		);
		mTextureResourceCache.SetUnusedFrameThreshold(120);
		mLastTextureCacheStatsLogFrame = 0;
		RebuildPipelineCatalog(renderDevice, dx);

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
	}

	void Renderer::RebuildPipelineCatalog(
		RenderDevice& renderDevice, Rhi::D3D12Device& dx
	) {
		auto& assetManager = renderDevice.GetAssetManager();

		mPipelineRegistry.Clear();
		// Pipeline handles become invalid after catalog rebuild; material bindings rebuild their variants on next load.
		ReleaseMaterialBindings(renderDevice);

		const AssetID fullscreenProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/fullscreen_copy.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID depthVisProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/depth_vis.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID geomProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/pbr.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID skyboxProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/skybox.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID csProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/cs_write_uav.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID spriteOverlayProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/sprite_overlay.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID debugLineProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/DebugLine.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID bloomDownsampleProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/bloom_downsample.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID bloomUpsampleProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/bloom_upsample.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID bloomCombineProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/bloom_combine.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);
		const AssetID toneMapExposureProgramId = LoadAsset(
			assetManager,
			"./content/core/shaders/programs/tonemap_exposure.shader.json",
			ASSET_TYPE::SHADER_PROGRAM
		);

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

		LoadPostFxChain(renderDevice);
	}
}
