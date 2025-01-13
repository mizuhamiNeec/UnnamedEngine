#include "MathLib.h"
#define NOMINMAX
#include <algorithm>
#include <cassert>
#include <cmath>
#include <Engine.h>
#include <Camera/CameraManager.h>
#include <Components/CameraComponent.h>
#include <Lib/Console/Console.h>
#include <Lib/Structs/Structs.h>
#include <Window/Window.h>

bool Math::IsCollision(const AABB& aabb, const Vec3& point) {
	return (
		point.x >= aabb.min.x && point.x <= aabb.max.x &&
		point.y >= aabb.min.y && point.y <= aabb.max.y &&
		point.z >= aabb.min.z && point.z <= aabb.max.z
		);
}

Vec3 Math::Lerp(const Vec3& a, const Vec3& b, const float t) {
	return a * (1 - t) + b * t;
}

Vec3 Math::CatmullRomPosition(const std::vector<Vec3>& points, const float t) {
	assert(points.size() >= 4 && "制御点は4点以上必要です");

	// 区間数は制御点の数-1
	size_t division = points.size() - 1;
	// 1区間の長さ(全体を1.0とした割合)
	float areaWidth = 1.0f / static_cast<float>(division);

	// 区間内の始点を0.0f,終点を1.0fとしたときの現在位置
	float t_2 = std::fmod(t, areaWidth) * static_cast<float>(division);
	// 下限(0.0f)と上限(1.0f)の範囲に収める
	t_2 = std::clamp(t_2, 0.0f, 1.0f);

	// 区間番号
	size_t index = static_cast<int>(t / areaWidth);
	// 区間番号が上限を超えないように収める
	if (index >= division) {
		index = division - 1;
	}

	// 4点分のインデックス
	size_t index0 = index;
	if (index > 0) {
		index0 = index - 1;
	}
	size_t index1 = index;
	size_t index2 = index + 1;
	size_t index3 = index2;
	if (index3 + 1 < points.size()) {
		index3 = index2 + 1;
	}

	// 4点の座標
	const Vec3& p0 = points[index0];
	const Vec3& p1 = points[index1];
	const Vec3& p2 = points[index2];
	const Vec3& p3 = points[index3];

	// 4点を指定してCatmull-Rom補完
	return CatmullRomInterpolation(p0, p1, p2, p3, t_2);
}

Vec3 Math::CatmullRomInterpolation(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const float t) {
	const float t2 = t * t;
	const float t3 = t2 * t;

	// Catmull-Rom スプラインの補間式
	return 0.5f * (
		(2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
		);
}

float Math::DeltaAngle(const float& current, const float& target) {
	float delta = fmod(target - current, 360.0f);
	if (delta > 180.0f) {
		delta -= 360.0f;
	} else if (delta < -180.0f) {
		delta += 360.0f;
	}
	return delta;
}

//-----------------------------------------------------------------------------
// Purpose: ワールド座標をスクリーン座標と画面中心からの角度に変換します
// Params : worldPos    - 変換するワールド座標
//          bClamp      - 画面外の座標を画面端にクランプするか?
//          margin      - 画面端からのマージン [px]
//          isOffscreen - 画面外にあるかどうかの結果
//          outAngle    - 画面中心からの角度 [rad]
//-----------------------------------------------------------------------------
Vec2 Math::WorldToScreen(const Vec3& worldPos, const bool& bClamp, const float& margin, bool& isOffscreen, float& outAngle) {
	// 初期設定
	CameraComponent* camera = CameraManager::GetActiveCamera().get();
	const Vec4 viewSpace = Vec4(worldPos, 1.0f) * camera->GetViewMat();
	const Vec4 clipSpace = viewSpace * camera->GetProjMat();

	const float screenWidth = static_cast<float>(Window::GetClientWidth());
	const float screenHeight = static_cast<float>(Window::GetClientHeight());
	const Vec2 screenCenter(screenWidth * 0.5f, screenHeight * 0.5f);

	// w除算を行いNDC空間に変換
	const float invW = 1.0f / clipSpace.w;
	Vec3 ndc(clipSpace.x * invW, clipSpace.y * invW, clipSpace.z * invW);

	// スクリーン座標計算
	Vec2 screenPos((ndc.x * 0.5f + 0.5f) * screenWidth,
		(1.0f - (ndc.y * 0.5f + 0.5f)) * screenHeight);

// 画面中心からの方向ベクトル計算
	Vec2 direction = screenPos - screenCenter;

	// 角度計算
	outAngle = std::atan2(direction.x, -direction.y);
	if (viewSpace.z < 0.0f) outAngle += pi;
	outAngle = std::fmod(outAngle + 2.0f * pi, 2.0f * pi);

	// オフスクリーン判定（bClamp無効時）
	if (!bClamp) {
		isOffscreen = viewSpace.z < 0.0f ||
			screenPos.x < 0 || screenPos.x > screenWidth ||
			screenPos.y < 0 || screenPos.y > screenHeight;
		return isOffscreen ? -Vec2::one * 0xFFFFFF : screenPos;
	}

	// クランプ処理
	isOffscreen = false;
	Vec2 clampDirection;
	if (viewSpace.z < 0.0f || screenPos.x < margin || screenPos.x > screenWidth - margin ||
		screenPos.y < margin || screenPos.y > screenHeight - margin) {
		isOffscreen = true;
		clampDirection = viewSpace.z < 0.0f ? Vec2(viewSpace.x, -viewSpace.y) : direction;

		const float length = std::hypot(clampDirection.x, clampDirection.y);
		if (length > 0.0f) {
			clampDirection /= length; // 正規化
		}

		const float screenRight = screenWidth - margin;
		const float screenBottom = screenHeight - margin;

		// 境界との交差点を計算
		std::vector<float> tValues{
			(margin - screenCenter.x) / clampDirection.x,
			(screenRight - screenCenter.x) / clampDirection.x,
			(margin - screenCenter.y) / clampDirection.y,
			(screenBottom - screenCenter.y) / clampDirection.y
		};

		float minT = std::numeric_limits<float>::max(); // 修正
		for (float t : tValues) {
			if (t > 0.0f && t < minT) minT = t;
		}

		screenPos = screenCenter + clampDirection * minT;
		screenPos.x = std::clamp(screenPos.x, margin, screenRight);
		screenPos.y = std::clamp(screenPos.y, margin, screenBottom);
	}

	return screenPos;
}

Vec3 Math::HtoM(const Vec3& vec) {
	// Hammer -> Meter
	return vec * 0.0254f;
}

float Math::HtoM(const float val) {
	// Hammer -> Meter
	return val * 0.0254f;

}

Vec3 Math::MtoH(const Vec3& vec) {
	// Meter -> Hammer
	return vec * 39.3701f;
}

float Math::MtoH(const float val) {
	// Meter -> Hammer
	return val * 39.3701f;
}
