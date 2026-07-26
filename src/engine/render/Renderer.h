#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "RenderStartupOptions.h"
#include "TextureResourceCache.h"

#include "core/assets/AssetID.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/MaterialAssetData.h"
#include "core/math/Vec2.h"

#include "engine/rhi/Buffer.h"
#include "engine/rhi/Constants.h"

#include "frame/RenderFrameInputs.h"

#include "rendergraph/RenderGraph.h"
#include "rendergraph/RgResourceRegistry.h"

#include "shaders/PipelineRegistry.h"

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Rhi {
	class D3D12Device;
}

namespace Unnamed::Render {
	struct RenderFrameInputs;
	class RenderPassContext;
	class RenderDevice;

	/// @brief UI などがシーン出力をサンプリングするための SRV と有効 UV 範囲です。
	struct SceneOutputView {
		uint64_t                    srvRevision = 0;          // 8
		D3D12_CPU_DESCRIPTOR_HANDLE srvCpu      = {};         // 8
		Vec2                        uvMin       = Vec2::zero; // 8
		Vec2                        uvMax       = Vec2::one;  // 8
		uint32_t                    textureId   = 0;          // 4
		// 36 パディングにより 40
	};

	/// @brief Geometry material texture table の固定スロット。
	/// @details ORM は R=Ambient Occlusion, G=Perceptual Roughness, B=Metallic。
	enum class MATERIAL_TEXTURE_SLOT : uint8_t {
		BASE_COLOR = 0,
		NORMAL,
		ORM,
		EMISSIVE,
		COUNT,
	};

	/// @brief 解決済み Material texture の RgTextureId セット。
	struct MaterialTextureSet {
		uint32_t baseColorTextureId = 0;
		uint32_t normalTextureId    = 0;
		uint32_t ormTextureId       = 0;
		uint32_t emissiveTextureId  = 0;
	};

	/// @brief フレーム入力から RenderGraph を構築し、シーンおよび UI 描画を記録するレンダラーです。
	/// @details GPU テクスチャの寿命は RenderDevice のレジストリが管理し、このクラスはビュー、
	///          パイプライン、フレームごとのパス構成を管理します。
	class Renderer final {
	public:
		explicit Renderer(ConsoleSystem* console);

		/// @brief Renderer が保持する Registry texture を明示解放します。
		void Shutdown(RenderDevice& renderDevice);

		/// @brief レンダラの初期化処理に呼び出されます。
		/// @param renderDevice 描画に使用するRenderDevice。
		/// @param startupOptions レンダラの起動オプション。
		/// @return 必須Rendererアセットを含む初期化に成功した場合true。
		[[nodiscard]] bool Init(
			RenderDevice&               renderDevice,
			const RenderStartupOptions& startupOptions
		);

		/// @brief 起動シーンから到達可能なMaterial Pipelineを厳格検証します。
		/// @param renderDevice 描画に使用するRenderDevice。
		/// @return 全対象Pipelineの解決に成功した場合true。
		[[nodiscard]] bool ValidateStartupResources(
			RenderDevice& renderDevice
		);

		/// @brief 毎フレームの描画処理に呼び出されます。
		/// @param renderDevice 描画に使用するRenderDevice
		/// @param inputs 描画に必要な入力データ
		void RenderFrame(
			RenderDevice& renderDevice, const RenderFrameInputs& inputs
		);

		using UiMainRenderCallback = std::function<void(RenderPassContext&)>;
		using UiPlatformRenderCallback = std::function<void()>;

		/// @brief RenderGraph の UI パスで呼び出す描画コールバックを設定します。
		/// @details main コールバックは Graph が設定済みのアタッチメントを変更してはいけません。
		void SetUiCallbacks(
			UiMainRenderCallback     mainRenderCallback,
			UiPlatformRenderCallback platformRenderCallback
		);

