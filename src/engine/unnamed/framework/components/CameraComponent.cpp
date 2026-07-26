#include "CameraComponent.h"

#include <algorithm>

#include <imgui.h>

#include <engine/ImGui/Icons.h>

#include "TransformComponent.h"

#include "engine/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

namespace Unnamed {
	void CameraComponent::Deserialize(const JsonReader& reader) {
		mFovYDegrees =
			reader.ReadFloatOr("fovYDegrees", mFovYDegrees);
		mNearZ        = reader.ReadFloatOr("nearZ", mNearZ);
		mFarZ         = reader.ReadFloatOr("farZ", mFarZ);
		mExposureEv   = reader.ReadFloatOr("exposureEv", mExposureEv);
		mCameraActive = reader.ReadBoolOr("cameraActive", mCameraActive);
	}

	void CameraComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fovYDegrees");
		writer.Write(mFovYDegrees);
		writer.Key("nearZ");
		writer.Write(mNearZ);
		writer.Key("farZ");
		writer.Write(mFarZ);
		writer.Key("exposureEv");
		writer.Write(mExposureEv);
		writer.Key("cameraActive");
		writer.Write(mCameraActive);
	}

	uint32_t CameraComponent::GetIcon() const {
		return kIconVideoCam;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void CameraComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Camera Active", &mCameraActive);
		ImGui::DragFloat("FovYDegrees", &mFovYDegrees, 0.1f, 1.0f, 179.0f);
		ImGui::DragFloat("NearZ", &mNearZ, 0.001f, 0.001f, mFarZ - 0.01f);
		ImGui::DragFloat("FarZ", &mFarZ, 1.0f, mNearZ + 0.01f, 100000.0f);
		ImGui::DragFloat("ExposureEV", &mExposureEv, 0.05f, -16.0f, 16.0f);
		ImGui::Text("AspectRatio: %.3f", mAspectRatio);
	}
#endif

	void CameraComponent::SetAspectRatio(const float aspectRatio) {
		mAspectRatio = aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f;
	}

	void CameraComponent::SetCameraActive(const bool active) noexcept {
		mCameraActive = active;
	}

	float CameraComponent::GetAspectRatio() const noexcept {
		return mAspectRatio;
	}

	bool CameraComponent::IsCameraActive() const noexcept {
		return mCameraActive;
	}

	bool CameraComponent::BuildCameraInput(
		Render::RenderCameraInput& outCamera
	) const {
		if (!mCameraActive) {
			return false;
		}

		const auto* transform = GetTransform();
		if (!transform) {
			return false;
		}

		const Mat4 world = transform->RenderWorldMat();
		outCamera.view   = world.Inverse();
		outCamera.proj   = Mat4::PerspectiveFovD3D(
			mFovYDegrees * Math::deg2Rad,
			mAspectRatio,
			mNearZ,
			mFarZ,
			ProjectionDepthMode::ReverseZ
		);
		outCamera.viewProj   = outCamera.view * outCamera.proj;
		outCamera.cameraPos  = world.TransformPoint(Vec3::zero);
		outCamera.exposureEv = mExposureEv;
		outCamera.nearZ      = mNearZ;
		outCamera.farZ       = mFarZ;
		outCamera.depthMode  = Render::PROJECTION_DEPTH_MODE::REVERSE_Z;
		outCamera.valid      = true;
		return true;
	}

	void CameraComponent::SetFovYDegrees(const float fovYDegrees) {
		mFovYDegrees = std::clamp(fovYDegrees, 1.0f, 179.0f);
	}

	float CameraComponent::GetFovYDegrees() const noexcept {
		return mFovYDegrees;
	}

	bool CameraComponent::SetAsCurrentCamera() const {
		const Entity* owner = GetOwner();
		if (!owner) {
			return false;
		}

		World* world = GetWorld();
		if (!world) {
			return false;
		}

		return world->GetCameraManager().SetCurrentCamera(owner->GetGuid());
	}

	bool CameraComponent::IsCurrentCamera() const {
		const Entity* owner = GetOwner();
		if (!owner) {
			return false;
		}

		const World* world = GetWorld();
		if (!world) {
			return false;
		}

		const auto& manager = world->GetCameraManager();
		if (!manager.HasCurrentCamera()) {
			return false;
		}

		return manager.GetCurrentCameraGuid() == owner->GetGuid();
	}

	std::string_view CameraComponent::GetStableName() const {
		return "game.Camera";
	}

	std::string_view CameraComponent::GetComponentName() const {
		return "Camera";
	}

	TransformComponent* CameraComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(CameraComponent);
}
