#include "TransformComponent.h"

#include <algorithm>

#include <engine/ImGui/Icons.h>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		// 描画補間の際、これ以上の速度があるときはワープとみなして補間しないようにします。
		constexpr float kInterpolationMaxVelocityHu = 4096.0f; // HU/s
	}

	Vec3 TransformComponent::GetPosition() const noexcept {
		return mLocalPos;
	}

	Quaternion TransformComponent::GetRotation() const noexcept {
		return mLocalRot;
	}

	Vec3 TransformComponent::GetScale() const noexcept {
		return mLocalScale;
	}

	TransformComponent* TransformComponent::GetParent() const noexcept {
		return mParent;
	}

	const Mat4& TransformComponent::GetWorldMat() const noexcept {
		return mWorldMat;
	}

	const Mat4& TransformComponent::RenderWorldMat() const noexcept {
		// 描画用のワールド行列は、補間されたローカル行列を親の描画用ワールド行列と掛け合わせて計算
		mRenderWorldMat = BuildRenderLocalMatrix();
		if (mParent) {
			mRenderWorldMat = mRenderWorldMat * mParent->RenderWorldMat();
		}
		return mRenderWorldMat;
	}

	void TransformComponent::SetPosition(const Vec3 position) noexcept {
		mLocalPos = position;
		MarkPostSimulationOverride();
		MarkDirty();
	}

	void TransformComponent::SetRotation(const Quaternion rotation) noexcept {
		mLocalRot = rotation;
		MarkPostSimulationOverride();
		MarkDirty();
	}

	void TransformComponent::SetScale(const Vec3 scale) noexcept {
		mLocalScale = scale;
		MarkPostSimulationOverride();
		MarkDirty();
	}

	void TransformComponent::RequestInterpolationResync() noexcept {
		mInterpResyncRequested = true;
	}

	void TransformComponent::SetParent(
		TransformComponent* parent, const bool preserveWorld
	) {
		if (mParent == parent) {
			Warning(GetComponentName(), "SetParent: 新しい親が現在の親と同じです。");
			return;
		}

		// 親候補から祖先を辿り、自分自身に到達するなら循環参照になるため拒否
		for (
			const TransformComponent* candidate = parent;
			candidate;
			candidate = candidate->mParent
		) {
			if (candidate == this) {
				Warning(
					GetComponentName(),
					"SetParent: 循環する親子関係は設定できません。"
				);
				return;
			}
		}

		if (mParent) {
			auto& children = mParent->mChildren;
			std::erase(children, this);
		}

		Vec3       preservedPosition = mLocalPos;
		Quaternion preservedRotation = mLocalRot;
		Vec3       preservedScale    = mLocalScale;

		if (preserveWorld) {
			// ローカルチェーンからワールド行列を構築する関数
			auto BuildWorldFromLocalChain = [](
				const TransformComponent* node
			) {
				Mat4 world = Mat4::Affine(
					node->mLocalScale,
					node->mLocalRot,
					node->mLocalPos
				);
				for (
					const TransformComponent* p = node->mParent;
					p;
					p = p->mParent
				) {
					world =
						world * Mat4::Affine(
							p->mLocalScale,
							p->mLocalRot,
							p->mLocalPos
						);
				}
				return world;
			};

			const Mat4 currentWorld = BuildWorldFromLocalChain(this);

			Mat4 newLocal = currentWorld;
			if (parent) {
				const Mat4 parentWorld = BuildWorldFromLocalChain(parent);
				newLocal               = currentWorld * parentWorld.Inverse();
			}
			preservedPosition = newLocal.GetTranslate();
			preservedRotation = newLocal.ToQuaternion();
			preservedScale    = newLocal.GetScale();
		}

		// 新しい親を設定
		mParent = parent;
		if (mParent) {
			mParent->mChildren.emplace_back(this);
		}
		
		if (preserveWorld) {
			mLocalPos   = preservedPosition;
			mLocalRot   = preservedRotation;
			mLocalScale = preservedScale;
		}
		mPendingParentEntityGuid = 0;

		MarkPostSimulationOverride();

		MarkDirty();
	}

	void TransformComponent::ResolveDeferredParent(const Scene& scene) {
		// 遅延解決が必要な親がいない場合は何もしない
		if (mPendingParentEntityGuid == 0) {
			return;
		}

		// 遅延解決された親 GUID から Transform を取得して親子付けを行う
		const Entity* parentEntity = scene.FindEntity(
			mPendingParentEntityGuid
		);

		// 親エンティティが見つからない場合は、遅延解決を諦める（親なしになる）
		if (!parentEntity) {
			mPendingParentEntityGuid = 0;
			return;
		}

		auto* parentTransform =
			const_cast<Entity*>(parentEntity)->
			GetComponent<TransformComponent>();

		if (!parentTransform) {
			Warning(
				GetComponentName(),
				"ResolveDeferredParent: 親エンティティに TransformComponent が見つかりません。"
			);
			mPendingParentEntityGuid = 0;
			return;
		}
		SetParent(parentTransform, false);
		mPendingParentEntityGuid = 0;
	}

	Vec3 TransformComponent::Right() const noexcept {
		return mWorldMat.GetRight();
	}

	Vec3 TransformComponent::Up() const noexcept {
		return mWorldMat.GetUp();
	}

	Vec3 TransformComponent::Forward() const noexcept {
		return mWorldMat.GetForward();
	}

	void TransformComponent::OnAttached() {
		BaseComponent::OnAttached();
		mInterpPrevPos   = mLocalPos;
		mInterpPrevRot   = mLocalRot;
		mInterpPrevScale = mLocalScale;
		mInterpCurrPos   = mLocalPos;
		mInterpCurrRot   = mLocalRot;
		mInterpCurrScale = mLocalScale;
		if (const World* world = GetWorld()) {
			mInterpSampleTick = world->GetTime().fixedTickCounter;
		} else {
			mInterpSampleTick = 0;
		}
		mInterpInitialized         = true;
		mRenderInterpAlpha         = 1.0f;
		mInterpResyncRequested     = false;
		mHasPostSimulationOverride = false;
		mRenderWorldMat            = mWorldMat;
	}

	void TransformComponent::OnDetached() {
		const auto children = mChildren;
		for (TransformComponent* child : children) {
			if (!child) {
				continue;
			}
			// 子の親を外す（子のリストは SetParent 内で更新されるため、コピーしたリストを使う）
			child->SetParent(nullptr);
		}

		// 親からも自分を外す
		if (mParent) {
			auto& siblings = mParent->mChildren;
			std::erase(siblings, this);
			mParent = nullptr;
		}
		mChildren.clear();
	}

	void TransformComponent::OnTick(float) {
		// シミュレーションを確定し、その結果を補間へ反映する
		UpdateWorldRecursive();
		RefreshInterpolationSamplesFromSimulation();
		mHasPostSimulationOverride = false;
	}

	void TransformComponent::OnRenderTick(
		const float, const float interpolationAlpha
	) {
		// 描画補間係数を受け取って保存する
		// 実際の補間は RenderWorldMat() に任せる
		mRenderInterpAlpha = std::clamp(interpolationAlpha, 0.0f, 1.0f);
	}

	void TransformComponent::OnEditorTick(const float deltaTime) {
		// エディタモードでも行列更新を行う
		OnTick(deltaTime);
	}

	BaseComponent::TICK_GROUP TransformComponent::GetTickGroup() const {
		return TICK_GROUP::KINEMATIC_SOURCE;
	}

	std::string_view TransformComponent::GetStableName() const {
		return "engine.Transform";
	}

	std::string_view TransformComponent::GetComponentName() const {
		return "Transform";
	}

	uint32_t TransformComponent::GetIcon() const {
		return kIconDragPan;
	}

	void TransformComponent::Serialize(JsonWriter& writer) const {
		writer.Key("position");
		writer.BeginArray();
		writer.Write(mLocalPos.x);
		writer.Write(mLocalPos.y);
		writer.Write(mLocalPos.z);
		writer.EndArray();

		writer.Key("rotation");
		writer.BeginArray();
		writer.Write(mLocalRot.x);
		writer.Write(mLocalRot.y);
		writer.Write(mLocalRot.z);
		writer.Write(mLocalRot.w);
		writer.EndArray();

		writer.Key("scale");
		writer.BeginArray();
		writer.Write(mLocalScale.x);
		writer.Write(mLocalScale.y);
		writer.Write(mLocalScale.z);
		writer.EndArray();

		writer.Key("parentEntityGuid");
		writer.Write(
			mParent && mParent->GetOwner() ?
				mParent->GetOwner()->GetGuid() :
				0ull
		);
	}

	void TransformComponent::UpdateWorldRecursive() {
		if (mIsDirty) {
			// ローカル行列を計算
			const Mat4 localMat = Mat4::Affine(
				mLocalScale, mLocalRot, mLocalPos
			);

			// ワールド行列を計算
			if (mParent) {
				// 親がいる場合は親のワールド行列を掛ける
				mWorldMat = localMat * mParent->GetWorldMat();
			} else {
				// 親がいない場合はローカル行列がそのままワールド行列
				mWorldMat = localMat;
			}

			// フラグを外す
			mIsDirty = false;
		}

		// 子も再帰的に更新
		for (TransformComponent* child : mChildren) {
			if (!child || !child->mIsDirty) {
				continue;
			}
			child->UpdateWorldRecursive();
		}
	}

	void TransformComponent::RefreshInterpolationSamplesFromSimulation() {
		const World*   world     = GetWorld();
		const uint64_t fixedTick =
			world ?
				world->GetTime().fixedTickCounter :
				0;

		// 補間履歴が未初期化だったら現在の値で初期化します。
		if (!mInterpInitialized) {
			mInterpPrevPos     = mLocalPos;
			mInterpPrevRot     = mLocalRot;
			mInterpPrevScale   = mLocalScale;
			mInterpCurrPos     = mLocalPos;
			mInterpCurrRot     = mLocalRot;
			mInterpCurrScale   = mLocalScale;
			mInterpSampleTick  = fixedTick;
			mInterpInitialized = true;
			return;
		}

		// 固定ティックが進んだら prev/curr 履歴を更新
		if (fixedTick != mInterpSampleTick) {
			if (mInterpResyncRequested) {
				// 再同期要求がある場合は、prev を現在のシミュレーション結果でリセットします。
				mInterpPrevPos         = mLocalPos;
				mInterpPrevRot         = mLocalRot;
				mInterpPrevScale       = mLocalScale;
				mInterpResyncRequested = false;
			} else {
				// 通常の固定ティック境界では履歴を一段進めます。
				mInterpPrevPos   = mInterpCurrPos;
				mInterpPrevRot   = mInterpCurrRot;
				mInterpPrevScale = mInterpCurrScale;
			}
			mInterpSampleTick = fixedTick;
		}

		// 同一ティック内の最終結果を「現在値」として保持します。
		mInterpCurrPos   = mLocalPos;
		mInterpCurrRot   = mLocalRot;
		mInterpCurrScale = mLocalScale;
	}

	bool TransformComponent::ShouldInterpolateForRender() const {
		// 補間が初期化されていない || シミュレーション後にオーバーライドがある場合は補間しない
		if (!mInterpInitialized || mHasPostSimulationOverride) {
			return false;
		}

		const World* world = GetWorld();
		if (!world) {
			return false; // ワールドがない場合は補間できないので、補間しない（=現在値を使う）ようにします。
		}

		const float fixedDeltaTime = world->GetTime().fixedDeltaTime;
		if (fixedDeltaTime <= 0.0f) {
			return false; // 無効なデルタタイムの場合は補間しないようにします。
		}

		// 前回と今回の位置の差を計算します。
		const Vec3 tickDelta = mInterpCurrPos - mInterpPrevPos;

		// 1ティックあたりの最大移動距離を計算します。
		const float maxDistance =
			kInterpolationMaxVelocityHu * fixedDeltaTime;

		const float maxDistanceSq = maxDistance * maxDistance;

		// 移動距離が一定値を超える場合は、ワープと見なして補間しない
		return tickDelta.SqrLength() <= maxDistanceSq;
	}

	Mat4 TransformComponent::BuildRenderLocalMatrix() const {
		if (!ShouldInterpolateForRender()) {
			return Mat4::Affine(mLocalScale, mLocalRot, mLocalPos);
		}

		// 描画は、前フレームと今のシミュレーション結果を補間する。
		const float alpha = std::clamp(mRenderInterpAlpha, 0.0f, 1.0f);

		const Vec3 scale = Math::Lerp(
			mInterpPrevScale,
			mInterpCurrScale,
			alpha
		);
		const Quaternion rot = Quaternion::Slerp(
			mInterpPrevRot,
			mInterpCurrRot,
			alpha
		);
		const Vec3 pos = Math::Lerp(
			mInterpPrevPos,
			mInterpCurrPos,
			alpha
		);
		return Mat4::Affine(scale, rot, pos);
	}

	void TransformComponent::MarkDirty() {
		mIsDirty = true;

		// 子も再計算
		for (auto* child : mChildren) {
			child->MarkDirty();
		}
	}

	void TransformComponent::MarkPostSimulationOverride() {
		mHasPostSimulationOverride = true;

		// 子もフラグを立てる
		for (auto* child : mChildren) {
			child->MarkPostSimulationOverride();
		}
	}