		/// @brief 指定ビューのサンプリング用出力を返します。存在しない場合は空の値を返します。
		[[nodiscard]] SceneOutputView GetViewOutputView(
			const RenderDevice& renderDevice,
			std::string_view    viewKey
		) const;
		/// @brief 指定ビューの論理解像度を返します。存在しない場合はゼロサイズを返します。
		[[nodiscard]] Vec2 GetViewOutputSize(std::string_view viewKey) const;

	private:
		struct MaterialBinding;

		enum class REQUIRED_SHADER_STAGES : uint8_t {
			GRAPHICS,
			COMPUTE,
		};

		// シーンの描画にはHDRを使う!
		static constexpr DXGI_FORMAT kSceneHdrColorFormat =
			DXGI_FORMAT_R16G16B16A16_FLOAT;

		// UIなどはLDRを使う
		static constexpr DXGI_FORMAT kSceneLdrColorFormat =
			DXGI_FORMAT_R8G8B8A8_UNORM;

		/// @brief 現在フレームのビュー入力からパスとリソース契約を構築します。
		/// @details 実行コールバックは描画だけを記録し、アタッチメントは各 setup 宣言から RenderGraph が設定します。
		/// @param renderDevice 描画に使用するRenderDevice
		/// @param frameViews フレーム内の全てのRenderViewInput
		void BuildGraph(
			RenderDevice&                       renderDevice,
			const std::vector<RenderViewInput>& frameViews
		);
		/// @brief ビューごとの解像度と永続出力リソースの寿命を同期します。
		void SynchronizeViewRuntimeStates(
			uint32_t      backBufferWidth,
			uint32_t      backBufferHeight
		);
		/// @brief 今フレームで参照するテクスチャ、メッシュ、マテリアル、PSO を遅延準備します。
		void PrepareFrameResources(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx
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
			const RenderDevice& renderDevice, Rhi::D3D12Device& dx,
			AssetID             meshAssetId
		);
		void LoadMaterialResources(
			RenderDevice& renderDevice, Rhi::D3D12Device& dx
		);
		void ReleaseMaterialBindings(RenderDevice& renderDevice);
		void EnsureDefaultMaterialTextures(RenderDevice& renderDevice);
		void ReleaseDefaultMaterialTextures(RenderDevice& renderDevice);
		/// @brief Core mount 固定でRenderer内部アセットをロードします。
		/// @param assetManager アセット管理サービス。
		/// @param virtualPathText content root 基準の論理パス。
		/// @param type ロードするアセット型。
		/// @return ロードしたアセットID。失敗時はkInvalidAssetID。
		[[nodiscard]] static AssetID LoadCoreAsset(
			AssetManager&    assetManager,
			std::string_view virtualPathText,
			ASSET_TYPE       type
		);
		/// @brief ShaderProgramが必要なstage sourceを保持するか検証します。
		/// @param assetManager アセット管理サービス。
		/// @param shaderProgramId 検証するShaderProgram ID。
		/// @param requiredStages 必須stage構成。
		/// @param debugName ログ表示名。
		/// @return 必須stageとShaderSourceが全て有効な場合true。
		[[nodiscard]] static bool ValidateShaderProgramStages(
			const AssetManager&    assetManager,
			AssetID                shaderProgramId,
			REQUIRED_SHADER_STAGES requiredStages,
			std::string_view       debugName
		);
		void EnsureMaterialTextureTable(
			RenderDevice& renderDevice, MaterialBinding& binding
		) const;
		[[nodiscard]] bool LoadPostFxChain(
			const RenderDevice& renderDevice
		);
		[[nodiscard]] bool RebuildPipelineCatalog(
			RenderDevice&               renderDevice,
			Rhi::D3D12Device&           dx,
			const RenderStartupOptions& startupOptions
		);
		[[nodiscard]] bool ResolveRegisteredPipelines(
			RenderDevice&          renderDevice,
			PIPELINE_RESOLVE_SCOPE scope =
				PIPELINE_RESOLVE_SCOPE::ALL_REGISTERED
		);

