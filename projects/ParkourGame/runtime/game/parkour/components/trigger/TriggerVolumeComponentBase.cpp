#include "TriggerVolumeComponentBase.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	Vec3 TriggerVolumeComponentBase::GetLocalCenter() const noexcept {
		return Math::HtoM(mLocalCenterHu);
	}

	Vec3 TriggerVolumeComponentBase::GetWorldCenter() const noexcept {
		const Vec3 localCenterMeters = Math::HtoM(mLocalCenterHu);
		if (const auto* transform = GetTransform()) {
			return transform->GetWorldMat().TransformPoint(localCenterMeters);
		}
		return localCenterMeters;
	}

	Vec3
	TriggerVolumeComponentBase::GetWorldHalfExtentsMeters() const noexcept {
		return Math::HtoM(mExtentsHu * 0.5f);
	}

	void TriggerVolumeComponentBase::DeserializeVolume(
		const JsonReader& reader
	) {
		if (const JsonReader localCenterHu = reader["localCenterHu"];
			localCenterHu.Valid()) {
			mLocalCenterHu = localCenterHu.GetVec3(mLocalCenterHu);
		} else if (const JsonReader legacyLocalCenter = reader["localCenter"];
		           legacyLocalCenter.Valid()) {
			// 旧シーン互換: localCenter はメートル値として読み取り、Huへ移行します。
			mLocalCenterHu = Math::MtoH(
				legacyLocalCenter.GetVec3(Math::HtoM(mLocalCenterHu))
			);
		}
		mExtentsHu = reader["extentsHu"].GetVec3(mExtentsHu);
	}

	void TriggerVolumeComponentBase::SerializeVolume(JsonWriter& writer) const {
		writer.WriteVec3("localCenterHu", mLocalCenterHu);
		writer.WriteVec3("extentsHu", mExtentsHu);
	}

	TransformComponent* TriggerVolumeComponentBase::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

#ifdef _DEBUG
	void TriggerVolumeComponentBase::DrawVolumeInspectorImGui() {
		ImGui::DragFloat3("Local Center HU", &mLocalCenterHu.x, 1.0f);
		ImGui::DragFloat3("Extents HU", &mExtentsHu.x, 1.0f, 0.0f, 100000.0f);
	}
#endif
}
