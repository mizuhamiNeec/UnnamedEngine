#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "RendererDraw.h"

#include "core/assets/AssetID.h"
#include "core/math/Vec2.h"

#include "engine/rhi/Buffer.h"
#include "engine/rhi/Constants.h"
#include "engine/rhi/UploadBuffer.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "foundation/AdvancedRenderFoundation.h"

#include "frame/RenderFrameInputs.h"

#include "rendergraph/RenderGraph.h"

#include "shaders/PipelineRegistry.h"

namespace Unnamed::Render {
	struct RenderFrameInputs;
	class RenderPassContext;
	class RenderDevice;

	struct SceneOutputView {
		uint32_t                    textureId   = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE srvCpu      = {};
		uint64_t                    srvRevision = 0;
		Vec2                        uvMin       = Vec2(0.0f, 0.0f);
		Vec2                        uvMax       = Vec2(1.0f, 1.0f);
	};

	class Renderer {
	public:
		Renderer(ConsoleSystem* console);

		/// @brief レンダラの初期化処理に呼び出されます。
		/// @param renderDevice 描画に使用するRenderDevice
		void Init(RenderDevice& renderDevice);

		/// @brief 毎フレームの描画処理に呼び出されます。
		/// @param renderDevice 描画に使用するRenderDevice
		/// @param inputs 描画に必要な入力データ
		void RenderFrame(
			RenderDevice& renderDevice, const RenderFrameInputs& inputs
		);

		/// @brief クライアントのリサイズ時の処理に呼び出されます。
		void OnResize(uint32_t width, uint32_t height);

		using UiMainRenderCallback = std::function<void(RenderPassContext&)>;
		using UiPlatformRenderCallback = std::function<void()>;

		void SetUiCallbacks(
			UiMainRenderCallback     mainRenderCallback,
			UiPlatformRenderCallback platformRenderCallback
		);

		[[nodiscard]] SceneOutputView GetViewOutputView(
			const RenderDevice& renderDevice,
			std::string_view    viewKey
		) const;
		[[nodiscard]] Vec2 GetViewOutputSize(std::string_view viewKey) const;

	private:
		// シーンの描画にはHDRを使う!
		static constexpr DXGI_FORMAT kSceneHdrColorFormat =
			DXGI_FORMAT_R16G16B16A16_FLOAT;

		// UIなどはLDRを使う
		static constexpr DXGI_FORMAT kSceneLdrColorFormat =
			DXGI_FORMAT_R8G8B8A8_UNORM;

		/// @brief レンダリンググラフの構築
		/// @param renderDevice 描画に使用するRenderDevice
		void BuildGraph(
			RenderDevice&                       renderDevice,
			const std::vector<RenderViewInput>& frameViews
		);
		static std::pair<uint32_t, uint32_t> ResolveSceneRenderExtent(
			uint32_t                   backBufferWidth,
			uint32_t                   backBufferHeight,
			const SceneViewRenderMode& request
		);

		void CreateTriangleTestResources(Rhi::D3D12Device& dx);
		void CreateQuadResources(Rhi::D3D12Device& dx);
		void CreateSkyboxCubeResources(Rhi::D3D12Device& dx);
		bool EnsureMeshResourceLoaded(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx,
			AssetID       meshAssetId
		);
		void LoadSceneMeshResources(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx
		);
		void LoadMaterialResources(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx
		);
		void LoadPostFxChain(RenderDevice& renderDevice);
		void RebuildPipelineCatalog(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx
		);
		void ResolveRegisteredPipelines(RenderDevice& renderDevice);

		struct FullscreenPassRes {
			PipelineHandle                  pipeline = {};
			const ResolvedGraphicsPipeline* resolved = nullptr;
		};

		struct ComputePassRes {
			PipelineHandle                 pipeline = {};
			const ResolvedComputePipeline* resolved = nullptr;
		};

		struct GeometryPassRes {
			PipelineHandle                  pipeline = {};
			const ResolvedGraphicsPipeline* resolved = nullptr;

			Microsoft::WRL::ComPtr<ID3D12Resource> vb;
			Microsoft::WRL::ComPtr<ID3D12Resource> ib;
			D3D12_VERTEX_BUFFER_VIEW               vbv        = {};
			D3D12_INDEX_BUFFER_VIEW                ibv        = {};
			uint32_t                               indexCount = 0;
			AABB                                   localAABB  = {};
		};

		struct MaterialBinding {
			Rhi::MaterialConstants constants          = {};
			AssetID                materialInstanceId = kInvalidAssetID;
			uint32_t               albedoTextureId    = 0;
			uint32_t               padding0           = 0;
		};

		struct PostFxRuntimePass {
			std::string                            name;
			bool                                   enabled = true;
			std::unordered_map<std::string, float> scalarDefaults;
			std::unordered_map<std::string, Vec4>  colorDefaults;
			FullscreenPassRes                      pass = {};
		};

		struct SpritePassRes {
			GeometryPassRes geom           = {};
			GeometryPassRes geomLinearClamp = {};
			GeometryPassRes geomPointClamp  = {};
		};

		struct BillboardPassRes {
			GeometryPassRes depthGeom = {};
			GeometryPassRes frontGeom = {};
		};