		/// @brief FullscreenPassResは、Fullscreen描画passが再利用するroot signatureとpipeline stateを保持します
		struct FullscreenPassRes {
			PipelineHandle                  pipeline = {};
			const ResolvedGraphicsPipeline* resolved = nullptr;
		};

		/// @brief ComputePassResは、Compute描画passが再利用するroot signatureとpipeline stateを保持します
		struct ComputePassRes {
			PipelineHandle                 pipeline = {};
			const ResolvedComputePipeline* resolved = nullptr;
		};

		/// @brief GeometryPassResは、Geometry描画passが再利用するroot signatureとpipeline stateを保持します
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

		// Current contract:
		// - Material shader used by Geometry pass must be compatible with GeomRootSignature.
		// - Supported bindings are FRAME(b0), OBJECT(b1), MATERIAL(b2), SKINNING(b3),
		//   MaterialTextures(t0..t3), SHADOW_CONSTANTS(b4), SHADOW_MAP(t4),
		//   and ENVIRONMENT_LIGHTING(b5).
		// - MaterialTextures order is BaseColor(t0), Normal(t1), ORM(t2), Emissive(t3).
		// - Custom constant buffers and shader reflection are not supported yet.
		// - Non-compatible shaders may compile/resolve but can fail at draw time.
		/// @brief MaterialBindingは、Materialの論理識別子とruntime resource参照の対応を保持します
		struct MaterialBinding {
			Rhi::MaterialConstants constants = {};
			AssetID materialInstanceId = kInvalidAssetID;
			AssetID shaderProgramId = kInvalidAssetID;
			MaterialRenderStateData renderState = {};
			PipelineHandle geometryPipeline = {};
			const ResolvedGraphicsPipeline* resolvedGeometryPipeline = nullptr;
			MaterialTextureSet textures = {};
			// SRV テーブルと RenderGraph の read 宣言で共有する解決済み ID。
			std::array<uint32_t, 4> resolvedTextureIds            = {};
			RgSrvDescriptorTable    materialTextureTable          = {};
			std::array<uint64_t, 4> materialTextureSrvRevisions   = {};
			bool                    pipelineResolveWarningEmitted = false;
		};

		/// @brief PostFxRuntimePassは、post-process passのmaterial、入出力resource、実行順を保持します
		struct PostFxRuntimePass {
			std::string                            name;
			bool                                   enabled = true;
			std::unordered_map<std::string, float> scalarDefaults;
			std::unordered_map<std::string, Vec4>  colorDefaults;
			FullscreenPassRes                      pass = {};
		};

		/// @brief SpritePassResは、Sprite描画passが再利用するroot signatureとpipeline stateを保持します
		struct SpritePassRes {
			GeometryPassRes geom            = {};
			GeometryPassRes geomLinearClamp = {};
			GeometryPassRes geomPointClamp  = {};
		};

		/// @brief BillboardPassResは、Billboard描画passが再利用するroot signatureとpipeline stateを保持します
		struct BillboardPassRes {
			GeometryPassRes depthGeom = {};
			GeometryPassRes frontGeom = {};
		};

		/// @brief SkyboxPassResは、Skybox描画passが再利用するroot signatureとpipeline stateを保持します
		struct SkyboxPassRes {
			GeometryPassRes geom = {};
		};

		/// @brief DebugLineVertexは、debug line shaderへ渡すworld位置と色の頂点layoutを定義します
		struct DebugLineVertex {
			float px = 0.0f;
			float py = 0.0f;
			float pz = 0.0f;
			float r  = 1.0f;
			float g  = 1.0f;
			float b  = 1.0f;
			float a  = 1.0f;
		};

		/// @brief LinePassResは、Line描画passが再利用するroot signatureとpipeline stateを保持します
		struct LinePassRes {
			PipelineHandle                  pipeline = {};
			const ResolvedGraphicsPipeline* resolved = nullptr;

