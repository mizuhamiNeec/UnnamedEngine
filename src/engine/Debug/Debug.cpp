#include "engine/Debug/Debug.h"

#include "engine/Camera/CameraManager.h"
#include "engine/Engine.h"

/// @brief 線を描画
/// @param a 線の始点
/// @param b 線の終点
/// @param color 線の色
void Debug::DrawLine(const Vec3& a, const Vec3& b, const Vec4& color) {
	if (Unnamed::Engine::IsEditorMode()) {
		mLine->AddLine(a, b, color);
	}
}

/// @brief レイを描画
/// @param position レイの始点
/// @param dir レイの方向と長さ
/// @param color レイの色
void Debug::DrawRay(const Vec3& position, const Vec3& dir, const Vec4& color) {
	if (Unnamed::Engine::IsEditorMode()) {
		mLine->AddLine(position, position + dir, color);
	}
}

/// @brief 軸を描画
/// @param position 軸の位置
/// @param orientation 軸の向き
void Debug::DrawAxis(const Vec3& position, const Quaternion& orientation) {
	Mat4 viewMat   = CameraManager::GetActiveCamera()->GetViewMat().Inverse();
	Vec3 cameraPos = viewMat.GetTranslate();

	// カメラとの距離を計算
	float distance = (position - cameraPos).Length();

	// カメラとの距離が一定以下の場合は軸を描画しない
	if (distance < Math::HtoM(4.0f)) {
		return;
	}

	// スクリーン上で一定の長さに見えるように、
	// カメラからの距離に比例して実際の長さを調整
	float desiredScreenSize = 128.0f; // スクリーン上での目標サイズ（ピクセル）

	// 最大距離を設定（この距離以上では軸の長さが一定になる）
	constexpr float maxDistance = 32.0f; // 50メートルを超えたら一定の長さにする
	distance                    = std::min(distance, maxDistance);

	float length = distance * (desiredScreenSize / 1000.0f); // 1000.0fは調整用の係数

	const Vec3 right   = orientation * Vec3::right * length;
	const Vec3 up      = orientation * Vec3::up * length;
	const Vec3 forward = orientation * Vec3::forward * length;

	DrawRay(position, right, Vec4::red);
	DrawRay(position, up, Vec4::green);
	DrawRay(position, forward, Vec4::blue);
}

/// @brief 円を描画
/// @param position 円の中心位置
/// @param rotation 円の向き
/// @param radius 円の半径
/// @param color 円の色
/// @param segments 円の分割数
void Debug::DrawCircle(
	const Vec3&     position, const Quaternion& rotation, const float& radius,
	const Vec4&     color,
	const uint32_t& segments
) {
	// 描画できない形状の場合
	if (radius <= 0.0f || segments <= 0) {
		// 返す
		return;
	}

	float angleStep = (360.0f / static_cast<float>(segments));

	// ラジアンに変換
	angleStep *= Math::deg2Rad;

	// とりあえず原点で計算する
	Vec3 lineStart = Vec3::zero;
	Vec3 lineEnd   = Vec3::zero;

	for (int i = 0; i < static_cast<int>(segments); i++) {
		// 開始点
		lineStart.x = std::cos(angleStep * static_cast<float>(i));
		lineStart.y = std::sin(angleStep * static_cast<float>(i));
		lineStart.z = 0.0f;

		// 終了点
		lineEnd.x = std::cos(angleStep * static_cast<float>(i + 1));
		lineEnd.y = std::sin(angleStep * static_cast<float>(i + 1));
		lineEnd.z = 0.0f;

		// 目的の半径にする
		lineStart *= radius;
		lineEnd *= radius;

		// 回転させる
		lineStart = rotation * lineStart;
		lineEnd   = rotation * lineEnd;

		// 目的の座標に移動
		lineStart += position;
		lineEnd += position;

		// なんやかんやした線を描画
		DrawLine(lineStart, lineEnd, color);
	}
}

