#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/math/Mat4.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

#include "engine/unnamed/primitive/Primitives.h"

namespace Unnamed::Render {
	enum class PROJECTION_DEPTH_MODE : uint8_t {
		FORWARD_Z,
		REVERSE_Z,
	};

	enum class SPRITE_TEXTURE_SOURCE : uint8_t {
		ASSET       = 0,
		VIEW_OUTPUT = 1,
	};

	struct SpriteTextureRef {
		SPRITE_TEXTURE_SOURCE source         = SPRITE_TEXTURE_SOURCE::ASSET;
		AssetID               textureAssetId = kInvalidAssetID;
		std::string           viewKey;
	};

	enum class SCENE_RENDER_MODE : uint8_t {
		FIT_VIEWPORT,
		FIXED_ASPECT_16X9,
		FIXED_ASPECT_4X3,
		HD_720P,
		FHD_1080P,
		UHD_4K,
	};

	struct SceneViewRenderMode {
		SCENE_RENDER_MODE mode = SCENE_RENDER_MODE::FIT_VIEWPORT;
		uint32_t          viewportPanelWidth = 0;
		uint32_t          viewportPanelHeight = 0;
		uint32_t          allocationHintWidth = 0;
		uint32_t          allocationHintHeight = 0;
		bool              preferRealtimeResize = true;
	};

	/// @brief SceneViewRenderModeからカメラ用アスペクト比を求めます。
	/// @param request シーンビューの表示要求
	/// @return カメラ投影に使用するアスペクト比
	float ResolveSceneViewAspectRatio(const SceneViewRenderMode& request);

	/// @brief SceneViewRenderModeから実際のシーン描画サイズを求めます。
	/// @param fallbackWidth viewportPanelWidth未指定時に使う幅
	/// @param fallbackHeight viewportPanelHeight未指定時に使う高さ
	/// @param request シーンビューの表示要求
	/// @return 2以上8192以下の偶数へ丸めた描画サイズ
	std::pair<uint32_t, uint32_t> ResolveSceneViewRenderExtent(
		uint32_t                   fallbackWidth,
		uint32_t                   fallbackHeight,
		const SceneViewRenderMode& request
	);

	struct PostFxPassOverride {
		std::string                            passName;
		bool                                   hasEnabledOverride = false;
		bool                                   enabled            = true;
		std::unordered_map<std::string, float> scalarParams;
		std::unordered_map<std::string, Vec4>  colorParams;
	};

	enum class RENDER_VIEW_TYPE : uint8_t {
		SCENE       = 0,
		SPRITE_ONLY = 1,
	};

	enum class RENDER_VIEW_SIZE_MODE : uint8_t {
		FIXED             = 0,
		MATCH_BACK_BUFFER = 1,
	};

	struct RenderViewOutputDesc {
		RENDER_VIEW_SIZE_MODE sizeMode = RENDER_VIEW_SIZE_MODE::FIXED;
		uint32_t              width = 0;
		uint32_t              height = 0;
		bool                  presentToSwapChain = false;
		bool                  clearSwapChainWhenNotPresenting = false;
		bool                  exposeToUi = false;
	};

	struct RenderCameraInput {
		Mat4                  view       = Mat4::identity;
		Mat4                  proj       = Mat4::identity;
		Mat4                  viewProj   = Mat4::identity;
		Vec3                  cameraPos  = Vec3::zero;
		float                 exposureEv = 0.0f;
		float                 nearZ      = 0.001f;
		float                 farZ       = 10000.0f;
		PROJECTION_DEPTH_MODE depthMode  = PROJECTION_DEPTH_MODE::REVERSE_Z;
		bool                  valid      = false;
	};

	struct SkyboxInput {
		bool    enabled        = false;
		AssetID textureAssetId = kInvalidAssetID;
		float   intensity      = 1.0f;
	};

	struct DirectionalLightInput {
		bool enabled     = false;
		bool castsShadow = true;

		Vec3  lightRayDirection = Vec3(0.0f, -1.0f, 0.0f);
		Vec3  directionToLight  = Vec3(0.0f, 1.0f, 0.0f);
		Vec3  color             = Vec3::one;
		float intensity         = 1.0f;
	};

	struct EnvironmentLightInput {
		Vec3  skyColor    = Vec3::zero;
		Vec3  groundColor = Vec3::zero;
		float intensity   = 0.0f;
		bool  enabled     = false;
	};

	struct VisibleRenderObject {
		AssetID              meshAssetId               = kInvalidAssetID;
		AssetID              materialInstanceId        = kInvalidAssetID;
		std::vector<AssetID> materialInstanceIdsBySlot = {};
		uint64_t             ownerEntityGuid           = 0;

		Mat4 world       = Mat4::identity;
		AABB worldBounds = {};

		bool     isSkinned         = false;
		uint32_t skeletonPaletteId = 0;
	};

	struct SkinningPaletteInput {
		AssetID           meshAssetId = kInvalidAssetID;
		std::vector<Mat4> boneMatrices;
	};

	struct ScreenSpriteInput {
		SpriteTextureRef texture;
		Vec2             positionPx  = Vec2::zero;
		Vec2             sizePx      = Vec2::one;
		Vec2             anchor      = Vec2(0.5f, 0.5f);
		float            rotationRad = 0.0f;
		Vec4             color       = Vec4::one;
		int32_t          sortKey     = 0;
		Vec2             uvMin       = Vec2(0.0f, 0.0f);
		Vec2             uvMax       = Vec2(1.0f, 1.0f);
		bool             uvFlipY     = false;
	};

	struct WorldBillboardInput {
		SpriteTextureRef texture;
		Vec3             worldPosition = Vec3::zero;
		Vec2             sizeWorld     = Vec2::one;
		Vec4             color         = Vec4::one;
		float            rotationRad   = 0.0f;
		int32_t          sortKey       = 0;
		bool             uvFlipY       = true;
		bool             depthTest     = true;
	};

	struct WorldSpriteInput {
		SpriteTextureRef texture;
		Vec3             worldPosition = Vec3::zero;
		Vec3             worldRight    = Vec3::right;
		Vec3             worldUp       = Vec3::up;
		Vec2             sizeWorld     = Vec2::one;
		Vec4             color         = Vec4::one;
		float            rotationRad   = 0.0f;
		int32_t          sortKey       = 0;
		bool             uvFlipY       = true;
	};

	struct DebugLineInput {
		Vec3 start = Vec3::zero;
		Vec3 end   = Vec3::right;
		Vec4 color = Vec4::one;
	};

	struct DebugDrawFrameInput {
		std::vector<DebugLineInput> lines;
	};

	struct RenderViewInput {
		std::string viewKey;

		RENDER_VIEW_TYPE                type          = RENDER_VIEW_TYPE::SCENE;
		RenderViewOutputDesc            output        = {};
		SceneViewRenderMode             sceneViewMode = {};
		std::vector<PostFxPassOverride> postFxPassOverrides;
		RenderCameraInput               camera           = {};
		SkyboxInput                     skybox           = {};
		DirectionalLightInput           directionalLight = {};
		EnvironmentLightInput           environmentLight = {};

		std::vector<VisibleRenderObject>  visibleObjects;
		std::vector<SkinningPaletteInput> skinningPalettes;
		std::vector<WorldBillboardInput>  worldBillboards;
		std::vector<WorldSpriteInput>     worldSprites;
		std::vector<ScreenSpriteInput>    screenSprites;
	};

	struct RenderFrameInputs {
		uint32_t                     frameIndex = 0;
		std::vector<RenderViewInput> views;
		DebugDrawFrameInput          debugDraw;

		float time = 0.0f;
	};
}