			Microsoft::WRL::ComPtr<ID3D12Resource> dynamicVb;
			DebugLineVertex*                       mappedVertices   = nullptr;
			D3D12_VERTEX_BUFFER_VIEW               frameVbv         = {};
			uint32_t                               vertexCapacity   = 0;
			uint32_t                               frameVertexCount = 0;
		};

		/// @brief DirectionalShadowRuntimeStateは、directional shadowの有効cascadeとframe resource参照を保持します
		struct DirectionalShadowRuntimeState {
			bool     enabled              = false;
			uint32_t shadowDepthTextureId = 0;
			uint32_t resolution           = 1024;
			Mat4     lightView            = Mat4::identity;
			Mat4     lightProj            = Mat4::identity;
			Mat4     lightViewProj        = Mat4::identity;
			Vec3     lightRayDirection    = Vec3(0.0f, -1.0f, 0.0f);
			Vec3     directionToLight     = Vec3(0.0f, 1.0f, 0.0f);
			Vec3     color                = Vec3::one;
			float    intensity            = 1.0f;
		};

		/// @brief ViewRuntimeStateは、描画viewごとの履歴textureと前frame camera情報を保持します
		struct ViewRuntimeState {
			RENDER_VIEW_TYPE      type               = RENDER_VIEW_TYPE::SCENE;
			RenderViewOutputDesc  output             = {};
			uint32_t              logicalWidth       = 1;
			uint32_t              logicalHeight      = 1;
			uint32_t              allocatedWidth     = 1;
			uint32_t              allocatedHeight    = 1;
			uint32_t              colorTextureId     = 0;
			uint32_t              depthTextureId     = 0;
			uint32_t              postFxTextureAId   = 0;
			uint32_t              postFxTextureBId   = 0;
			std::vector<uint32_t> bloomMipTextureIds = {};
			uint32_t              outputTextureId    = 0;
		};

		/// @brief シーンビューのクリア pass を追加します。
		void AddSceneClearPass(
			const std::string& prefix,
			uint32_t           colorId,
			uint32_t           depthId
		);

		/// @brief シーンビューの skybox pass を追加します。
		void AddSkyboxPass(
			RenderDevice&           renderDevice,
			const std::string&      prefix,
			size_t                  viewIndex,
			const ViewRuntimeState& state,
			uint32_t                skyboxTextureId
		);

		/// @brief シーンビューの mesh geometry pass を追加します。
		void AddGeometryPass(
			RenderDevice&           renderDevice,
			const std::string&      prefix,
			size_t                  viewIndex,
			const ViewRuntimeState& state
		);

		/// @brief directional light shadow map 用 depth-only pass を追加します。
		void AddShadowMapPass(
			RenderDevice&                        renderDevice,
			size_t                               viewIndex,
			const DirectionalShadowRuntimeState& shadowState
		);

		/// @brief depth test ありの world billboard pass を追加します。
		void AddWorldBillboardDepthPass(
			RenderDevice&                renderDevice,
			const std::string&           prefix,
			size_t                       viewIndex,
			const ViewRuntimeState&      state,
			const std::vector<uint32_t>& worldBillboardTextureIds
		);

		/// @brief world sprite pass を追加します。
		void AddWorldSpritePass(
			RenderDevice&                renderDevice,
			const std::string&           prefix,
			size_t                       viewIndex,
			const ViewRuntimeState&      state,
			const std::vector<uint32_t>& worldSpriteTextureIds
		);

		/// @brief debug line pass を追加します。
		void AddDebugLinePass(
			RenderDevice&           renderDevice,
			const std::string&      prefix,
			size_t                  viewIndex,
			const ViewRuntimeState& state
		);

