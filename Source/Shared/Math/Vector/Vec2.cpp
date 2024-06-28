#include "Vec2.h"

#include <stdexcept>

/// <summary>
/// ベクトルの長さを返します
/// </summary>
/// <returns></returns>
float Vec2::Length() const {
	const float sqrLength = SqrLength();
	if (sqrLength > 0.0f) {
		return sqrtf(sqrLength);
	} else {
		return 0.0f;
	}
}

/// <summary>
/// ベクトルの長さの2乗を返します。
/// Length()より早いので可能な限りこっちを使ってください
/// </summary>
/// <returns></returns>
float Vec2::SqrLength() const {
	return x * x + y * y;
}

/// <summary>
/// otherとの直線距離を返します
/// </summary>
/// <param name="other">他の点</param>
/// <returns>2点間の距離</returns>
float Vec2::Distance(const Vec2& other) const {
	const float distX = other.x - x;
	const float distY = other.y - y;
	return sqrtf(distX * distX + distY * distY);
}

/// <summary>
/// otherとのDot積を返します
/// </summary>
/// <param name="other"></param>
/// <returns></returns>
float Vec2::DotProduct(const Vec2& other) const {
	return x * other.x + y * other.y;
}

/// <summary>
/// otherとのCross積を返します
/// </summary>
/// <param name="other"></param>
/// <returns></returns>
float Vec2::CrossProduct(const Vec2& other) const {
	return x * other.y - y * other.x;
}

/// <summary>
/// ベクトルがゼロに近い値の場合Trueを返します
/// </summary>
/// <param name="tolerance">許容範囲</param>
/// <returns></returns>
bool Vec2::IsZero(float tolerance) const {
	return (fabsf(x) < tolerance) && (fabsf(y) < tolerance);
}

/// <summary>
/// ノーマライズしたベクトルを返します
/// </summary>
/// <returns></returns>
Vec2 Vec2::Normalize() const {
	const float sqrLength = SqrLength();
	if (sqrLength > 0.0f) {
		const float invLength = 1.0f / sqrtf(sqrLength);
		return { x * invLength, y * invLength };
	} else {
		return { 0.0f, 0.0f };
	}
}

/// <summary>
/// ベクトルの長さを最大値と最小値でクランプします
/// </summary>
/// <param name="min">最小値</param>
/// <param name="max">最大値</param>
/// <returns>クランプされた値</returns>
Vec2 Vec2::ClampLength(float min, float max) const {
	const float sqrLength = SqrLength();
	if (sqrLength > max * max) {
		const float scale = max / sqrtf(sqrLength);
		return { x * scale, y * scale };
	} else if (sqrLength < min * min) {
		const float scale = min / sqrtf(sqrLength);
		return { x * scale, y * scale };
	}
	return { x, y };
}

/// <summary>
/// ベクトルを回転させた値を返します
/// </summary>
/// <param name="angle">rad</param>
/// <returns></returns>
Vec2 Vec2::RotateVector(float angle) const {
	const float cos = cosf(angle);
	const float sin = sinf(angle);
	return { x * cos - y * sin, x * sin + y * cos };
}

/// <summary>
/// startとend間を線形補間します
/// </summary>
/// <param name="end">終了位置</param>
/// <param name="t">時間</param>
/// <returns>線形補間された値</returns>
Vec2 Vec2::Lerp(const Vec2& end, float t) const {
	return { x + (end.x - x) * t, y + (end.y - y) * t };
}

float& Vec2::operator[](const int index) {
	if (index == 0) {
		return x;
	} else if (index == 1) {
		return y;
	} else {
		throw std::out_of_range("Vec2型の添字演算子に無効なインデックスが渡されました");
	}
}

const float& Vec2::operator[](const int index) const {
	if (index == 0) {
		return x;
	} else if (index == 1) {
		return y;
	} else {
		throw std::out_of_range("Vec2型の添字演算子に無効なインデックスが渡されました");
	}
}

Vec2 Vec2::operator+(const Vec2& other) const {
	return { x + other.x, y + other.y };
}

Vec2 Vec2::operator-(const Vec2& other) const {
	return { x - other.x, y - other.y };
}

Vec2 Vec2::operator*(float scalar) const {
	return { x * scalar, y * scalar };
}

Vec2 Vec2::operator/(float divisor) const {
	return { x / divisor, y / divisor };
}

Vec2 Vec2::operator+(const float& scalar) const {
	return { x + scalar, y + scalar };
}

Vec2 Vec2::operator-(const float& scalar) const {
	return { x + scalar, y + scalar };
}

Vec2 Vec2::operator*(const Vec2& other) const {
	return { x * other.x, y * other.y };
}

Vec2 Vec2::operator/(const Vec2& other) const {
	return { x / other.x, y / other.y };
}

Vec2& Vec2::operator+=(const Vec2& other) {
	x += other.x;
	y += other.y;
	return *this;
}

Vec2& Vec2::operator-=(const Vec2& other) {
	x -= other.x;
	y -= other.y;
	return *this;
}

Vec2& Vec2::operator*=(float scalar) {
	x *= scalar;
	y *= scalar;
	return *this;
}

Vec2& Vec2::operator/=(float divisor) {
	x /= divisor;
	y /= divisor;
	return *this;
}
