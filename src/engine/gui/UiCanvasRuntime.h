#pragma once
#include <cstdint>

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed {
	class AssetManager;
	class Scene;
	struct Ray;

	namespace Render {
		struct RenderCameraInput;
		struct ScreenSpriteInput;
	}

	/// @brief UIキャンバスのランタイム用ユーティリティ群です。
	namespace UiCanvasRuntime {
		/// @brief UI描画コマンドの矩形からスクリーンスプライト入力を生成します。
		/// @param rect UI描画コマンドの矩形
		/// @param sortKey 描画順キー
		/// @return スクリーンスプライト入力
		[[nodiscard]] Render::ScreenSpriteInput BuildScreenSprite(
			const Gui::UiDrawCommandRect& rect, int32_t sortKey
		);

		[[nodiscard]] Render::ScreenSpriteInput BuildScreenSprite(
			const Gui::UiDrawCommandImage& image, int32_t sortKey
		);

		/// @brief マウス位置からワールド空間レイを構築します。
		/// @param camera 使用するカメラ入力
		/// @param mousePos マウス位置
		/// @param viewportSize ビューポートサイズ
		/// @param outRay 出力レイ
		/// @return 構築できた場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool BuildRayFromMouse(
			const Render::RenderCameraInput& camera,
			const Vec2&                      mousePos,
			const Vec2&                      viewportSize,
			Ray&                             outRay
		);

		/// @brief レイとUI平面の交差を判定します。
		/// @param ray 判定するレイ
		/// @param center 平面の中心座標
		/// @param axisRight 平面の右方向
		/// @param axisUp 平面の上方向
		/// @param worldSize 平面のワールドサイズ
		/// @param outDistance ヒット距離の出力先
		/// @param outLocalWorld ヒット位置のローカルワールド座標の出力先
		/// @return 交差した場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool RayIntersectsUiPlane(
			const Ray&  ray,
			const Vec3& center,
			const Vec3& axisRight,
			const Vec3& axisUp,
			const Vec2& worldSize,
			float&      outDistance,
			Vec2&       outLocalWorld
		);

		/// @brief UI平面上のワールド座標をピクセル座標に変換します。
		/// @param localWorld 平面上のローカルワールド座標
		/// @param worldSize UIのワールドサイズ
		/// @param pixelSize UIのピクセルサイズ
		/// @return ピクセル座標
		[[nodiscard]] Vec2 WorldToUiPixels(
			const Vec2& localWorld,
			const Vec2& worldSize,
			const Vec2& pixelSize
		);

		/// @brief UI平面の手前にシーンジオメトリがあるかを判定します。
		/// @param scene 判定対象のシーン
		/// @param assetManager アセットマネージャー
		/// @param ray 判定するレイ
		/// @param maxDistance UI平面までの距離
		/// @param ignoredEntityGuid 除外するエンティティのGUID
		/// @return UI平面より手前にジオメトリがある場合はtrue
		[[nodiscard]] bool RayHitsSceneGeometryBefore(
			const Scene&  scene,
			AssetManager& assetManager,
			const Ray&    ray,
			float         maxDistance,
			uint64_t      ignoredEntityGuid
		);
	}
}
