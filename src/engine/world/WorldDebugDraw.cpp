#include "WorldDebugDraw.h"
#include <core/math/Quaternion.h>

#include <utility>

namespace Unnamed {
	void WorldDebugDraw::DrawLine(
		const Vec3& start, const Vec3& end, const Vec4& color
	) {
#ifdef _DEBUG
		mPendingLines.emplace_back(
			PendingLine{
				.start = start,
				.end   = end,
				.color = color,
			}
		);
#else
		(void)start;
		(void)end;
		(void)color;
#endif
	}

	void WorldDebugDraw::DrawRay(
		const Vec3& position, const Vec3& dir, const float length,
		const Vec4& color
	) {
#ifdef _DEBUG
		mPendingLines.emplace_back(
			PendingLine{
				.start = position,
				.end   = position + dir * length,
				.color = color,
			}
		);
#else
		(void)position;
		(void)dir;
		(void)length;
		(void)color;
#endif
	}

	void WorldDebugDraw::DrawRay(
		const Vec3& position, const Vec3& dir, const Vec4& color
	) {
#ifdef _DEBUG
		mPendingLines.emplace_back(
			PendingLine{
				.start = position,
				.end   = position + dir,
				.color = color,
			}
		);
#else
		(void)position;
		(void)dir;
		(void)color;
#endif
	}

	void WorldDebugDraw::DrawAxis(
		const Vec3 position, const Vec3 right, const Vec3 up, const Vec3 forward
	) {
		DrawRay(position, right, Vec4::red);
		DrawRay(position, up, Vec4::green);
		DrawRay(position, forward, Vec4::blue);
	}

	void WorldDebugDraw::DrawCircle(
		const Vec3& position, const Quaternion& rotation, const float radius,
		const Vec4& color, const uint32_t       segments
	) {
		// 描画できない形状の場合
		if (radius <= 0.0f || segments <= 0) {
			return;
		}

		float angleStep = 360.0f / static_cast<float>(segments);

		// ラジアンに変換
		angleStep *= Math::deg2Rad;

		// とりあえず原点で計算する
		Vec3 lineStart = Vec3::zero;
		Vec3 lineEnd   = Vec3::zero;

		for (int i = 0; std::cmp_less(i, segments); ++i) {
			// 開始点
			lineStart.x = std::cos(angleStep * static_cast<float>(i));
			lineStart.y = std::sin(angleStep * static_cast<float>(i));
			lineStart.z = 0.0f;

			// 終了点
			lineEnd.x = std::cos(angleStep * static_cast<float>(i + 1));
			lineEnd.y = std::sin(angleStep * static_cast<float>(i + 1));
			lineEnd.z = 0.0f;

			// 半径を適用
			lineStart *= radius;
			lineEnd   *= radius;

			// 回転させる
			lineStart = rotation * lineStart;
			lineEnd   = rotation * lineEnd;

			lineStart += position;
			lineEnd   += position;

			DrawLine(lineStart, lineEnd, color);
		}
	}

	void WorldDebugDraw::DrawArc(
		const float startAngle, const float endAngle, const Vec3& position,
		const Quaternion& orientation, const float radius, const Vec4& color,
		const bool drawChord, const bool drawSector, const int segments
	) {
		float arcSpan = Math::DeltaAngle(startAngle, endAngle);

		if (arcSpan <= 0.0f) {
			arcSpan += 360.0f;
		}

		const float angleStep = arcSpan / static_cast<float>(segments) *
		                        Math::deg2Rad;

		const float stepOffset = startAngle + Math::deg2Rad;

		Vec3 lineStart = Vec3::zero;
		Vec3 lineEnd   = Vec3::zero;

		Vec3 arcStart = Vec3::zero;
		Vec3 arcEnd   = Vec3::zero;

		const Vec3 arcOrigin = position;

		for (int i = 0; i < segments; ++i) {
			const float stepStart =
				angleStep * static_cast<float>(i) + stepOffset;
			const float stepEnd =
				angleStep * static_cast<float>(i + 1) + stepOffset;

			lineStart.x = std::cos(stepStart);
			lineStart.y = std::sin(stepStart);
			lineStart.z = 0.0f;

			lineEnd.x = std::cos(stepEnd);
			lineEnd.y = std::sin(stepEnd);
			lineEnd.z = 0.0f;

			lineStart *= radius;
			lineEnd   *= radius;

			lineStart = orientation * lineStart;
			lineEnd   = orientation * lineEnd;

			lineStart += position;
			lineEnd   += position;

			if (i == 0) {
				arcStart = lineStart;
			}

			if (i == segments - 1) {
				arcEnd = lineEnd;
			}

			DrawLine(lineStart, lineEnd, color);
		}

		if (drawChord) {
			DrawLine(arcStart, arcEnd, color);
		}
		if (drawSector) {
			DrawLine(arcStart, arcOrigin, color);
			DrawLine(arcOrigin, arcEnd, color);
		}
	}

	void WorldDebugDraw::DrawArrow(
		const Vec3& position, const Vec3& direction, const Vec4& color,
		float       headSize
	) {
		// 矢印の終点
		const Vec3 end = position + direction;

		// 矢印の長さが頭のサイズより小さい場合は、頭のサイズを矢印の長さに合わせる
		headSize = std::min((position - end).Length(), headSize);

		// 矢印の方向を正規化
		const Vec3 dirNormalized = direction.Normalized();

		// 矢印の頭を描画するための垂直ベクトルを計算
		Vec3 up = Vec3::up;

		// dirNormalized と up が平行な場合、別のベクトルを使用する
		if (dirNormalized.IsParallel(up)) {
			up = Vec3::right;
		}

		const Vec3 right = dirNormalized.Cross(up).Normalized();

		const Vec3 arrowLeft = end - dirNormalized * headSize + right * headSize
		                       * 0.5f;
		const Vec3 arrowRight = end - dirNormalized * headSize - right *
		                        headSize * 0.5f;

		DrawLine(position, end, color);

		// 頭を描画
		DrawLine(arrowLeft, end, color);
		DrawLine(arrowRight, end, color);
	}

