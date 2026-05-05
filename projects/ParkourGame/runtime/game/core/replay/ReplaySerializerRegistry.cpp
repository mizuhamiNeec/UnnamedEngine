#include "ReplaySerializerRegistry.h"

#include "ReplayHash.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

#include "game/core/components/CameraRotatorComponent.h"
#include "game/core/components/character/GameMovementComponent.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"
#include "game/parkour/components/course/CourseProgressComponent.h"

namespace Unnamed {
	void ReplaySerializerRegistry::RegisterComponentSerializer(
		ComponentSerializer serializer
	) {
		if (serializer.stableName.empty()) {
			return;
		}
		mSerializers[serializer.stableName] = std::move(serializer);
	}

	bool ReplaySerializerRegistry::SerializeEntity(
		const Entity& entity, nlohmann::json& outState, uint64_t& outHash
	) const {
		outState         = nlohmann::json::object();
		outState["components"] = nlohmann::json::object();
		uint64_t hash    = ReplayHash::Begin();
		uint32_t written = 0;

		for (const auto& [stableName, serializer] : mSerializers) {
			const BaseComponent* component = FindComponentByStableName(
				entity, stableName
			);
			if (component == nullptr) {
				continue;
			}

			nlohmann::json componentState = nlohmann::json::object();
			if (serializer.writeState) {
				serializer.writeState(*component, componentState);
			}
			outState["components"][stableName] = std::move(componentState);
			++written;

			ReplayHash::AppendString(hash, stableName);
			const uint64_t componentHash = serializer.hashState ?
				                               serializer.hashState(*component) :
				                               0ull;
			ReplayHash::AppendPod(hash, componentHash);
		}

		ReplayHash::AppendPod(hash, written);
		outHash = hash;
		return written > 0;
	}

	void ReplaySerializerRegistry::DeserializeEntity(
		Entity& entity, const nlohmann::json& inState
	) const {
		if (!inState.is_object()) {
			return;
		}
		const auto itComponents = inState.find("components");
		if (itComponents == inState.end() || !itComponents->is_object()) {
			return;
		}

		for (const auto& [stableName, serializer] : mSerializers) {
			const auto it = itComponents->find(stableName);
			if (it == itComponents->end()) {
				continue;
			}
			BaseComponent* component = FindComponentByStableName(entity, stableName);
			if (component == nullptr || !serializer.readState) {
				continue;
			}
			serializer.readState(*component, *it);
		}
	}

