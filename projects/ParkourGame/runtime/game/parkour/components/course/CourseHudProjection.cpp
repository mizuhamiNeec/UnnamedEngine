#include "CourseHudProjection.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace Unnamed {
	namespace {
		[[nodiscard]] float Clamp01(const float value) {
			return std::clamp(value, 0.0f, 1.0f);
		}

		constexpr float kMinGuideAlpha = 0.25f;
	}

	bool BuildCourseHudProjection(
		const Vec3&                      targetWorldPosition,
		const Render::RenderCameraInput& cameraInput,
		const Vec2&                      viewportSizePx,
		const float                      marginPx,
		CourseHudProjectionResult&       outResult
	) {
		outResult = {};
		if (!cameraInput.valid ||
		    viewportSizePx.x <= 1.0f ||
		    viewportSizePx.y <= 1.0f) {
			return false;
		}

		const Vec4 clip = Vec4(targetWorldPosition, 1.0f) * cameraInput.viewProj;
		if (std::abs(clip.w) <= 1.0e-6f) {
			return false;
		}

		Vec3 ndc = Vec3(clip.x, clip.y, clip.z) / clip.w;
		Vec2 screenPositionPx = Vec2(
			(ndc.x * 0.5f + 0.5f) * viewportSizePx.x,
			(1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSizePx.y
		);

		const Vec2 center = viewportSizePx * 0.5f;
		Vec2       toCenter = screenPositionPx - center;
		if (clip.w < 0.0f) {
			// カメラ背面は方向を反転させ、画面端へ誘導します。
			toCenter         = -toCenter;
			screenPositionPx = center + toCenter;
		}

		const float safeMarginX = std::min(
			marginPx,
			std::max(0.0f, viewportSizePx.x * 0.5f - 1.0f)
		);
		const float safeMarginY = std::min(
			marginPx,
			std::max(0.0f, viewportSizePx.y * 0.5f - 1.0f)
		);

		const float minX = safeMarginX;
		const float minY = safeMarginY;
		const float maxX = viewportSizePx.x - safeMarginX;
		const float maxY = viewportSizePx.y - safeMarginY;
		const float clampMinX = std::min(minX, maxX);
		const float clampMaxX = std::max(minX, maxX);
		const float clampMinY = std::min(minY, maxY);
		const float clampMaxY = std::max(minY, maxY);

		const bool outOfScreen = clip.w < 0.0f ||
		                         screenPositionPx.x < minX ||
		                         screenPositionPx.x > maxX ||
		                         screenPositionPx.y < minY ||
		                         screenPositionPx.y > maxY;
		if (outOfScreen) {
			Vec2 dir = toCenter;
			const float len = std::hypot(dir.x, dir.y);
			if (len > 1.0e-6f) {
				dir /= len;
			} else {
				dir = Vec2(1.0f, 0.0f);
			}

			const float tx = std::abs(dir.x) > 1.0e-6f ?
				                 (std::max(maxX - center.x, center.x - minX) /
				                  std::abs(dir.x)) :
				                 FLT_MAX;
			const float ty = std::abs(dir.y) > 1.0e-6f ?
				                 (std::max(maxY - center.y, center.y - minY) /
				                  std::abs(dir.y)) :
				                 FLT_MAX;
			const float t = std::min(tx, ty);
			screenPositionPx = center + dir * t;
			screenPositionPx.x = std::clamp(screenPositionPx.x, clampMinX, clampMaxX);
			screenPositionPx.y = std::clamp(screenPositionPx.y, clampMinY, clampMaxY);
			toCenter = screenPositionPx - center;
		}

		const float distanceFromCenter = std::hypot(toCenter.x, toCenter.y);
		const float maxDistance = std::max(1.0f, std::hypot(center.x, center.y));
		const float distanceT = Clamp01(distanceFromCenter / maxDistance);

		outResult.screenPositionPx = screenPositionPx;
		outResult.vectorFromCenter = toCenter;
		// 中心付近でも完全には消えないよう、最小アルファを確保します。
		outResult.alpha            = std::lerp(kMinGuideAlpha, 1.0f, distanceT);
		outResult.arrowRotationRad = std::atan2(toCenter.x, -toCenter.y);
		outResult.outOfScreen      = outOfScreen;
		return true;
	}
}