#ifdef _DEBUG
	void TransformComponent::DrawInspectorImGui() {
		Vec3       localPos   = mLocalPos;
		Quaternion localRot   = mLocalRot;
		Vec3       localScale = mLocalScale;

		// Position 編集
		if (
			ImGuiWidgets::DragVec3(
				"Position", localPos, Vec3::zero, 0.1f, "%.3f"
			)
		) {
			SetPosition(localPos);
		}

		// Rotation 編集
		Vec3 eulerDegrees = localRot.ToEulerDegrees();
		if (
			ImGuiWidgets::DragVec3(
				"Rotation", eulerDegrees, Vec3::zero, 0.1f, "%.3f"
			)
		) {
			// 編集された Euler 角を Quaternion に変換
			localRot = Quaternion::EulerDegrees(eulerDegrees);
			SetRotation(localRot);
		}

		// Scale 編集
		if (
			ImGuiWidgets::DragVec3(
				"Scale", localScale, Vec3::one, 0.1f, "%.3f"
			)
		) {
			SetScale(localScale);
		}
	}
#endif

	void TransformComponent::Deserialize(const JsonReader& reader) {
		mLocalPos   = reader["position"].GetVec3(mLocalPos);
		mLocalRot   = reader["rotation"].GetQuaternion(mLocalRot);
		mLocalScale = reader["scale"].GetVec3(mLocalScale);

		mPendingParentEntityGuid =
			reader.ReadUint64("parentEntityGuid").value_or(0ull);

		MarkPostSimulationOverride();

		MarkDirty();
	}

	// コンポーネント登録
	REGISTER_COMPONENT(TransformComponent);
}