	ReplaySerializerRegistry ReplaySerializerRegistry::BuildDefault() {
		ReplaySerializerRegistry registry = {};

		registry.RegisterComponentSerializer(
			{
				.stableName = "engine.Transform",
				.writeState =
					[](const BaseComponent& component, nlohmann::json& outState) {
						const auto* typed = dynamic_cast<const TransformComponent*>(
							&component
						);
						if (!typed) {
							return;
						}
						const Vec3       position = typed->GetPosition();
						const Quaternion rotation = typed->GetRotation();
						const Vec3       scale    = typed->GetScale();
						outState["position"] = nlohmann::json::array(
							{position.x, position.y, position.z}
						);
						outState["rotation"] = nlohmann::json::array(
							{rotation.x, rotation.y, rotation.z, rotation.w}
						);
						outState["scale"] = nlohmann::json::array(
							{scale.x, scale.y, scale.z}
						);
					},
				.readState =
					[](BaseComponent& component, const nlohmann::json& inState) {
						auto* typed = dynamic_cast<TransformComponent*>(&component);
						if (!typed || !inState.is_object()) {
							return;
						}
						if (const auto itPos = inState.find("position");
							itPos != inState.end() && itPos->is_array() &&
							itPos->size() == 3) {
							typed->SetPosition(
								Vec3(
									(*itPos)[0].get<float>(),
									(*itPos)[1].get<float>(),
									(*itPos)[2].get<float>()
								)
							);
						}
						if (const auto itRot = inState.find("rotation");
							itRot != inState.end() && itRot->is_array() &&
							itRot->size() == 4) {
							typed->SetRotation(
								Quaternion(
									(*itRot)[0].get<float>(),
									(*itRot)[1].get<float>(),
									(*itRot)[2].get<float>(),
									(*itRot)[3].get<float>()
								)
							);
						}
						if (const auto itScale = inState.find("scale");
							itScale != inState.end() && itScale->is_array() &&
							itScale->size() == 3) {
							typed->SetScale(
								Vec3(
									(*itScale)[0].get<float>(),
									(*itScale)[1].get<float>(),
									(*itScale)[2].get<float>()
								)
							);
						}
					},
				.hashState = [](const BaseComponent& component) -> uint64_t {
					const auto* typed = dynamic_cast<const TransformComponent*>(
						&component
					);
					if (!typed) {
						return 0ull;
					}
					const Vec3       position = typed->GetPosition();
					const Quaternion rotation = typed->GetRotation();
					const Vec3       scale    = typed->GetScale();
					uint64_t         hash     = ReplayHash::Begin();
					ReplayHash::AppendFloating(hash, position.x);
					ReplayHash::AppendFloating(hash, position.y);
					ReplayHash::AppendFloating(hash, position.z);
					ReplayHash::AppendFloating(hash, rotation.x);
					ReplayHash::AppendFloating(hash, rotation.y);
					ReplayHash::AppendFloating(hash, rotation.z);
					ReplayHash::AppendFloating(hash, rotation.w);
					ReplayHash::AppendFloating(hash, scale.x);
					ReplayHash::AppendFloating(hash, scale.y);
					ReplayHash::AppendFloating(hash, scale.z);
					return hash;
				}
			}
		);

		registry.RegisterComponentSerializer(
			{
				.stableName = "game.GameMovement",
				.writeState =
					[](const BaseComponent& component, nlohmann::json& outState) {
						if (const auto* typed = dynamic_cast<
								const GameMovementComponent*>(&component)) {
							typed->WriteReplayState(outState);
						}
					},
				.readState =
					[](BaseComponent& component, const nlohmann::json& inState) {
						if (auto* typed = dynamic_cast<GameMovementComponent*>(
								&component)) {
							typed->ReadReplayState(inState);
						}
					},
				.hashState = [](const BaseComponent& component) -> uint64_t {
					if (const auto* typed = dynamic_cast<
							const GameMovementComponent*>(&component)) {
						return typed->ComputeReplayStateHash();
					}
					return 0ull;
				}
			}
		);

		registry.RegisterComponentSerializer(
			{
				.stableName = "parkour.ParkourMovement",
				.writeState =
					[](const BaseComponent& component, nlohmann::json& outState) {
						if (const auto* typed = dynamic_cast<
								const ParkourMovementComponent*>(&component)) {
							typed->WriteReplayState(outState);
						}
					},
				.readState =
					[](BaseComponent& component, const nlohmann::json& inState) {
						if (auto* typed = dynamic_cast<ParkourMovementComponent*>(
								&component)) {
							typed->ReadReplayState(inState);
						}
					},
				.hashState = [](const BaseComponent& component) -> uint64_t {
					if (const auto* typed = dynamic_cast<
							const ParkourMovementComponent*>(&component)) {
						return typed->ComputeReplayStateHash();
					}
					return 0ull;
				}
			}
		);

		registry.RegisterComponentSerializer(
			{
				.stableName = "parkour.CourseProgress",
				.writeState =
					[](const BaseComponent& component, nlohmann::json& outState) {
						if (const auto* typed = dynamic_cast<
								const CourseProgressComponent*>(&component)) {
							typed->WriteReplayState(outState);
						}
					},
				.readState =
					[](BaseComponent& component, const nlohmann::json& inState) {
						if (auto* typed = dynamic_cast<CourseProgressComponent*>(
								&component)) {
							typed->ReadReplayState(inState);
						}
					},
				.hashState = [](const BaseComponent& component) -> uint64_t {
					if (const auto* typed = dynamic_cast<
							const CourseProgressComponent*>(&component)) {
						return typed->ComputeReplayStateHash();
					}
					return 0ull;
				}
			}
		);

		registry.RegisterComponentSerializer(
			{
				.stableName = "game.CameraRotator",
				.writeState =
					[](const BaseComponent& component, nlohmann::json& outState) {
						if (const auto* typed = dynamic_cast<
								const CameraRotatorComponent*>(&component)) {
							typed->WriteReplayState(outState);
						}
					},
				.readState =
					[](BaseComponent& component, const nlohmann::json& inState) {
						if (auto* typed = dynamic_cast<CameraRotatorComponent*>(
								&component)) {
							typed->ReadReplayState(inState);
						}
					},
				.hashState = [](const BaseComponent& component) -> uint64_t {
					if (const auto* typed = dynamic_cast<
							const CameraRotatorComponent*>(&component)) {
						return typed->ComputeReplayStateHash();
					}
					return 0ull;
				}
			}
		);

		return registry;
	}

	const BaseComponent* ReplaySerializerRegistry::FindComponentByStableName(
		const Entity& entity, const std::string& stableName
	) const {
		const BaseComponent* found = nullptr;
		entity.ForEachComponent(
			[&](const BaseComponent& component) -> bool {
				if (component.GetStableName() == stableName) {
					found = &component;
					return false;
				}
				return true;
			}
		);
		return found;
	}

	BaseComponent* ReplaySerializerRegistry::FindComponentByStableName(
		Entity& entity, const std::string& stableName
	) const {
		BaseComponent* found = nullptr;
		entity.ForEachComponent(
			[&](BaseComponent& component) -> bool {
				if (component.GetStableName() == stableName) {
					found = &component;
					return false;
				}
				return true;
			}
		);
		return found;
	}
}
