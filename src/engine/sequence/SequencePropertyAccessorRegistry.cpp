#include "SequencePropertyAccessorRegistry.h"

#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"

namespace Unnamed {
	namespace {
		void RegisterTransformAccessors(
			SequencePropertyAccessorRegistry& registry
		) {
			registry.RegisterFloat(
				"engine.Transform",
				"position.x",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetPosition().x;
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							Vec3 position = transform->GetPosition();
							position.x    = value;
							transform->SetPosition(position);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterFloat(
				"engine.Transform",
				"position.y",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetPosition().y;
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							Vec3 position = transform->GetPosition();
							position.y    = value;
							transform->SetPosition(position);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterFloat(
				"engine.Transform",
				"position.z",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetPosition().z;
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							Vec3 position = transform->GetPosition();
							position.z    = value;
							transform->SetPosition(position);
							return true;
						}
						return false;
					},
				}
			);

			registry.RegisterFloat(
				"engine.Transform",
				"scale.x",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetScale().x;
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							Vec3 scale = transform->GetScale();
							scale.x    = value;
							transform->SetScale(scale);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterFloat(
				"engine.Transform",
				"scale.y",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetScale().y;
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							Vec3 scale = transform->GetScale();
							scale.y    = value;
							transform->SetScale(scale);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterFloat(
				"engine.Transform",
				"scale.z",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetScale().z;
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							Vec3 scale = transform->GetScale();
							scale.z    = value;
							transform->SetScale(scale);
							return true;
						}
						return false;
					},
				}
			);

			registry.RegisterVec3(
				"engine.Transform",
				"position",
				SequenceVec3Accessor{
					.get = [](Entity& entity, Vec3& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetPosition();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const Vec3& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							transform->SetPosition(value);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterVec3(
				"engine.Transform",
				"scale",
				SequenceVec3Accessor{
					.get = [](Entity& entity, Vec3& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetScale();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const Vec3& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							transform->SetScale(value);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterVec3(
				"engine.Transform",
				"rotation.euler",
				SequenceVec3Accessor{
					.get = [](Entity& entity, Vec3& outValue) {
						if (const auto* transform = entity.GetComponent<
							TransformComponent>()) {
							outValue = transform->GetRotation().
							                      ToEulerDegrees();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const Vec3& value) {
						if (auto* transform = entity.GetComponent<
							TransformComponent>()) {
							transform->SetRotation(
								Quaternion::EulerDegrees(value));
							return true;
						}
						return false;
					},
				}
			);
		}

		void RegisterCameraAccessors(
			SequencePropertyAccessorRegistry& registry
		) {
			registry.RegisterFloat(
				"game.Camera",
				"fovYDegrees",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* camera = entity.GetComponent<
							CameraComponent>()) {
							outValue = camera->GetFovYDegrees();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* camera = entity.GetComponent<
							CameraComponent>()) {
							camera->SetFovYDegrees(value);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterBool(
				"game.Camera",
				"active",
				SequenceBoolAccessor{
					.get = [](Entity& entity, bool& outValue) {
						if (const auto* camera = entity.GetComponent<
							CameraComponent>()) {
							outValue = camera->IsCameraActive();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const bool& value) {
						if (auto* camera = entity.GetComponent<
							CameraComponent>()) {
							camera->SetCameraActive(value);
							return true;
						}
						return false;
					},
				}
			);
		}

		void RegisterSkeletalAccessors(
			SequencePropertyAccessorRegistry& registry
		) {
			registry.RegisterFloat(
				"engine.SkeletalAnimation",
				"speed",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							outValue = component->GetSpeed();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							component->SetSpeed(value);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterFloat(
				"engine.SkeletalAnimation",
				"playbackTime",
				SequenceFloatAccessor{
					.get = [](Entity& entity, float& outValue) {
						if (const auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							outValue = component->GetPlaybackTime();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const float& value) {
						if (auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							component->SetPlaybackTime(value);
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterBool(
				"engine.SkeletalAnimation",
				"playing",
				SequenceBoolAccessor{
					.get = [](Entity& entity, bool& outValue) {
						if (const auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							outValue = component->IsPlaying();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const bool& value) {
						if (auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							if (value) {
								component->Play();
							} else {
								component->Pause();
							}
							return true;
						}
						return false;
					},
				}
			);
			registry.RegisterBool(
				"engine.SkeletalAnimation",
				"loop",
				SequenceBoolAccessor{
					.get = [](Entity& entity, bool& outValue) {
						if (const auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							outValue = component->GetLoop();
							return true;
						}
						return false;
					},
					.set = [](Entity& entity, const bool& value) {
						if (auto* component = entity.GetComponent<
							SkeletalAnimationComponent>()) {
							component->SetLoop(value);
							return true;
						}
						return false;
					},
				}
			);
		}
	}

	SequencePropertyAccessorRegistry::SequencePropertyAccessorRegistry() {
		// シリアライズに使う安定名をキーにし、実装型名の変更からシーケンスを守る
		RegisterTransformAccessors(*this);
		RegisterCameraAccessors(*this);
		RegisterSkeletalAccessors(*this);
	}

	void SequencePropertyAccessorRegistry::RegisterFloat(
		const std::string&    componentStableName,
		const std::string&    propertyPath,
		SequenceFloatAccessor accessor
	) {
		mFloatAccessors[BuildKey(componentStableName, propertyPath)] =
			std::move(
				accessor
			);
	}

	void SequencePropertyAccessorRegistry::RegisterBool(
		const std::string&   componentStableName,
		const std::string&   propertyPath,
		SequenceBoolAccessor accessor
	) {
		mBoolAccessors[BuildKey(componentStableName, propertyPath)] = std::move(
			accessor
		);
	}

	void SequencePropertyAccessorRegistry::RegisterVec3(
		const std::string&   componentStableName,
		const std::string&   propertyPath,
		SequenceVec3Accessor accessor
	) {
		mVec3Accessors[BuildKey(componentStableName, propertyPath)] = std::move(
			accessor
		);
	}

	const SequenceFloatAccessor* SequencePropertyAccessorRegistry::FindFloat(
		const std::string_view componentStableName,
		const std::string_view propertyPath
	) const {
		const auto it = mFloatAccessors.find(
			BuildKey(componentStableName, propertyPath)
		);
		return it == mFloatAccessors.end() ? nullptr : &it->second;
	}

	const SequenceBoolAccessor* SequencePropertyAccessorRegistry::FindBool(
		const std::string_view componentStableName,
		const std::string_view propertyPath
	) const {
		const auto it = mBoolAccessors.find(
			BuildKey(componentStableName, propertyPath)
		);
		return it == mBoolAccessors.end() ? nullptr : &it->second;
	}

	const SequenceVec3Accessor* SequencePropertyAccessorRegistry::FindVec3(
		const std::string_view componentStableName,
		const std::string_view propertyPath
	) const {
		const auto it = mVec3Accessors.find(
			BuildKey(componentStableName, propertyPath)
		);
		return it == mVec3Accessors.end() ? nullptr : &it->second;
	}

	std::string SequencePropertyAccessorRegistry::BuildKey(
		const std::string_view componentStableName,
		const std::string_view propertyPath
	) {
		std::string key = std::string(componentStableName);
		key             += "|";
		key             += std::string(propertyPath);
		return key;
	}
}