/// @brief 円弧を描画
/// @param startAngle 開始角度（度数法）
/// @param endAngle 終了角度（度数法）
/// @param position 円弧の中心位置
/// @param orientation 円弧の向き
/// @param radius 円弧の半径
/// @param color 円弧の色
/// @param drawChord 弦を描画するかどうか
/// @param drawSector 扇形の辺を描画するかどうか
/// @param arcSegments 円弧の分割数
void Debug::DrawArc(
	const float&      startAngle, const float& endAngle, const Vec3& position,
	const Quaternion& orientation, const float& radius, const Vec4& color,
	const bool&       drawChord, const bool& drawSector, const int& arcSegments
) {
	float arcSpan = Math::DeltaAngle(startAngle, endAngle);

	if (arcSpan <= 0) {
		arcSpan += 360.0f;
	}

	float angleStep = (arcSpan / static_cast<float>(arcSegments)) *
		Math::deg2Rad;
	float stepOffset = startAngle * Math::deg2Rad;

	Vec3 lineStart = Vec3::zero;
	Vec3 lineEnd   = Vec3::zero;

	Vec3 arcStart = Vec3::zero;
	Vec3 arcEnd   = Vec3::zero;

	Vec3 arcOrigin = position;

	for (int i = 0; i < arcSegments; i++) {
		const float stepStart = angleStep * static_cast<float>(i) + stepOffset;
		const float stepEnd   = angleStep * static_cast<float>(i + 1) +
			stepOffset;

		lineStart.x = std::cos(stepStart);
		lineStart.y = std::sin(stepStart);
		lineStart.z = 0.0f;

		lineEnd.x = std::cos(stepEnd);
		lineEnd.y = std::sin(stepEnd);
		lineEnd.z = 0.0f;

		lineStart *= radius;
		lineEnd *= radius;

		lineStart = orientation * lineStart;
		lineEnd   = orientation * lineEnd;

		lineStart += position;
		lineEnd += position;

		if (i == 0) {
			arcStart = lineStart;
		}

		if (i == arcSegments - 1) {
			arcEnd = lineEnd;
		}

		DrawLine(lineStart, lineEnd, color);
	}

	if (drawChord) {
		DrawLine(arcStart, arcEnd, color);
	}
	if (drawSector) {
		DrawLine(arcStart, arcOrigin, color);
		DrawLine(arcEnd, arcOrigin, color);
	}
}

/// @brief 矢印を描画
/// @param position 矢印の始点
/// @param direction 矢印の方向と長さ
/// @param color 矢印の色
/// @param headSize 矢印の頭の大きさ
void Debug::DrawArrow(
	const Vec3& position, const Vec3& direction,
	const Vec4& color,
	float       headSize
) {
	// 矢印の終点
	const Vec3 end = position + direction;

	// 頭のリサイズ
	headSize = std::min((position - end).Length(), headSize);

	// 矢印の方向を正規化
	const Vec3 dirNormalized = direction.Normalized();

	// 矢印の頭部を描画するための垂直なベクトルを計算
	Vec3 up = Vec3::up; // Y軸を基準に取る（特殊ケースで方向と一致する場合を後で処理）
	if (dirNormalized.IsParallel(up)) {
		up = Vec3::right; // 代わりにX軸を使用
	}
	const Vec3 right = dirNormalized.Cross(up).Normalized();

	// 頭部の羽根を描画
	const Vec3 arrowLeft = end - (dirNormalized * headSize) + (right * headSize
		* 0.5f);
	const Vec3 arrowRight = end - (dirNormalized * headSize) - (right * headSize
		* 0.5f);

	// 主体の線
	DrawLine(position, end, color);

	// 羽根の線
	DrawLine(end, arrowLeft, color);
	DrawLine(end, arrowRight, color);
}

/// @brief 四角形を描画
/// @param pointA 四角形の頂点A
/// @param pointB 四角形の頂点B
/// @param pointC 四角形の頂点C
/// @param pointD 四角形の頂点D
/// @param color 四角形の色
void Debug::DrawQuad(
	const Vec3& pointA, const Vec3& pointB, const Vec3& pointC,
	const Vec3& pointD, const Vec4& color
) {
	DrawLine(pointA, pointB, color);
	DrawLine(pointB, pointC, color);
	DrawLine(pointC, pointD, color);
	DrawLine(pointD, pointA, color);
}

