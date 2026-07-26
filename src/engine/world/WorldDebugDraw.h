#pragma once

#include <vector>

#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	/// @brief WorldDebugDrawは、ワールド空間のデバッグ線分を寿命付きで蓄積して描画入力へ提出します
	class WorldDebugDraw {
	public:
		/// @brief ラインを描画します。
		/// @param start ラインの開始点
		/// @param end ラインの終了点
		/// @param color ラインの色（RGBA）
		void DrawLine(const Vec3& start, const Vec3& end, const Vec4& color);

		/// @brief レイを描画します。
		/// @param position レイの開始点
		/// @param dir レイの方向
		/// @param length レイの長さ
		/// @param color レイの色（RGBA）
		void DrawRay(
			const Vec3& position, const Vec3& dir, float length,
			const Vec4& color
		);

		/// @brief レイを描画します。dirに長さが含まれている時用
		/// @param position レイの開始点
		/// @param dir レイの方向と長さ
		/// @param color レイの色（RGBA）
		void DrawRay(
			const Vec3& position, const Vec3& dir, const Vec4& color
		);

		/// @brief 座標軸を描画します。
		/// @param position 座標軸の中心位置
		/// @param right
		/// @param up
		/// @param forward
		void DrawAxis(Vec3 position, Vec3 right, Vec3 up, Vec3 forward);

		/// @brief 円を描画します。
		/// @param position 円の中心位置
		/// @param rotation 円の回転
		/// @param radius 円の半径
		/// @param color 円の色（RGBA）
		/// @param segments 円を構成する線分の数（デフォルトは16）
		void DrawCircle(
			const Vec3&       position,
			const Quaternion& rotation,
			float             radius,
			const Vec4&       color,
			uint32_t          segments = 16
		);

		/// @brief 円弧を描画します。
		/// @param startAngle 円弧の開始角度（度数法）
		/// @param endAngle 円弧の終了角度（度数法）
		/// @param position 円弧の中心位置
		/// @param orientation 円弧の回転
		/// @param radius 円弧の半径
		/// @param color 円弧の色（RGBA）
		/// @param drawChord 円弧の弦を描画するかどうか
		/// @param drawSector 円弧の扇形部分を描画するかどうか
		/// @param segments 円弧を構成する線分の数（デフォルトは16）
		void DrawArc(
			float             startAngle, float  endAngle, const Vec3& position,
			const Quaternion& orientation, float radius, const Vec4&   color,
			bool              drawChord, bool    drawSector, int       segments
		);

		/// @brief 矢印を描画します。
		/// @param position 矢印の開始点
		/// @param direction 矢印の方向と長さ
		/// @param color 矢印の色（RGBA）
		/// @param headSize 矢印の頭のサイズ（デフォルトは0.25f）
		void DrawArrow(
			const Vec3& position, const Vec3& direction, const Vec4& color,
			float       headSize = 0.25f
		);

		/// @brief 四角形を描画します。
		/// @param pointA 四角形の頂点A
		/// @param pointB 四角形の頂点B
		/// @param pointC 四角形の頂点C
		/// @param pointD 四角形の頂点D
		/// @param color
		void DrawQuad(
			const Vec3& pointA, const Vec3& pointB,
			const Vec3& pointC, const Vec3& pointD, const Vec4& color
		);

		/// @brief 長方形を描画します。
		/// @param position 長方形の中心位置
		/// @param orientation 長方形の回転
		/// @param extent 長方形の幅と高さ
		/// @param color 長方形の色（RGBA）
		void DrawRect(
			const Vec3& position, const Quaternion& orientation,
			const Vec2& extent, const Vec4&         color
		);

		/// @brief 長方形を描画します。
		/// @param point1 長方形の一方の頂点の位置（ローカル座標）
		/// @param point2 長方形のもう一方の頂点の位置（ローカル座標）
		/// @param origin 長方形の中心位置（ワールド座標）
		/// @param orientation 長方形の回転
		/// @param color 長方形の色（RGBA）
		void DrawRect(
			const Vec2&       point1, const Vec2& point2, const Vec3& origin,
			const Quaternion& orientation,
			const Vec4&       color
		);

		/// @brief 球を描画します。
		/// @param position 球の中心位置
		/// @param orientation 球の回転
		/// @param radius 球の半径
		/// @param color 球の色（RGBA）
		/// @param segments 球を構成する線分の数（デフォルトは16）
		void DrawSphere(
			const Vec3&       position,
			const Quaternion& orientation,
			float             radius,
			const Vec4&       color,
			int               segments = 16
		);

		void DrawBox(
			const Vec3& position, const Quaternion& orientation,
			Vec3        size, const Vec4&           color
		);

		void Clear();

		/// @brief 描画コマンドをレンダーフレーム入力にフラッシュします。
		/// @param inputs フレーム入力構造体。描画コマンドが追加されます。
		void FlushToRenderFrameInputs(Render::RenderFrameInputs& inputs);

	private:
		/// @brief PendingLineは、debug線分の両端、色、残り表示時間を保持します
		struct PendingLine {
			Vec3 start = Vec3::zero;
			Vec3 end   = Vec3::right;
			Vec4 color = Vec4::one;
		};

		std::vector<PendingLine> mPendingLines;
	};
}