		struct SkyboxPassRes {
			GeometryPassRes geom = {};
		};

		struct DebugLineVertex {
			float px = 0.0f;
			float py = 0.0f;
			float pz = 0.0f;
			float r  = 1.0f;
			float g  = 1.0f;
			float b  = 1.0f;
			float a  = 1.0f;
		};

		struct LinePassRes {
			PipelineHandle                  pipeline = {};
			const ResolvedGraphicsPipeline* resolved = nullptr;

			Microsoft::WRL::ComPtr<ID3D12Resource> dynamicVb;
			DebugLineVertex*                       mappedVertices   = nullptr;
			D3D12_VERTEX_BUFFER_VIEW               frameVbv         = {};
			uint32_t                               vertexCapacity   = 0;
			uint32_t                               frameVertexCount = 0;
		};

		struct ViewRuntimeState {
			RENDER_VIEW_TYPE      type = RENDER_VIEW_TYPE::SCENE;
			RenderViewOutputDesc  output = {};
			uint32_t              logicalWidth = 1;
			uint32_t              logicalHeight = 1;
			uint32_t              allocatedWidth = 1;
			uint32_t              allocatedHeight = 1;
			uint32_t              colorTextureId = 0;
			uint32_t              depthTextureId = 0;
			uint32_t              postFxTextureAId = 0;
			uint32_t              postFxTextureBId = 0;
			std::vector<uint32_t> bloomMipTextureIds = {};
			uint32_t              outputTextureId = 0;
		};

		static constexpr uint32_t kMaxDrawObjects       = 1024; // TODO: とりあえず
		static constexpr uint32_t kMaxDebugLines        = 65536; // TODO: とりあえず

		ConsoleSystem* mConsole = nullptr;

		RenderGraph mGraph;
		PipelineRegistry mPipelineRegistry;

		FullscreenPassRes        mFullscreenPass      = {};
		FullscreenPassRes        mHdrCopyPass         = {};
		FullscreenPassRes        mToneMapPass         = {};
		FullscreenPassRes        mBloomDownsamplePass = {};
		FullscreenPassRes        mBloomUpsamplePass   = {};
		FullscreenPassRes        mBloomCombinePass    = {};
		FullscreenPassRes        mDepthVisPass        = {};
		ComputePassRes           mComputePass         = {};
		GeometryPassRes          mGeometryPass        = {};
		SpritePassRes            mSpritePass          = {};
		BillboardPassRes         mBillboardPass       = {};
		SkyboxPassRes            mSkyboxPass          = {};
		LinePassRes              mLinePass            = {};
		AdvancedRenderFoundation mAdvancedFoundation  = {};

		Rhi::UploadBuffer<Rhi::FrameConstants> mFrameCb;
		Rhi::UploadBuffer<Rhi::ObjectConstants> mObjectCb;
		Rhi::UploadBuffer<Rhi::MaterialConstants> mMaterialCb;
		Rhi::UploadBuffer<Rhi::SkinningPaletteConstants> mSkinningCb;
		std::vector<MeshBuffer> mSceneMeshes;
		std::unordered_map<AssetID, MeshBuffer> mSceneMeshesByAsset;
		AssetID mLoadedMeshAsset = kInvalidAssetID;
		AssetID mDefaultMaterialInstance =
			kInvalidAssetID;
		AssetID mPostFxChainAsset = kInvalidAssetID;
		std::unordered_map<AssetID, MaterialBinding> mMaterialBindings;
		std::vector<PostFxRuntimePass> mPostFxPasses;
		std::unordered_map<AssetID, uint32_t> mSpriteTextureIds;
		std::unordered_map<AssetID, uint32_t> mSkyboxTextureIds;
		uint32_t mSpriteFallbackTextureId = 0;

		std::vector<MeshDrawItem>     mMainDrawList;
		std::vector<DrawBatch>        mMainBatches;
		std::unordered_map<std::string, ViewRuntimeState> mViewStates;
		std::vector<std::string> mViewExecutionOrder;
		std::vector<RenderViewInput> mFrameViews;
		std::vector<DebugLineInput> mFrameDebugLines;
		std::string mPresentViewKey;
		UiMainRenderCallback mUiMainRenderCallback;
		UiPlatformRenderCallback mUiPlatformRenderCallback;

		bool mGraphBuilt           = false;
		Mat4 mBillboardCameraWorld = Mat4::identity;

		uint32_t EnsureSpriteTextureLoaded(
			RenderDevice& renderDevice, AssetID textureAssetId
		);
		uint32_t ResolveSpriteTexture(
			RenderDevice& renderDevice, const SpriteTextureRef& textureRef
		);
		uint32_t EnsureSkyboxTextureLoaded(
			RenderDevice& renderDevice, AssetID textureAssetId
		);
		void        EnsureSpriteFallbackTexture(RenderDevice& renderDevice);
		void        InitializeDebugLineResources(Rhi::D3D12Device& dx);
		void        UploadDebugLinesForFrame();
		static void ReleaseViewRuntimeTextures(
			RenderDevice& renderDevice, ViewRuntimeState& state
		);
	};
}