		/// @brief depth test なしの world billboard pass を追加します。
		void AddWorldBillboardFrontPass(
			RenderDevice&                renderDevice,
			const std::string&           prefix,
			size_t                       viewIndex,
			const ViewRuntimeState&      state,
			const std::vector<uint32_t>& worldBillboardTextureIds
		);

		/// @brief シーンビューの post process と tone map pass を追加します。
		void AddScenePostProcessPasses(
			RenderDevice&           renderDevice,
			const std::string&      prefix,
			size_t                  viewIndex,
			const ViewRuntimeState& state,
			uint32_t&               outputId
		);

		/// @brief bloom downsample pass 群を追加します。
		void AddBloomDownsamplePasses(
			const RenderDevice&     renderDevice,
			const std::string&      prefix,
			const ViewRuntimeState& state,
			int                     mipCount,
			float                   bloomIntensity,
			float                   bloomThreshold,
			float                   bloomRadius,
			float                   bloomKnee,
			uint32_t                postFxInputId
		);

		/// @brief bloom upsample pass 群を追加します。
		void AddBloomUpsamplePasses(
			const RenderDevice&     renderDevice,
			const std::string&      prefix,
			const ViewRuntimeState& state,
			int                     mipCount,
			float                   bloomIntensity,
			float                   bloomRadius
		);

		/// @brief bloom 合成前の base copy pass を追加します。
		void AddBloomBaseCopyPass(
			RenderDevice&           renderDevice,
			const std::string&      prefix,
			const ViewRuntimeState& state,
			uint32_t                baseCopyInId,
			uint32_t                bloomCombinedOutId
		);

		/// @brief bloom mip を base copy へ合成する pass を追加します。
		void AddBloomCompositePass(
			const RenderDevice&     renderDevice,
			const std::string&      prefix,
			const ViewRuntimeState& state,
			uint32_t                bloomBaseId,
			uint32_t                bloomCombinedOutId,
			float                   bloomIntensity,
			float                   bloomRadius
		);

		/// @brief 汎用 post-fx pass を追加し ping-pong を進めます。
		void AddGenericPostFxPasses(
			const RenderDevice&      renderDevice,
			const std::string&       prefix,
			const ViewRuntimeState&  state,
			const PostFxRuntimePass& passRes,
			const Vec4&              scalar0,
			const Vec4&              scalar1,
			const Vec4&              color0,
			const Vec4&              color1,
			uint32_t&                postFxInputId,
			uint32_t&                postFxOutputId
		);

		/// @brief tone map pass を追加し最終 outputId を更新します。
		void AddToneMapExposurePass(
			const RenderDevice&     renderDevice,
			const std::string&      prefix,
			const ViewRuntimeState& state,
			const RenderViewInput&  view,
			uint32_t                postFxInputId,
			uint32_t&               outputId
		);

		/// @brief sprite-only view の clear pass を追加します。
		void AddSpriteOnlyClearPass(
			const std::string& prefix,
			uint32_t           outputId
		);

		/// @brief screen sprite pass を追加します。
		void AddScreenSpritePass(
			RenderDevice&                renderDevice,
			const std::string&           prefix,
			size_t                       viewIndex,
			const ViewRuntimeState&      state,
			uint32_t                     outputId,
			const std::vector<uint32_t>& screenSpriteTextureIds
		);

		/// @brief present 対象 view を back buffer に合成する pass を追加します。
		void AddPresentPass(RenderDevice& renderDevice);

		/// @brief shadow map depth texture の debug overlay pass を追加します。
		void AddShadowMapDebugPass(RenderDevice& renderDevice);

		/// @brief swap chain present がない editor frame の back buffer clear pass を追加します。
		void AddEditorBackBufferClearPass(
			const std::vector<RenderViewInput>& frameViews
		);

		/// @brief ImGui main draw data pass を追加します。
		/// @param uiSampledTextureIds 確定済み ImGui draw data が SRV として参照するテクスチャ。
		void AddImGuiMainPass(
			const std::vector<uint32_t>& uiSampledTextureIds
		);

