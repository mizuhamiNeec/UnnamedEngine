#pragma once
#include "Source/Engine/Input/Input.h"
#include "Source/Engine/Object3D/Object3D.h"
#include "Source/Engine/Sprite/Sprite.h"

//const float kPlayerRadius = 1.0f;

class Player : public Object3D {
public:
	~Player();

	void Initialize();

	void Update();

	void Draw() const;

	void Rotate();

	static Vec3 TransformNormal(Vec3 v, const Mat4& m);
	/// <summary>
	/// 攻撃
	/// </summary>
	void Attack();

	Vec3 GetWorldPosition();

	// 衝突を検出したら呼び出されるコールバック関数.
	//void OnCollision();

	void DrawUI();

private: // メンバ園数
	// モデル
	Model* model_ = nullptr;
	// テクスチャハンドル
	uint32_t textureHandle_ = 0u;

	// キーボード入力
	Input* input_ = nullptr;

	// 2Dレティクル用スプライト
	Sprite* sprite2DReticle_ = nullptr;
};
