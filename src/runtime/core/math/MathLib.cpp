#include <pch.h>

#include <runtime/core/math/Math.h>

#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>

namespace Math {
	/// @brief 線形補間を行います
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数 (0.0 ~ 1.0)
	Vec2 Lerp(const Vec2& a, const Vec2& b, float t) {
		return a * (1.0f - t) + b * t;
	}

	/// @brief ワールド座標をスクリーン座標と画面中心からの角度に変換します 
	/// @param worldPos 変換するワールド座標
	/// @param screenSize 画面サイズ
	/// @param bClamp 画面外の座標を画面端にクランプするか?
	/// @param margin 画面端からのマージン[px]
	/// @param outIsOffscreen 画面外にあるか?の結果
	/// @param outAngle 画面中心からの角度 [rad]
	/// @return スクリーン座標
	Vec2 WorldToScreen(
		const Vec3& worldPos, Vec2       screenSize,
		const bool& bClamp, const float& margin,
		bool&       outIsOffscreen,
		float&      outAngle
	) {
		CameraComponent* camera = CameraManager::GetActiveCamera().get();
		const Vec4 viewSpace = Vec4(worldPos, 1.0f) * camera->GetViewMat();
		const Vec4 clipSpace = viewSpace * camera->GetProjMat();

		const Vec2 screenCenter = screenSize * 0.5f;

		// w除算を行いNDC空間に変換
		const float invW = 1.0f / clipSpace.w;
		auto        ndc  = Vec3(
			clipSpace.x * invW,
			clipSpace.y * invW,
			clipSpace.z * invW
		);

		// スクリーン座標変換
		auto screenPos = Vec2(
			(ndc.x * 0.5f + 0.5f) * screenSize.x,
			(1.0f - (ndc.y * 0.5f + 0.5f)) * screenSize.y
		);

		// 画面中心からの方向ベクトル計算
		Vec2 direction = screenPos - screenCenter;

		// 角度計算
		outAngle = std::atan2(direction.x, -direction.y);
		if (viewSpace.z < 0.0f) {
			outAngle += pi;
		}
		outAngle = std::fmod(outAngle + 2.0f * pi, 2.0f * pi);

		// オフスクリーン判定
		if (!bClamp) {
			outIsOffscreen =
				viewSpace.z < 0.0f ||
				screenPos.x < 0.0f ||
				screenPos.x > screenSize.x ||
				screenPos.y < 0.0f ||
				screenPos.y > screenSize.y;
			return outIsOffscreen ? -Vec2::one * 0xFFFFFF : screenPos;
		}

		// クランプ処理
		outIsOffscreen = false;
		if (
			viewSpace.z < 0.0f ||
			screenPos.x < margin ||
			screenPos.x > screenSize.x - margin ||
			screenPos.y < margin ||
			screenPos.y > screenSize.y - margin
		) {
			outIsOffscreen      = true;
			Vec2 clampDirection = viewSpace.z < 0.0f ?
				                      Vec2(viewSpace.x, -viewSpace.y) :
				                      direction;

			const float length =
				std::hypot(clampDirection.x, clampDirection.y);
			if (length > 0.0f) {
				clampDirection /= length; // 正規化
			}

			const float screenRight  = screenSize.x - margin;
			const float screenBottom = screenSize.y - margin;

			// 境界との交差点を計算
			const std::vector tValues{
				(margin - screenCenter.x) / clampDirection.x,
				(screenRight - screenCenter.x) / clampDirection.x,
				(margin - screenCenter.y) / clampDirection.y,
				(screenBottom - screenCenter.y) / clampDirection.y
			};

			float minT = std::numeric_limits<float>::max();
			for (const float t : tValues) {
				if (t > 0.0f && t < minT) {
					minT = t;
				}
			}

			screenPos   = screenCenter + clampDirection * minT;
			screenPos.x = std::clamp(screenPos.x, margin, screenRight);
			screenPos.y = std::clamp(screenPos.y, margin, screenBottom);
		}

		return screenPos;
	}

	/// @brief ベクトルを平面に投影します
	/// @param vector 投影するベクトル
	/// @param normal 平面の法線ベクトル
	/// @return 投影後のベクトル
	Vec3 ProjectOnPlane(const Vec3& vector, const Vec3& normal) {
		return vector - normal * vector.Dot(normal);
	}

