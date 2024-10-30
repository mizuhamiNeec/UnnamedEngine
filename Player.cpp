#include "Player.h"

#include <algorithm>
#include <cassert>
#include <format>

#include "TextureManager.h"
#include "Console/Console.h"
#include "imgui/imgui.h"

Player::~Player() {
	//delete sprite2DReticle_;
}

void Player::Initialize() {
	// ヌルポチェック
	//assert(model);

	//model_ = model;
	//textureHandle_ = textureHandle;

	//uint32_t textureReticle = TextureManager::Load("./Resources/reticle.png");

	// スプライト生成
	//sprite2DReticle_ = Sprite::Create(textureReticle, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.5f, 0.5f });

	// シングルトンインスタンスを取得する
	input_ = Input::GetInstance();
}

void Player::Update() {

	//// デスフラグの立った弾を削除
	//bullets_.remove_if([](const PlayerBullet* bullet) {
	//	if (bullet->IsDead()) {
	//		delete bullet;
	//		return true;
	//	}
	//	return false;
	//	});

	// キャラクター旋回処理
	Rotate();

	//// 自機のワールド座標から3Dレティクルのワールド座標を計算
	//{
	//	// 自機から3Dレティクルへの距離
	//	const float kDistancePlayerTo3DReticle = 50.0f;
	//	// 自機から3Dレティクルへのオフセット(Z+向き)
	//	Vec3 offset = { 0.0f, 0.0f, 1.0f };
	//	// 自機のワールド行列の回転を反映
	//	offset = TransformNormal(offset, worldTransform_.matWorld_);

	//	offset = offset.Normalize() * kDistancePlayerTo3DReticle;
	//	// 3Dレティクルの座標を設定
	//	worldTransform3DReticle_.translation_ = GetWorldPosition() + offset;

	//	worldTransform3DReticle_.UpdateMatrix();
	//	worldTransform3DReticle_.TransferMatrix();
	//}

	// キャラクター攻撃処理
	Attack();

	//// 弾更新
	//for (PlayerBullet* bullet : bullets_) {
	//	bullet->Update();
	//}

	// キャラクターの座標を画面表示する処理
	ImGui::Begin("Menu");
	ImGui::SetNextItemOpen(true);
	if (ImGui::TreeNode("Player")) {
		ImGui::DragFloat3("pos", &transform_.translate.x, 0.01f);
		ImGui::DragFloat3("rot", &transform_.rotate.x, 0.01f);
		ImGui::DragFloat3("scale", &transform_.scale.x, 0.01f);
		ImGui::TreePop();
	}

	ImGui::End();

	// 行列を定数バッファに転送
	Object3D::Update();
}

void Player::Draw() const {
	//// 3Dモデルを描画
	//model_->Draw();// 3Dモデルを描画
	//model_->Draw();

	//// 弾描画
	//for (PlayerBullet* bullet : bullets_) {
	//	bullet->Draw(viewProjection);
	//}

	//// 3Dレティクルのワールド座標から2Dレティクルのスクリーン座標を計算
	//{
	//	Vec3 positionReticle = { worldTransform3DReticle_.matWorld_.m[3][0], worldTransform3DReticle_.matWorld_.m[3][1], worldTransform3DReticle_.matWorld_.m[3][2] };
	//	// ビューポート行列
	//	Matrix4x4 matViewport = Matrix4x4::ViewportMat(0.0f, 0.0f, WinApp::kWindowWidth, WinApp::kWindowHeight, 0.0f, 1.0f);

	//	// ビュー行列とプロジェクション行列、ビューポート行列を合成する
	//	Matrix4x4 matViewProjectionViewport = viewProjection.matView * viewProjection.matProjection * matViewport;

	//	// ワールド→スクリーン座標変換(ここで3Dから2Dになる)
	//	positionReticle = Matrix4x4::Transform(positionReticle, matViewProjectionViewport);

	//	// スプライトのレティクルに座標設定
	//	sprite2DReticle_->SetPosition(Vector2(positionReticle.x, positionReticle.y));
	//}

	//// 3Dレティクルを描画
	// model_->Draw(worldTransform3DReticle_, viewProjection);

	Object3D::Draw();
}

void Player::Rotate() {
	//// 回転速さ [ラジアン/frame]
	//const float kRotSpeed = 0.02f;

	//// 押した方向で移動ベクトルを変更
	//if (input_->PushKey(DIK_A)) {
	//	worldTransform_.rotation_.y -= kRotSpeed;
	//} else if (input_->PushKey(DIK_D)) {
	//	worldTransform_.rotation_.y += kRotSpeed;
	//}
}

Vec3 Player::TransformNormal(const Vec3 v, const Mat4& m) {
	return { v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0], v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1], v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] };
}

void Player::Attack() {
	if (input_->TriggerKey(DIK_SPACE)) {
		Console::Print("Im Shooting!!\n");

		//// 弾の速度
		//const float kBulletSpeed = 5.0f;

		//Vec3 reticle = { worldTransform3DReticle_.matWorld_.m[3][0], worldTransform3DReticle_.matWorld_.m[3][1], worldTransform3DReticle_.matWorld_.m[3][2] };

		//Vec3 velocity = reticle - GetWorldPosition();
		//velocity = velocity.Normalize() * kBulletSpeed;
		//// velocity = TransformNormal(velocity, worldTransform_.matWorld_);

		//// 弾を生成し、初期化
		//PlayerBullet* newBullet = new PlayerBullet();
		//newBullet->Initialize(model_, GetWorldPosition(), velocity);

		//// たまを登録する
		//bullets_.push_back(newBullet);
	}
}

Vec3 Player::GetWorldPosition() {
	// ワールド座標を入れる変数
	Vec3 worldPos;
	// ワールド行列の平行移動成分を取得(ワールド座標)
	// worldPos = { worldTransform_.matWorld_.m[3][0], worldTransform_.matWorld_.m[3][1], worldTransform_.matWorld_.m[3][2] };

	return worldPos;
}

