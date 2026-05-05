#pragma once

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	/// @brief コース誘導HUDの画面投影結果です。
	struct CourseHudProjectionResult {
		Vec2  screenPositionPx  = Vec2::zero;
		Vec2  vectorFromCenter  = Vec2::zero;
		float alpha             = 1.0f;
		float arrowRotationRad  = 0.0f;
		bool  outOfScreen       = false;
	};

	/// @brief ワールド座標の誘導ターゲットを画面座標へ投影します。
	/// @param targetWorldPosition 投影対象のワールド座標
	/// @param cameraInput 現在カメラ情報
	/// @param viewportSizePx ビューポートサイズ（ピクセル）
	/// @param marginPx 画面端クランプ余白
	/// @param outResult 投影結果
	/// @return 投影可能な場合 true
	[[nodiscard]] bool BuildCourseHudProjection(
		const Vec3&                    targetWorldPosition,
		const Render::RenderCameraInput& cameraInput,
		const Vec2&                    viewportSizePx,
		float                          marginPx,
		CourseHudProjectionResult&     outResult
	);
}

