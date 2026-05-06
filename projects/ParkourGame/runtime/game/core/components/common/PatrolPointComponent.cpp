#include "PatrolPointComponent.h"

#include <algorithm>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	namespace {
		constexpr float kArriveEpsilon = 0.001f;
	}

	void PatrolPointComponent::OnAttached() {
		ApplyStartPoint();
	}

	void PatrolPointComponent::OnTick(const float deltaTime) {
		if (!mEnabled || deltaTime <= 0.0f) {
			return;
		}

		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		if ((mPointB - mPointA).SqrLength() <= kArriveEpsilon *
		    kArriveEpsilon) {
			transform->SetPosition(mPointA);
			return;
		}

		float remainingDistance = std::max(0.0f, mSpeed) * deltaTime;
		if (remainingDistance <= 0.0f) {
			return;
		}

		Vec3 current = transform->GetPosition();
		for (uint32_t i = 0; i < 32 && remainingDistance > 0.0f; ++i) {
			const Vec3  target   = mMoveToPointB ? mPointB : mPointA;
			const Vec3  toTarget = target - current;
			const float distance = toTarget.Length();

			if (distance <= kArriveEpsilon) {
				current       = target;
				mMoveToPointB = !mMoveToPointB;
				continue;
			}

			if (remainingDistance >= distance) {
				current           = target;
				remainingDistance -= distance;
				mMoveToPointB     = !mMoveToPointB;
				continue;
			}

			current           += toTarget * (remainingDistance / distance);
			remainingDistance = 0.0f;
		}

		transform->SetPosition(current);
	}

	BaseComponent::TICK_GROUP PatrolPointComponent::GetTickGroup() const {
		// Transform/KinematicMover/ColliderSync より前に移動ソースを更新する。
		return TICK_GROUP::EARLY;
	}

	std::string_view PatrolPointComponent::GetStableName() const {
		return "game.PatrolPoint";
	}

	std::string_view PatrolPointComponent::GetComponentName() const {
		return "PatrolPoint";
	}

	void PatrolPointComponent::Deserialize(const JsonReader& reader) {
		mPointA = reader["pointA"].GetVec3(mPointA);
		mPointB = reader["pointB"].GetVec3(mPointB);

		const JsonReader speed = reader["speed"];
		if (speed.Valid()) {
			mSpeed = speed.GetFloat();
		}

		const JsonReader enabled = reader["enabled"];
		if (enabled.Valid()) {
			mEnabled = enabled.GetBool();
		}

		const JsonReader startFromA = reader["startFromA"];
		if (startFromA.Valid()) {
			mStartFromA = startFromA.GetBool();
		}

		const JsonReader moveToPointB = reader["moveToPointB"];
		if (moveToPointB.Valid()) {
			mMoveToPointB = moveToPointB.GetBool();
		} else {
			mMoveToPointB = mStartFromA;
		}

		const JsonReader snapOnAttach = reader["snapOnAttach"];
		if (snapOnAttach.Valid()) {
			mSnapOnAttach = snapOnAttach.GetBool();
		}
	}

	void PatrolPointComponent::Serialize(JsonWriter& writer) const {
		writer.WriteVec3("pointA", mPointA);
		writer.WriteVec3("pointB", mPointB);
		writer.Key("speed");
		writer.Write(mSpeed);
		writer.Key("enabled");
		writer.Write(mEnabled);
		writer.Key("startFromA");
		writer.Write(mStartFromA);
		writer.Key("moveToPointB");
		writer.Write(mMoveToPointB);
		writer.Key("snapOnAttach");
		writer.Write(mSnapOnAttach);
	}

	uint32_t PatrolPointComponent::GetIcon() const {
		return kIconSiren;
	}

#ifdef _DEBUG
	void PatrolPointComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::DragFloat3("PointA", &mPointA.x, 1.0f);
		ImGui::DragFloat3("PointB", &mPointB.x, 1.0f);
		ImGui::DragFloat("Speed", &mSpeed, 1.0f, 0.0f, 10000.0f);
		ImGui::Checkbox("StartFromA", &mStartFromA);
		ImGui::Checkbox("MoveToPointB", &mMoveToPointB);
		ImGui::Checkbox("SnapOnAttach", &mSnapOnAttach);

		if (ImGui::Button("ApplyStartPoint")) {
			ApplyStartPoint();
		}
	}
#endif

	TransformComponent* PatrolPointComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	void PatrolPointComponent::ApplyStartPoint() {
		if (mSnapOnAttach) {
			if (auto* transform = GetTransform()) {
				transform->SetPosition(mStartFromA ? mPointA : mPointB);
			}
		}
		mMoveToPointB = mStartFromA;
	}

	REGISTER_COMPONENT(PatrolPointComponent);
}
