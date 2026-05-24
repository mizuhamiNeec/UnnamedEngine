#include "StaticMeshColliderComponent.h"

#include <imgui.h>

#include "../TransformComponent.h"

#include "../../entity/Entity.h"

#include "../mesh/StaticMeshRendererComponent.h"

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/physics/core/Physics.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"


namespace Unnamed {
	void StaticMeshColliderComponent::OnAttached() {
		BaseComponent::OnAttached();
		RefreshPhysicsRegistration();
	}

	void StaticMeshColliderComponent::OnDetached() {
		UnregisterPhysicsMesh();
		BaseComponent::OnDetached();
	}

	void StaticMeshColliderComponent::OnTick(float deltaTime) {
		BaseComponent::OnTick(deltaTime);
		RefreshPhysicsRegistration();
	}

	BaseComponent::TICK_GROUP StaticMeshColliderComponent::
	GetTickGroup() const {
		return TICK_GROUP::COLLIDER_SYNC;
	}

	std::string_view StaticMeshColliderComponent::GetStableName() const {
		return "engine.StaticMeshCollider";
	}

	std::string_view StaticMeshColliderComponent::GetComponentName() const {
		return "StaticMeshCollider";
	}

#ifdef UNNAMED_WITH_EDITOR
	void StaticMeshColliderComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::Checkbox("Dynamic", &mDynamic);
	}
#endif

	void StaticMeshColliderComponent::Deserialize(const JsonReader& reader) {
		const JsonReader enabled = reader["enabled"];
		if (enabled.Valid()) {
			mEnabled = enabled.GetBool();
		}
		const JsonReader dynamic = reader["dynamic"];
		if (dynamic.Valid()) {
			mDynamic = dynamic.GetBool();
		}
	}

	void StaticMeshColliderComponent::Serialize(JsonWriter& writer) const {
		writer.Key("enabled");
		writer.Write(mEnabled);
		writer.Key("dynamic");
		writer.Write(mDynamic);
	}

	uint32_t StaticMeshColliderComponent::GetIcon() const {
		return kIconExplosion;
	}

	bool StaticMeshColliderComponent::IsCollisionEnabled() const noexcept {
		return mEnabled;
	}

	bool StaticMeshColliderComponent::IsDynamicMesh() const noexcept {
		return mDynamic;
	}

	void StaticMeshColliderComponent::RefreshPhysicsRegistration() {
		if (!mEnabled) {
			UnregisterPhysicsMesh();
			return;
		}

		Entity* owner     = GetOwner();
		auto*   transform = owner ?
			                  owner->GetComponent<TransformComponent>() :
			                  nullptr;
		auto* meshRenderer =
			owner ?
				owner->GetComponent<StaticMeshRendererComponent>() :
				nullptr;
		auto*            assetManager = ServiceLocator::Get<AssetManager>();
		Physics::Engine* physics      = GetWorld() ?
			                           &GetWorld()->GetPhysicsEngine() :
			                           nullptr;

		if (!transform || !meshRenderer || !assetManager || !physics) {
			UnregisterPhysicsMesh();
			return;
		}

		const AssetID meshAssetId = meshRenderer->ResolveMeshAsset(
			*assetManager
		);
		const MeshAssetData* mesh = assetManager->Get<MeshAssetData>(
			meshAssetId
		);
		if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) {
			UnregisterPhysicsMesh();
			return;
		}

		const Mat4 world = transform->GetWorldMat();
		if (
			mRegistered &&
			mRegisteredAsDynamic == mDynamic &&
			mRegisteredMeshAssetId == meshAssetId &&
			mRegisteredWorld == world
		) {
			return;
		}

		if (
			mRegistered &&
			mRegisteredAsDynamic &&
			mDynamic &&
			mRegisteredMeshAssetId == meshAssetId &&
			mRegisteredWorld != world
		) {
			if (physics->UpdateDynamicMeshTransform(owner->GetGuid(), world)) {
				mRegisteredWorld = world;
				return;
			}
		}

		UnregisterPhysicsMesh();
		if (!owner) {
			return;
		}

		const bool registered = mDynamic ?
			                        physics->RegisterDynamicMesh(
				                        owner->GetGuid(),
				                        std::span<const MeshVertex>(
					                        mesh->vertices.data(),
					                        mesh->vertices.size()
				                        ),
				                        std::span<const uint32_t>(
					                        mesh->indices.data(),
					                        mesh->indices.size()
				                        ),
				                        world
			                        ) :
			                        physics->RegisterStaticMesh(
				                        owner->GetGuid(),
				                        std::span<const MeshVertex>(
					                        mesh->vertices.data(),
					                        mesh->vertices.size()
				                        ),
				                        std::span<const uint32_t>(
					                        mesh->indices.data(),
					                        mesh->indices.size()
				                        ),
				                        world
			                        );
		if (registered) {
			mRegistered            = true;
			mRegisteredAsDynamic   = mDynamic;
			mRegisteredMeshAssetId = meshAssetId;
			mRegisteredWorld       = world;
		}
	}

	void StaticMeshColliderComponent::UnregisterPhysicsMesh() {
		if (!mRegistered) {
			return;
		}

		if (const Entity* owner = GetOwner()) {
			if (
				Physics::Engine* physics = GetWorld() ?
					                           &GetWorld()->GetPhysicsEngine() :
					                           nullptr
			) {
				physics->UnregisterStaticMesh(owner->GetGuid());
				physics->UnregisterDynamicMesh(owner->GetGuid());
			}
		}

		mRegistered            = false;
		mRegisteredAsDynamic   = false;
		mRegisteredMeshAssetId = kInvalidAssetID;
		mRegisteredWorld       = Mat4::zero;
	}

	REGISTER_COMPONENT(StaticMeshColliderComponent);
}