/// @brief 四角形を描画
/// @param position 四角形の中心位置
/// @param orientation 四角形の向き
/// @param extent 四角形の幅と高さ
/// @param color 四角形の色
void Debug::DrawRect(const Vec3& position, const Quaternion& orientation,
                     const Vec2& extent, const Vec4&         color) {
	const Vec3 rightOffset = Vec3::right * extent.x * 0.5f;
	const Vec3 upOffset    = Vec3::up * extent.y * 0.5f;

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

/// @brief 四角形を描画
/// @param point1 四角形の一方の頂点（ローカル座標系）
/// @param point2 四角形のもう一方の頂点（ローカル座標系）
/// @param origin 四角形の原点位置（ワールド座標系）
/// @param orientation 四角形の向き
/// @param color 四角形の色
void Debug::DrawRect(
	const Vec2&       point1, const Vec2& point2, const Vec3& origin,
	const Quaternion& orientation,
	const Vec4&       color
) {
	const float extentX = abs(point1.x - point2.x);
	const float extentY = abs(point1.y - point2.y);

	const Vec3 rotatedRight = orientation * Vec3::right;
	const Vec3 rotatedUp    = orientation * Vec3::up;

	const Vec3 pointA = origin + rotatedRight * point1.x + rotatedUp * point1.y;
	const Vec3 pointB = pointA + rotatedRight * extentX;
	const Vec3 pointC = pointB + rotatedUp * extentY;
	const Vec3 pointD = pointA + rotatedUp * extentY;

	DrawQuad(pointA, pointB, pointC, pointD, color);
}

/// @brief 球を描画
/// @param position 球の中心位置
/// @param orientation 球の向き
/// @param radius 球の半径
/// @param color 球の色
/// @param segments 球の分割数
void Debug::DrawSphere(
	const Vec3& position, const Quaternion& orientation, float radius,
	const Vec4& color, int                  segments
) {
	if (radius <= 0) {
		radius = 0.01f;
	}
	segments = std::max(segments, 2);

	const int doubleSegments = segments * 2;

	const float meridianStep = 180.0f / static_cast<float>(segments);

	for (int i = 0; i < segments; i++) {
		DrawCircle(
			position,
			orientation * Quaternion::Euler(
				0, meridianStep * static_cast<float>(i) * Math::deg2Rad, 0),
			radius,
			color,
			doubleSegments
		);
	}

	Vec3  verticalOffset    = Vec3::zero;
	float parallelAngleStep = Math::pi / static_cast<float>(segments);
	float stepRadius        = 0.0f;

	for (int i = 1; i < segments; i++) {
		float stepAngle = parallelAngleStep * static_cast<float>(i);
		verticalOffset  = (orientation * Vec3::up) * cos(stepAngle) * radius;
		stepRadius      = sin(stepAngle) * radius;

		DrawCircle(
			position + verticalOffset,
			orientation * Quaternion::Euler(90.0f * Math::deg2Rad, 0, 0),
			stepRadius,
			color,
			doubleSegments
		);
	}
}

/// @brief 箱を描画
/// @param position 箱の中心位置
/// @param orientation 箱の向き
/// @param size 箱のサイズ
/// @param color 箱の色
void Debug::DrawBox(
	const Vec3& position, const Quaternion& orientation,
	Vec3        size, const Vec4&           color
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

	DrawLine(position + (pointA - offsetZ), position + (pointA + offsetZ),
	         color);
	DrawLine(position + (pointB - offsetZ), position + (pointB + offsetZ),
	         color);
	DrawLine(position + (pointC - offsetZ), position + (pointC + offsetZ),
	         color);
	DrawLine(position + (pointD - offsetZ), position + (pointD + offsetZ),
	         color);
}

/// @brief 円柱を描画
/// @param position 円柱の中心位置
/// @param orientation 円柱の向き
/// @param height 円柱の高さ
/// @param radius 円柱の半径
/// @param color 円柱の色
/// @param drawFromBase 底面から描画するかどうか
void Debug::DrawCylinder(
	const Vec3&  position, const Quaternion& orientation, const float& height,
	const float& radius, const Vec4&         color,
	const bool&  drawFromBase
) {
	const Vec3 localUp      = orientation * Vec3::up;
	const Vec3 localRight   = orientation * Vec3::right;
	const Vec3 localForward = orientation * Vec3::forward;

	const Vec3 basePositionOffset = drawFromBase ?
		                                Vec3::zero :
		                                (localUp * height * 0.5f);
	const Vec3 basePosition = position - basePositionOffset;
	const Vec3 topPosition  = basePosition + localUp * height;

	const Quaternion circleOrientation = orientation * Quaternion::Euler(
		90.0f * Math::deg2Rad, 0, 0);

	const Vec3 pointA = basePosition + localRight * radius;
	const Vec3 pointB = basePosition + localForward * radius;
	const Vec3 pointC = basePosition - localRight * radius;
	const Vec3 pointD = basePosition - localForward * radius;

	DrawRay(pointA, localUp * height, color);
	DrawRay(pointB, localUp * height, color);
	DrawRay(pointC, localUp * height, color);
	DrawRay(pointD, localUp * height, color);

	DrawCircle(basePosition, circleOrientation, radius, color, 32);
	DrawCircle(topPosition, circleOrientation, radius, color, 32);
}

/// @brief カプセルを描画
/// @param position カプセルの中心位置
/// @param orientation カプセルの向き
/// @param height カプセルの高さ
/// @param radius カプセルの半径
/// @param color カプセルの色
/// @param drawFromBase 底面から描画するかどうか
void Debug::DrawCapsule(
	const Vec3&  position, const Quaternion& orientation, const float& height,
	const float& radius, const Vec4&         color,
	const bool&  drawFromBase
) {
	const float      rad            = std::clamp(radius, 0.0f, height * 0.5f);
	const Vec3       localUp        = orientation * Vec3::up;
	const Quaternion arcOrientation = orientation * Quaternion::Euler(
		0, 90.0f * Math::deg2Rad, 0);

	const Vec3 basePositionOffset = drawFromBase ?
		                                Vec3::zero :
		                                (localUp * height * 0.5f);
	const Vec3 baseArcPosition = position + localUp * rad - basePositionOffset;
	DrawArc(180, 360, baseArcPosition, orientation, rad, color);
	DrawArc(180, 360, baseArcPosition, arcOrientation, rad, color);

	const float cylinderHeight = height - rad * 2.0f;
	DrawCylinder(baseArcPosition, orientation, cylinderHeight, rad, color,
	             true);

	const Vec3 topArcPosition = baseArcPosition + localUp * cylinderHeight;

	DrawArc(0, 180, topArcPosition, orientation, rad, color);
	DrawArc(0, 180, topArcPosition, arcOrientation, rad, color);
}

/// @brief カプセルを描画
/// @param start カプセルの始点
/// @param end カプセルの終点
/// @param radius カプセルの半径
/// @param color カプセルの色
void Debug::DrawCapsule(const Vec3& start, const Vec3& end, const float& radius,
                        const Vec4& color) {
	// 始点から終点へのベクトルと長さを計算
	const Vec3  direction = (end - start).Normalized();
	const float length    = (end - start).Length();

	// 半球の向きを計算
	Quaternion orientation = Quaternion::LookRotation(
		direction, Vec3::up
	).Normalized();
	orientation = orientation * Quaternion::Euler(90.0f * Math::deg2Rad, 0, 0);

	// 始点での半球を描画
	DrawArc(180, 360, start, orientation, radius, color);
	DrawArc(180, 360, start,
	        orientation * Quaternion::Euler(0, 90.0f * Math::deg2Rad, 0),
	        radius, color);

	// 終点での半球を描画
	DrawArc(0, 180, end, orientation, radius, color);
	DrawArc(0, 180, end,
	        orientation * Quaternion::Euler(0, 90.0f * Math::deg2Rad, 0),
	        radius, color);

	// 中間の円柱部分を描画（両端の半球をつなぐ）
	DrawCylinder(start, orientation, length, radius, color, true);
}

/// @brief 三角形を描画
/// @param triangle 描画する三角形
/// @param color 三角形の色
void Debug::DrawTriangle(const Unnamed::Triangle& triangle, const Vec4 color) {
	DrawLine(triangle.v0, triangle.v1, color);
	DrawLine(triangle.v1, triangle.v2, color);
	DrawLine(triangle.v2, triangle.v0, color);
}

/// @brief デバッグ描画の初期化
/// @param lineCommon 線描画共通設定
void Debug::Init(LineCommon* lineCommon) {
	mLine = std::make_unique<Line>(lineCommon);
}

/// @brief 毎フレーム更新
void Debug::Update() {
}

/// @brief 描画
void Debug::Draw() {
	mLine->Draw();
}

/// @brief デバッグ描画の終了処理
void Debug::Shutdown() {
	mLine.reset();
}

std::unique_ptr<Line> Debug::mLine;