	/// @brief 地面の法線に基づいて移動方向を取得します
	/// @param forward 移動したい方向のベクトル
	/// @param groundNormal 地面の法線ベクトル
	/// @return 地面に沿った移動方向のベクトル
	Vec3 GetMoveDirection(const Vec3& forward, const Vec3& groundNormal) {
		Vec3 projectedForward = ProjectOnPlane(forward, groundNormal);
		return projectedForward.Normalized();
	}

	/// @brief 線形補間を行います
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @return 補間結果
	Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
		return a * (1.0f - t) + b * t;
	}

	/// @brief 各成分ごとの最小値を取得します
	/// @param a ベクトルA
	/// @param b ベクトルB
	/// @return 各成分ごとの最小値を持つベクトル
	Vec3 Min(const Vec3 a, const Vec3 b) {
		return Vec3(
			std::min(a.x, b.x),
			std::min(a.y, b.y),
			std::min(a.z, b.z)
		);
	}

	/// @brief 各成分ごとの最大値を取得します
	/// @param a ベクトルA
	/// @param b ベクトルB
	/// @return 各成分ごとの最大値を持つベクトル
	Vec3 Max(const Vec3 a, const Vec3 b) {
		return Vec3(
			std::max(a.x, b.x),
			std::max(a.y, b.y),
			std::max(a.z, b.z)
		);
	}

	/// @brief 線形補間を行います
	/// @param a 開始値
	/// @param b 終了値
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @return 補間結果
	Vec4 Lerp(const Vec4& a, const Vec4& b, float t) {
		return a * (1.0f - t) + b * t;
	}

	/// @brief 角度の差を計算します
	/// @param current 現在の角度 [rad]
	/// @param target 目標の角度 [rad]
	/// @return 角度の差 [rad]
	float DeltaAngle(const float current, const float target) {
		float delta = std::fmod(target - current, 2.0f * pi);
		if (delta > pi) delta -= 2.0f * pi;
		if (delta < -pi) delta += 2.0f * pi;
		return delta;
	}

	/// @brief 3次ベジェ曲線を計算します
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @param p1 制御点1
	/// @param p2 制御点2
	/// @return ベジェ曲線の値
	float CubicBezier(const float t, const Vec2 p1, const Vec2 p2) {
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;

		float         u       = t;
		constexpr int kMaxItr = 10;
		for (int i = 0; i < kMaxItr; i++) {
			const float oneMinusU = 1.0f - u;
			const float bezierX   = 3.0f * oneMinusU * oneMinusU * u * p1.x +
				3.0f * oneMinusU * u * u * p2.x +
				u * u * u;

			const float derivative = 3.0f * oneMinusU * oneMinusU * p1.x +
				6.0f * oneMinusU * u * (p2.x - p1.x) +
				3.0f * u * u * (1.0f - p2.x);

			if (std::fabs(derivative) < 1e-6f) {
				break;
			}

			const float diff = bezierX - t;
			u -= diff / derivative;
		}

		// u を用いて y(u) を計算
		const float oneMinusU = 1.0f - u;
		const float bezierY   = 3.0f * oneMinusU * oneMinusU * u * p1.y +
			3.0f * oneMinusU * u * u * p2.y +
			u * u * u;
		return bezierY;
	}

	/// @brief 3次ベジェ曲線を計算します
	/// @param t 補間係数 (0.0 ~ 1.0)
	/// @param p1 制御点1のx成分
	/// @param p2 制御点1のy成分
	/// @param p3 制御点2のx成分
	/// @param p4 制御点2のy成分
	/// @return ベジェ曲線の値
	float CubicBezier(
		const float t,
		const float p1, const float p2,
		const float p3, const float p4
	) {
		return CubicBezier(t, Vec2(p1, p2), Vec2(p3, p4));
	}

	/// @brief インチをメートルに変換します
	/// @param vec 変換するベクトル
	/// @return 変換後のベクトル
	Vec3 HtoM(const Vec3& vec) { return vec * 0.0254f; }
	/// @brief インチをメートルに変換します
	/// @param val 変換する値
	/// @return 変換後の値
	float HtoM(const float val) { return val * 0.0254f; }
	/// @brief メートルをインチに変換します
	/// @param vec 変換するベクトル
	/// @return 変換後のベクトル
	Vec3 MtoH(const Vec3& vec) { return vec * 39.3701f; }
	/// @brief メートルをインチに変換します
	/// @param val 変換する値
	/// @return 変換後の値
	float MtoH(const float val) { return val * 39.3701f; }
}