		static constexpr uint32_t kMaxDebugLines = 65536; // TODO: とりあえず

		ConsoleSystem*       mConsole        = nullptr;
		RenderStartupOptions mStartupOptions = {};

		RenderGraph      mGraph;
		PipelineRegistry mPipelineRegistry;

		FullscreenPassRes mFullscreenPass = {};
		FullscreenPassRes mHdrCopyPass = {};
		FullscreenPassRes mToneMapPass = {};
		FullscreenPassRes mBloomDownsamplePass = {};
		FullscreenPassRes mBloomUpsamplePass = {};
		FullscreenPassRes mBloomCombinePass = {};
		FullscreenPassRes mDepthVisPass = {};
		ComputePassRes mComputePass = {};
		GeometryPassRes mGeometryPass = {};
		GeometryPassRes mShadowDepthPass = {};
		GeometryPassRes mShadowDepthFrontCullPass = {};
		GeometryPassRes mShadowDepthDoubleSidedPass = {};
		SpritePassRes mSpritePass = {};
		BillboardPassRes mBillboardPass = {};
		SkyboxPassRes mSkyboxPass = {};
		LinePassRes mLinePass = {};
		AssetID mGeometryShaderProgramId = kInvalidAssetID;
		Rhi::VertexLayoutDesc mGeometryVertexLayout = {};
		std::unordered_map<AssetID, MeshBuffer> mSceneMeshesByAsset;
		AssetID mDefaultMaterialInstance =
			kInvalidAssetID;
		AssetID mPostFxChainAsset = kInvalidAssetID;
		DirectionalShadowRuntimeState mDirectionalShadow = {};
		std::unordered_map<AssetID, MaterialBinding> mMaterialBindings;
		std::vector<PostFxRuntimePass> mPostFxPasses;
		TextureResourceCache mTextureResourceCache;
		MaterialTextureSet mDefaultMaterialTextures;
		uint32_t mSpriteFallbackTextureId = 0;
		uint64_t mLastTextureCacheStatsLogFrame = 0;

		std::unordered_map<std::string, ViewRuntimeState> mViewStates;
		std::vector<RenderViewInput> mFrameViews;
		std::vector<DebugLineInput> mFrameDebugLines;
		/// @brief 現フレームの ImGui pass 完了後に解放する旧ビュー出力です。
		std::vector<ViewRuntimeState> mDeferredViewTextureReleases;
		/// @brief 現フレームの ImGui draw data が参照する RenderGraph テクスチャです。
		std::vector<uint32_t> mFrameUiSampledTextureIds;
		std::string mPresentViewKey;
		UiMainRenderCallback mUiMainRenderCallback;
		UiPlatformRenderCallback mUiPlatformRenderCallback;

		uint32_t EnsureSpriteTextureLoaded(
			RenderDevice& renderDevice, AssetID textureAssetId
		);
		uint32_t ResolveSpriteTexture(
			RenderDevice& renderDevice, const SpriteTextureRef& textureRef
		);
		uint32_t EnsureSkyboxTextureLoaded(
			const RenderDevice& renderDevice, AssetID textureAssetId
		);
		void        EnsureSpriteFallbackTexture(RenderDevice& renderDevice);
		void        InitializeDebugLineResources(const Rhi::D3D12Device& dx);
		void        UploadDebugLinesForFrame();
		static void ReleaseViewRuntimeTextures(
			RenderDevice& renderDevice, ViewRuntimeState& state
		);
		/// @brief ImGui が前フレーム出力を参照できるよう、解放を pass 完了後まで保留します。
		void DeferViewRuntimeTextureRelease(ViewRuntimeState&& state);
		/// @brief ImGui pass 実行後に保留中のビューリソースをレジストリから解放します。
		void ReleaseDeferredViewRuntimeTextures(RenderDevice& renderDevice);
	};
}