	void WorldDebugDraw::DrawQuad(
		const Vec3& pointA, const Vec3& pointB,
		const Vec3& pointC, const Vec3& pointD,
		const Vec4& color
	) {
		DrawLine(pointA, pointB, color);
		DrawLine(pointB, pointC, color);
		DrawLine(pointC, pointD, color);
		DrawLine(pointD, pointA, color);
	}

	void WorldDebugDraw::DrawRect(
		const Vec3& position, const Quaternion& orientation, const Vec2& extent,
		const Vec4& color
	) {
		// 四角形の中心から各頂点へのオフセットを計算
		const Vec3 rightOffset = Vec3::right * extent.x * 0.5f;
		const Vec3 upOffset    = Vec3::up * extent.y * 0.5f;

		// オフセットを回転させて、四角形の頂点の位置を計算
		const Vec3 offsetA = orientation * (rightOffset + upOffset);
		const Vec3 offsetB = orientation * (-rightOffset + upOffset);
		const Vec3 offsetC = orientation * (-rightOffset - upOffset);
		const Vec3 offsetD = orientation * (rightOffset - upOffset);

		DrawQuad(
			position + offsetA,
			position + offsetB,
			position + offsetC,
			position + offsetD,
			color
		);
	}

	void WorldDebugDraw::DrawRect(
		const Vec2&       point1, const Vec2&      point2, const Vec3& origin,
		const Quaternion& orientation, const Vec4& color
	) {
		const float extentX = abs(point1.x - point2.x);
		const float extentY = abs(point1.y - point2.y);

		const Vec3 rotatedRight = orientation * Vec3::right;
		const Vec3 rotatedUp    = orientation * Vec3::up;

		const Vec3 pointA =
			origin + rotatedRight * point1.x + rotatedUp * point1.y;
		const Vec3 pointB = pointA + rotatedRight * extentX;
		const Vec3 pointC = pointB + rotatedUp * extentY;
		const Vec3 pointD = pointA + rotatedUp * extentY;

		DrawQuad(pointA, pointB, pointC, pointD, color);
	}

	void WorldDebugDraw::DrawSphere(
		const Vec3&       position,
		const Quaternion& orientation,
		float             radius,
		const Vec4&       color,
		int               segments
	) {
		// 何が何でも描画
		if (radius <= 0.0f) {
			radius = 0.01f;
		}
		segments = std::max(segments, 2);

		const int doubleSegments = segments * 2;

		const float meridianStep = 180.0f / static_cast<float>(segments);

		for (int i = 0; i < segments; ++i) {
			DrawCircle(
				position,
				orientation *
				Quaternion::Euler(
					0, meridianStep * static_cast<float>(i) * Math::deg2Rad, 0
				),
				radius,
				color,
				doubleSegments
			);
		}

		Vec3        verticalOffset    = Vec3::zero;
		const float parallelAngleStep = Math::pi / static_cast<float>(segments);

		for (int i = 1; i < segments; ++i) {
			const float stepAngle = parallelAngleStep * static_cast<float>(i);
			verticalOffset = orientation * Vec3::up * cos(stepAngle) * radius;
			const float stepRadius = sin(stepAngle) * radius;

			DrawCircle(
				position + verticalOffset,
				orientation * Quaternion::Euler(90.0f * Math::deg2Rad, 0, 0),
				stepRadius,
				color,
				doubleSegments
			);
		}
	}

	void WorldDebugDraw::DrawBox(
		const Vec3& position, const Quaternion& orientation, Vec3 size,
		const Vec4& color
	) {
		const Vec3 offsetX = orientation * Vec3::right * size.x * 0.5f;
		const Vec3 offsetY = orientation * Vec3::up * size.y * 0.5f;
		const Vec3 offsetZ = orientation * Vec3::forward * size.z * 0.5f;

		const Vec3 pointA = -offsetX + offsetY;
		const Vec3 pointB = offsetX + offsetY;
		const Vec3 pointC = offsetX - offsetY;
		const Vec3 pointD = -offsetX - offsetY;

		DrawRect(position - offsetZ, orientation, {size.x, size.y}, color);
		DrawRect(position + offsetZ, orientation, {size.x, size.y}, color);

		DrawLine(
			position + (pointA - offsetZ), position + (pointA + offsetZ), color
		);
		DrawLine(
			position + (pointB - offsetZ), position + (pointB + offsetZ), color
		);
		DrawLine(
			position + (pointC - offsetZ), position + (pointC + offsetZ), color
		);
		DrawLine(
			position + (pointD - offsetZ), position + (pointD + offsetZ), color
		);
	}

	void WorldDebugDraw::Clear() {
		mPendingLines.clear();
	}

	void WorldDebugDraw::FlushToRenderFrameInputs(
		Render::RenderFrameInputs& inputs
	) {
#ifdef _DEBUG
		auto& lines = inputs.debugDraw.lines;
		lines.clear();
		lines.reserve(mPendingLines.size());
		for (const PendingLine& line : mPendingLines) {
			lines.emplace_back(
				Render::DebugLineInput{
					.start = line.start,
					.end   = line.end,
					.color = line.color,
				}
			);
		}
#endif
		(void)inputs;
		mPendingLines.clear();
	}
}
