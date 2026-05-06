#include "CameraFxControllerComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/tween/TweenEase.h"

#include "engine/ImGui/Icons.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		struct EaseNamePair {
			std::string_view name = "LINEAR";
			EASE_TYPE        type = EASE_TYPE::LINEAR;
		};

		[[nodiscard]] float ComputeLerpAlpha(
			const float speedPerSec, const float deltaTime
		) {
			return std::clamp(speedPerSec * deltaTime, 0.0f, 1.0f);
		}

		[[nodiscard]] float ResolveLerpSpeed(const float durationSec) {
			// Avoid one-frame teleport when duration is zero or extremely short.
			constexpr float kMinDurationSec = 0.05f;
			return 1.0f / std::max(durationSec, kMinDurationSec);
		}

		[[nodiscard]] uint32_t Hash1D(const int x, const uint32_t seed) {
			uint32_t h = static_cast<uint32_t>(x) ^ seed;
			h          ^= h >> 16;
			h          *= 0x7feb352du;
			h          ^= h >> 15;
			h          *= 0x846ca68bu;
			h          ^= h >> 16;
			return h;
		}

		[[nodiscard]] float HashToUnit(const uint32_t value) {
			constexpr float kInv24Bit = 1.0f / 16777215.0f;
			return static_cast<float>(value & 0x00ffffffu) * kInv24Bit;
		}

		[[nodiscard]] float PerlinFade(const float t) {
			return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
		}

		[[nodiscard]] float PerlinNoise1D(const float x, const uint32_t seed) {
			const int   x0 = static_cast<int>(std::floor(x));
			const int   x1 = x0 + 1;
			const float t  = x - static_cast<float>(x0);

			const float gradient0 = HashToUnit(Hash1D(x0, seed)) * 2.0f - 1.0f;
			const float gradient1 = HashToUnit(Hash1D(x1, seed)) * 2.0f - 1.0f;
			const float dot0      = gradient0 * t;
			const float dot1      = gradient1 * (t - 1.0f);

			return Math::Lerp(dot0, dot1, PerlinFade(t)) * 2.0f;
		}

		constexpr EaseNamePair kEaseNames[] = {
			{.name = "LINEAR", .type = EASE_TYPE::LINEAR},
			{.name = "IN_SINE", .type = EASE_TYPE::IN_SINE},
			{.name = "OUT_SINE", .type = EASE_TYPE::OUT_SINE},
			{.name = "IN_OUT_SINE", .type = EASE_TYPE::IN_OUT_SINE},
			{.name = "IN_QUAD", .type = EASE_TYPE::IN_QUAD},
			{.name = "OUT_QUAD", .type = EASE_TYPE::OUT_QUAD},
			{.name = "IN_OUT_QUAD", .type = EASE_TYPE::IN_OUT_QUAD},
			{.name = "IN_CUBIC", .type = EASE_TYPE::IN_CUBIC},
			{.name = "OUT_CUBIC", .type = EASE_TYPE::OUT_CUBIC},
			{.name = "IN_OUT_CUBIC", .type = EASE_TYPE::IN_OUT_CUBIC},
			{.name = "IN_QUART", .type = EASE_TYPE::IN_QUART},
			{.name = "OUT_QUART", .type = EASE_TYPE::OUT_QUART},
			{.name = "IN_OUT_QUART", .type = EASE_TYPE::IN_OUT_QUART},
			{.name = "IN_QUINT", .type = EASE_TYPE::IN_QUINT},
			{.name = "OUT_QUINT", .type = EASE_TYPE::OUT_QUINT},
			{.name = "IN_OUT_QUINT", .type = EASE_TYPE::IN_OUT_QUINT},
			{.name = "IN_EXPO", .type = EASE_TYPE::IN_EXPO},
			{.name = "OUT_EXPO", .type = EASE_TYPE::OUT_EXPO},
			{.name = "IN_OUT_EXPO", .type = EASE_TYPE::IN_OUT_EXPO},
			{.name = "IN_CIRC", .type = EASE_TYPE::IN_CIRC},
			{.name = "OUT_CIRC", .type = EASE_TYPE::OUT_CIRC},
			{.name = "IN_OUT_CIRC", .type = EASE_TYPE::IN_OUT_CIRC},
			{.name = "IN_BACK", .type = EASE_TYPE::IN_BACK},
			{.name = "OUT_BACK", .type = EASE_TYPE::OUT_BACK},
			{.name = "IN_OUT_BACK", .type = EASE_TYPE::IN_OUT_BACK},
			{.name = "IN_ELASTIC", .type = EASE_TYPE::IN_ELASTIC},
			{.name = "OUT_ELASTIC", .type = EASE_TYPE::OUT_ELASTIC},
			{.name = "IN_OUT_ELASTIC", .type = EASE_TYPE::IN_OUT_ELASTIC},
			{.name = "IN_BOUNCE", .type = EASE_TYPE::IN_BOUNCE},
			{.name = "OUT_BOUNCE", .type = EASE_TYPE::OUT_BOUNCE},
			{.name = "IN_OUT_BOUNCE", .type = EASE_TYPE::IN_OUT_BOUNCE},
		};

#ifdef _DEBUG
		bool EditStringField(
			const char* label, std::string& value, const size_t capacity = 128
		) {
			std::vector<char> buffer(capacity, '\0');
			const size_t      copyLength = std::min(value.size(), capacity - 1);
			if (copyLength > 0) {
				std::memcpy(buffer.data(), value.data(), copyLength);
			}
			if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
				return false;
			}
			value = buffer.data();
			return true;
		}

		bool EditEntityGuidField(const char* label, uint64_t& guid) {
			return ImGui::InputScalar(label, ImGuiDataType_U64, &guid);
		}
#endif
	}

	void CameraFxControllerComponent::OnAttached() {
		mCamera         = ResolveCamera();
		mShakeTransform = ResolveShakeTransform();
		if (mCamera) {
			// Track external FOV changes while preserving our current offset.
			mBaseFovYDegrees = mCamera->GetFovYDegrees() - mCurrentFovOffset;
		}
		if (mCamera) {
			mBaseFovYDegrees = mCamera->GetFovYDegrees();
		}
	}

	void CameraFxControllerComponent::OnDetached() {
		ResetOutputs();
		mActiveShakes.clear();
		mActiveFovAnim      = {};
		mActiveRotationAnim = {};
	}

	void CameraFxControllerComponent::OnRenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		(void)interpolationAlpha;
		if (renderDeltaTime <= 0.0f) {
			return;
		}

		mCamera         = ResolveCamera();
		mShakeTransform = ResolveShakeTransform();

		Vec2 shakeOffset = Vec2::zero;
		for (size_t i = 0; i < mActiveShakes.size();) {
			ActiveShake& shake = mActiveShakes[i];
			shake.elapsedSec   += renderDeltaTime;
			if (shake.elapsedSec >= shake.durationSec) {
				mActiveShakes.erase(
					mActiveShakes.begin() + static_cast<std::ptrdiff_t>(i)
				);
				continue;
			}

			const float normalizedTime = std::clamp(
				shake.elapsedSec / std::max(shake.durationSec, 1.0e-4f),
				0.0f,
				1.0f
			);
			const float envelope = std::pow(
				1.0f - normalizedTime, std::max(0.01f, shake.decay)
			);
			shake.noiseTimeSec     += shake.frequencyHz * renderDeltaTime;
			const float pitchNoise = PerlinNoise1D(
				shake.noiseTimeSec + 17.13f, shake.seedPitch
			);
			const float yawNoise = PerlinNoise1D(
				shake.noiseTimeSec + 91.47f, shake.seedYaw
			);
			shakeOffset.x += pitchNoise * shake.amplitudeDeg.x * envelope;
			shakeOffset.y += yawNoise * shake.amplitudeDeg.y * envelope;
			++i;
		}

		float       fovOffset = mCurrentFovOffset;
		const float fovSpeed  = std::max(0.0f, mActiveFovAnim.lerpSpeed);
		if (fovSpeed > 0.0f) {
			fovOffset = Math::Lerp(
				fovOffset,
				mActiveFovAnim.targetDeltaDeg,
				ComputeLerpAlpha(fovSpeed, renderDeltaTime)
			);
		} else {
			fovOffset = mActiveFovAnim.targetDeltaDeg;
		}

		Vec3 rotationOffset = mCurrentRotationOffsetDeg;
		if (mActiveRotationAnim.active) {
			const float inSec = std::max(0.0f, mActiveRotationAnim.inSec);
			const float outSec = std::max(0.0f, mActiveRotationAnim.outSec);
			const float totalDuration = inSec + outSec;
			if (totalDuration <= 1.0e-6f) {
				mActiveRotationAnim.active = false;
				rotationOffset             = Vec3::zero;
			} else {
				const float elapsedSec = std::min(
					mActiveRotationAnim.elapsedSec + renderDeltaTime,
					totalDuration
				);
				if (elapsedSec <= inSec) {
					const float t = inSec > 1.0e-6f ? elapsedSec / inSec : 1.0f;
					const float easedT = TweenEase::Evaluate(
						mActiveRotationAnim.ease,
						std::clamp(t, 0.0f, 1.0f)
					);
					rotationOffset = Math::Lerp(
						mActiveRotationAnim.startEulerDeg,
						mActiveRotationAnim.targetEulerDeg,
						easedT
					);
				} else if (elapsedSec < totalDuration) {
					const float outElapsed = elapsedSec - inSec;
					const float t = outSec > 1.0e-6f ? outElapsed / outSec : 1.0f;
					const float easedT = TweenEase::Evaluate(
						mActiveRotationAnim.ease,
						std::clamp(t, 0.0f, 1.0f)
					);
					rotationOffset = Math::Lerp(
						mActiveRotationAnim.targetEulerDeg,
						Vec3::zero,
						easedT
					);
				} else {
					rotationOffset             = Vec3::zero;
					mActiveRotationAnim.active = false;
				}
				mActiveRotationAnim.elapsedSec = elapsedSec;
			}
		} else {
			rotationOffset = Vec3::zero;
		}

		mCurrentLookOffset = Vec3(shakeOffset.x, shakeOffset.y, 0.0f) +
		                     rotationOffset;
		mCurrentRotationOffsetDeg = rotationOffset;
		mCurrentFovOffset         = fovOffset;
		ApplyOutputs(mCurrentLookOffset, mCurrentFovOffset);
	}

	BaseComponent::TICK_GROUP
	CameraFxControllerComponent::GetTickGroup() const {
		return TICK_GROUP::LATE;
	}

	void CameraFxControllerComponent::TriggerShake(
		const std::string_view presetId, const float intensityScale
	) {
		const ShakePreset* preset = FindShakePreset(presetId);
		if (!preset || intensityScale <= 0.0f) {
			return;
		}

		ActiveShake shake       = {};
		shake.amplitudeDeg      = preset->ampPitchYawDeg * intensityScale;
		shake.frequencyHz       = std::max(0.01f, preset->freqHz);
		shake.durationSec       = std::max(0.01f, preset->durationSec);
		shake.decay             = std::max(0.01f, preset->decay);
		const uint32_t seedBase = std::max(1u, mNextShakeSeed++);
		shake.seedPitch         = seedBase * 0x9e3779b9u + 0x85ebca6bu;
		shake.seedYaw           = seedBase * 0xc2b2ae35u + 0x27d4eb2fu;
		shake.noiseTimeSec      = static_cast<float>(seedBase) * 0.173f;
		mActiveShakes.emplace_back(shake);
	}

	void CameraFxControllerComponent::TriggerFov(
		const std::string_view presetId, const float intensityScale
	) {
		const FovPreset* preset = FindFovPreset(presetId);
		if (!preset || intensityScale <= 0.0f) {
			return;
		}

		mCamera = ResolveCamera();
		if (mCamera) {
			mBaseFovYDegrees = mCamera->GetFovYDegrees() - mCurrentFovOffset;
		}

		mActiveFovAnim.targetDeltaDeg = preset->targetDeltaDeg * intensityScale;
		mActiveFovAnim.lerpSpeed      = std::max(0.0f, preset->lerpSpeed);
	}

	void CameraFxControllerComponent::TriggerRotation(
		const std::string_view presetId, const float intensityScale
	) {
		const RotationPreset* preset = FindRotationPreset(presetId);
		if (!preset || intensityScale <= 0.0f) {
			return;
		}

		mActiveRotationAnim.active         = true;
		mActiveRotationAnim.elapsedSec     = 0.0f;
		mActiveRotationAnim.startEulerDeg  = mCurrentRotationOffsetDeg;
		mActiveRotationAnim.targetEulerDeg = preset->eulerDeg * intensityScale;
		mActiveRotationAnim.inSec          = std::max(0.0f, preset->inSec);
		mActiveRotationAnim.outSec         = std::max(0.0f, preset->outSec);
		mActiveRotationAnim.ease           = preset->ease;
	}

	bool CameraFxControllerComponent::HasShakePreset(
		const std::string_view presetId
	) const {
		return FindShakePreset(presetId) != nullptr;
	}

	bool CameraFxControllerComponent::HasFovPreset(
		const std::string_view presetId
	) const {
		return FindFovPreset(presetId) != nullptr;
	}

	bool CameraFxControllerComponent::HasRotationPreset(
		const std::string_view presetId
	) const {
		return FindRotationPreset(presetId) != nullptr;
	}

	bool CameraFxControllerComponent::GetFovPresetInSec(
		const std::string_view presetId, float& outInSec
	) const {
		const FovPreset* preset = FindFovPreset(presetId);
		if (!preset) {
			return false;
		}
		outInSec = 0.0f;
		return true;
	}

	std::string_view CameraFxControllerComponent::GetStableName() const {
		return "game.CameraFxController";
	}

	std::string_view CameraFxControllerComponent::GetComponentName() const {
		return "CameraFxController";
	}

	uint32_t CameraFxControllerComponent::GetIcon() const {
		return kIconVideoCam;
	}

#ifdef _DEBUG
	void CameraFxControllerComponent::DrawInspectorImGui() {
		mCamera         = ResolveCamera();
		mShakeTransform = ResolveShakeTransform();

		Entity* owner = GetOwner();
		ImGui::Text(
			"Owner GUID: %llu",
			static_cast<unsigned long long>(owner ? owner->GetGuid() : 0)
		);
		if (EditEntityGuidField("Camera Entity GUID", mCameraEntityGuid)) {
			mCamera = ResolveCamera();
		}
		if (EditEntityGuidField("Shake Entity GUID", mShakeEntityGuid)) {
			ResetOutputs();
			mShakeTransform = ResolveShakeTransform();
		}
		ImGui::TextUnformatted("GUID=0 means owner entity.");
		if (ImGui::Button("Use Owner As Targets")) {
			mCameraEntityGuid = 0;
			mShakeEntityGuid  = 0;
			ResetOutputs();
			mCamera         = ResolveCamera();
			mShakeTransform = ResolveShakeTransform();
		}

		ImGui::Text("Camera: %s", mCamera ? "Connected" : "Missing");
		ImGui::Text(
			"Shake Transform: %s", mShakeTransform ? "Connected" : "Missing"
		);
		ImGui::Text("Base FOV Y: %.3f", mBaseFovYDegrees);
		ImGui::Text("Current FOV Offset: %.3f", mCurrentFovOffset);
		ImGui::Text(
			"Current Look Offset: (%.3f, %.3f, %.3f)",
			mCurrentLookOffset.x,
			mCurrentLookOffset.y,
			mCurrentLookOffset.z
		);
		ImGui::Text(
			"Current Rotation Offset: (%.3f, %.3f, %.3f)",
			mCurrentRotationOffsetDeg.x,
			mCurrentRotationOffsetDeg.y,
			mCurrentRotationOffsetDeg.z
		);
		ImGui::Text(
			"Active Shakes: %d", static_cast<int>(mActiveShakes.size())
		);
		ImGui::Text("FOV Target Offset: %.3f", mActiveFovAnim.targetDeltaDeg);
		ImGui::Text("FOV Lerp Speed: %.3f", mActiveFovAnim.lerpSpeed);
		ImGui::Text(
			"Rotation Anim Active: %s",
			mActiveRotationAnim.active ? "Yes" : "No"
		);

		if (ImGui::Button("Capture Base FOV")) {
			if (mCamera) {
				mBaseFovYDegrees =
					mCamera->GetFovYDegrees() - mCurrentFovOffset;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset FX Outputs")) {
			mActiveShakes.clear();
			mActiveFovAnim      = {};
			mActiveRotationAnim = {};
			ResetOutputs();
		}

		(void)ImGui::DragFloat(
			"Trigger Intensity", &mDebugTriggerIntensity, 0.01f, 0.0f, 8.0f,
			"%.2f"
		);

		ImGui::SeparatorText("Shake Presets");
		size_t removeShake = mShakePresets.size();
		for (size_t i = 0; i < mShakePresets.size(); ++i) {
			ShakePreset& preset = mShakePresets[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::SeparatorText(("ShakePreset " + std::to_string(i)).c_str());

			(void)EditStringField("ID", preset.id);
			(void)ImGui::DragFloat2(
				"Amp PitchYaw Deg",
				&preset.ampPitchYawDeg.x,
				0.01f,
				-180.0f,
				180.0f,
				"%.2f"
			);
			if (ImGui::DragFloat(
				"Freq Hz", &preset.freqHz, 0.1f, 0.01f, 200.0f, "%.2f"
			)) {
				preset.freqHz = std::max(0.01f, preset.freqHz);
			}
			if (ImGui::DragFloat(
				"Duration Sec", &preset.durationSec, 0.01f, 0.01f, 20.0f, "%.2f"
			)) {
				preset.durationSec = std::max(0.01f, preset.durationSec);
			}
			if (ImGui::DragFloat(
				"Decay", &preset.decay, 0.01f, 0.01f, 8.0f, "%.2f"
			)) {
				preset.decay = std::max(0.01f, preset.decay);
			}
			if (ImGui::Button("Trigger Shake")) {
				TriggerShake(preset.id, std::max(0.0f, mDebugTriggerIntensity));
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove")) {
				removeShake = i;
			}
			ImGui::PopID();
		}
		if (removeShake < mShakePresets.size()) {
			mShakePresets.erase(
				mShakePresets.begin() + static_cast<std::ptrdiff_t>(removeShake)
			);
		}
		if (ImGui::Button("Add Shake Preset")) {
			ShakePreset preset = {};
			preset.id          = "new_shake";
			mShakePresets.emplace_back(std::move(preset));
		}

		ImGui::SeparatorText("FOV Presets");
		size_t removeFov = mFovPresets.size();
		for (size_t i = 0; i < mFovPresets.size(); ++i) {
			FovPreset& preset = mFovPresets[i];
			ImGui::PushID(100000 + static_cast<int>(i));
			ImGui::SeparatorText(("FovPreset " + std::to_string(i)).c_str());

			(void)EditStringField("ID", preset.id);
			(void)ImGui::DragFloat(
				"Target Delta Deg",
				&preset.targetDeltaDeg,
				0.05f,
				-120.0f,
				120.0f,
				"%.2f"
			);
			if (ImGui::DragFloat(
				"Lerp Speed", &preset.lerpSpeed, 0.05f, 0.0f, 200.0f, "%.2f"
			)) {
				preset.lerpSpeed = std::max(0.0f, preset.lerpSpeed);
			}

			if (ImGui::Button("Trigger FOV")) {
				TriggerFov(preset.id, std::max(0.0f, mDebugTriggerIntensity));
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove")) {
				removeFov = i;
			}
			ImGui::PopID();
		}
		if (removeFov < mFovPresets.size()) {
			mFovPresets.erase(
				mFovPresets.begin() + static_cast<std::ptrdiff_t>(removeFov)
			);
		}
		if (ImGui::Button("Add FOV Preset")) {
			FovPreset preset = {};
			preset.id        = "new_fov";
			mFovPresets.emplace_back(std::move(preset));
		}

		ImGui::SeparatorText("Rotation Presets");
		size_t removeRotation = mRotationPresets.size();
		for (size_t i = 0; i < mRotationPresets.size(); ++i) {
			RotationPreset& preset = mRotationPresets[i];
			ImGui::PushID(200000 + static_cast<int>(i));
			ImGui::SeparatorText(
				("RotationPreset " + std::to_string(i)).c_str()
			);

			(void)EditStringField("ID", preset.id);
			(void)ImGui::DragFloat3(
				"Euler Deg",
				&preset.eulerDeg.x,
				0.05f,
				-180.0f,
				180.0f,
				"%.2f"
			);
			if (ImGui::DragFloat(
				"In Sec", &preset.inSec, 0.01f, 0.0f, 20.0f, "%.2f"
			)) {
				preset.inSec = std::max(0.0f, preset.inSec);
			}
			if (ImGui::DragFloat(
				"Out Sec", &preset.outSec, 0.01f, 0.0f, 20.0f, "%.2f"
			)) {
				preset.outSec = std::max(0.0f, preset.outSec);
			}

			int currentEaseIndex = 0;
			for (size_t easeIndex = 0; easeIndex < std::size(kEaseNames); ++
			     easeIndex) {
				if (kEaseNames[easeIndex].type == preset.ease) {
					currentEaseIndex = static_cast<int>(easeIndex);
					break;
				}
			}
			std::array<const char*, std::size(kEaseNames)> easeNames = {};
			for (size_t easeIndex = 0; easeIndex < std::size(kEaseNames); ++
			     easeIndex) {
				easeNames[easeIndex] = kEaseNames[easeIndex].name.data();
			}
			if (ImGui::Combo(
				"Ease",
				&currentEaseIndex,
				easeNames.data(),
				static_cast<int>(easeNames.size())
			)) {
				preset.ease = kEaseNames[currentEaseIndex].type;
			}

			if (ImGui::Button("Trigger Rotation")) {
				TriggerRotation(
					preset.id, std::max(0.0f, mDebugTriggerIntensity)
				);
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove")) {
				removeRotation = i;
			}
			ImGui::PopID();
		}
		if (removeRotation < mRotationPresets.size()) {
			mRotationPresets.erase(
				mRotationPresets.begin() +
				static_cast<std::ptrdiff_t>(removeRotation)
			);
		}
		if (ImGui::Button("Add Rotation Preset")) {
			RotationPreset preset = {};
			preset.id             = "new_rotation";
			mRotationPresets.emplace_back(std::move(preset));
		}
	}
#endif

	void CameraFxControllerComponent::Deserialize(const JsonReader& reader) {
		mShakePresets.clear();
		mFovPresets.clear();
		mRotationPresets.clear();
		mCameraEntityGuid = 0;
		mShakeEntityGuid  = 0;

		if (reader.Has("cameraEntityGuid")) {
			mCameraEntityGuid = reader["cameraEntityGuid"].GetUint64();
		}
		if (reader.Has("shakeEntityGuid")) {
			mShakeEntityGuid = reader["shakeEntityGuid"].GetUint64();
		} else if (reader.Has("rotatorEntityGuid")) {
			// Backward compatibility for older scene data.
			mShakeEntityGuid = reader["rotatorEntityGuid"].GetUint64();
		}

		const JsonReader shakePresets = reader["shakePresets"];
		if (shakePresets.Valid() && shakePresets.IsArray()) {
			for (size_t i = 0; i < shakePresets.Size(); ++i) {
				const JsonReader node = shakePresets[i];
				if (!node.Valid()) {
					continue;
				}

				ShakePreset preset = {};
				if (node.Has("id")) {
					preset.id = node["id"].GetString();
				}
				if (preset.id.empty()) {
					continue;
				}
				if (node.Has("ampPitchYawDeg")) {
					preset.ampPitchYawDeg = node["ampPitchYawDeg"].GetVec2(
						preset.ampPitchYawDeg
					);
				}
				if (node.Has("freqHz")) {
					preset.freqHz = node["freqHz"].GetFloat(preset.freqHz);
				}
				if (node.Has("durationSec")) {
					preset.durationSec = node["durationSec"].GetFloat(
						preset.durationSec
					);
				}
				if (node.Has("decay")) {
					preset.decay = node["decay"].GetFloat(preset.decay);
				}

				preset.freqHz      = std::max(0.01f, preset.freqHz);
				preset.durationSec = std::max(0.01f, preset.durationSec);
				preset.decay       = std::max(0.01f, preset.decay);
				mShakePresets.emplace_back(std::move(preset));
			}
		}

		const JsonReader fovPresets = reader["fovPresets"];
		if (fovPresets.Valid() && fovPresets.IsArray()) {
			for (size_t i = 0; i < fovPresets.Size(); ++i) {
				const JsonReader node = fovPresets[i];
				if (!node.Valid()) {
					continue;
				}

				FovPreset preset = {};
				if (node.Has("id")) {
					preset.id = node["id"].GetString();
				}
				if (preset.id.empty()) {
					continue;
				}
				if (node.Has("targetDeltaDeg")) {
					preset.targetDeltaDeg = node["targetDeltaDeg"].GetFloat(
						preset.targetDeltaDeg
					);
				} else if (node.Has("deltaDeg")) {
					preset.targetDeltaDeg = node["deltaDeg"].GetFloat(
						preset.targetDeltaDeg
					);
				}
				if (node.Has("lerpSpeed")) {
					preset.lerpSpeed = node["lerpSpeed"].GetFloat(
						preset.lerpSpeed
					);
				} else {
					const float legacyInSec = node.Has("inSec") ?
						                          node["inSec"].GetFloat(0.0f) :
						                          0.0f;
					const float legacyOutSec = node.Has("outSec") ?
						                           node["outSec"].GetFloat(
							                           0.0f
						                           ) :
						                           0.0f;
					const float legacyDuration = legacyInSec > 0.0f ?
						                             legacyInSec :
						                             legacyOutSec;
					preset.lerpSpeed = ResolveLerpSpeed(legacyDuration);
				}

				preset.lerpSpeed = std::max(0.0f, preset.lerpSpeed);
				mFovPresets.emplace_back(std::move(preset));
			}
		}

		const JsonReader rotationPresets = reader["rotationPresets"];
		if (rotationPresets.Valid() && rotationPresets.IsArray()) {
			for (size_t i = 0; i < rotationPresets.Size(); ++i) {
				const JsonReader node = rotationPresets[i];
				if (!node.Valid()) {
					continue;
				}

				RotationPreset preset = {};
				if (node.Has("id")) {
					preset.id = node["id"].GetString();
				}
				if (preset.id.empty()) {
					continue;
				}
				if (node.Has("eulerDeg")) {
					preset.eulerDeg = node["eulerDeg"].GetVec3(preset.eulerDeg);
				}
				if (node.Has("inSec")) {
					preset.inSec = node["inSec"].GetFloat(preset.inSec);
				}
				if (node.Has("outSec")) {
					preset.outSec = node["outSec"].GetFloat(preset.outSec);
				}
				if (node.Has("ease")) {
					preset.ease = ParseEase(node["ease"]);
				}

				preset.inSec  = std::max(0.0f, preset.inSec);
				preset.outSec = std::max(0.0f, preset.outSec);
				mRotationPresets.emplace_back(std::move(preset));
			}
		}
	}

	void CameraFxControllerComponent::Serialize(JsonWriter& writer) const {
		writer.Key("cameraEntityGuid");
		writer.Write(mCameraEntityGuid);
		writer.Key("shakeEntityGuid");
		writer.Write(mShakeEntityGuid);

		writer.Key("shakePresets");
		writer.BeginArray();
		for (const ShakePreset& preset : mShakePresets) {
			writer.BeginObject();
			writer.Key("id");
			writer.Write(preset.id);
			writer.Key("ampPitchYawDeg");
			writer.BeginArray();
			writer.Write(preset.ampPitchYawDeg.x);
			writer.Write(preset.ampPitchYawDeg.y);
			writer.EndArray();
			writer.Key("freqHz");
			writer.Write(preset.freqHz);
			writer.Key("durationSec");
			writer.Write(preset.durationSec);
			writer.Key("decay");
			writer.Write(preset.decay);
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("fovPresets");
		writer.BeginArray();
		for (const FovPreset& preset : mFovPresets) {
			writer.BeginObject();
			writer.Key("id");
			writer.Write(preset.id);
			writer.Key("targetDeltaDeg");
			writer.Write(preset.targetDeltaDeg);
			writer.Key("lerpSpeed");
			writer.Write(preset.lerpSpeed);
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("rotationPresets");
		writer.BeginArray();
		for (const RotationPreset& preset : mRotationPresets) {
			writer.BeginObject();
			writer.Key("id");
			writer.Write(preset.id);
			writer.Key("eulerDeg");
			writer.BeginArray();
			writer.Write(preset.eulerDeg.x);
			writer.Write(preset.eulerDeg.y);
			writer.Write(preset.eulerDeg.z);
			writer.EndArray();
			writer.Key("inSec");
			writer.Write(preset.inSec);
			writer.Key("outSec");
			writer.Write(preset.outSec);
			writer.Key("ease");
			writer.Write(std::string(EaseToString(preset.ease)));
			writer.EndObject();
		}
		writer.EndArray();
	}

	CameraComponent* CameraFxControllerComponent::ResolveCamera() const {
		Entity* target = nullptr;
		if (mCameraEntityGuid != 0) {
			World* world = GetWorld();
			Scene* scene = world ? world->GetScenePtr() : nullptr;
			target = scene ? scene->FindEntity(mCameraEntityGuid) : nullptr;
		}
		if (!target) {
			target = GetOwner();
		}
		return target ? target->GetComponent<CameraComponent>() : nullptr;
	}

	TransformComponent*
	CameraFxControllerComponent::ResolveShakeTransform() const {
		Entity* target = nullptr;
		if (mShakeEntityGuid != 0) {
			World* world = GetWorld();
			Scene* scene = world ? world->GetScenePtr() : nullptr;
			target = scene ? scene->FindEntity(mShakeEntityGuid) : nullptr;
		}
		if (!target) {
			target = GetOwner();
		}
		return target ? target->GetComponent<TransformComponent>() : nullptr;
	}

	const CameraFxControllerComponent::ShakePreset*
	CameraFxControllerComponent::FindShakePreset(
		const std::string_view id
	) const {
		for (const ShakePreset& preset : mShakePresets) {
			if (preset.id == id) {
				return &preset;
			}
		}
		return nullptr;
	}

	const CameraFxControllerComponent::FovPreset*
	CameraFxControllerComponent::FindFovPreset(
		const std::string_view id
	) const {
		for (const FovPreset& preset : mFovPresets) {
			if (preset.id == id) {
				return &preset;
			}
		}
		return nullptr;
	}

	const CameraFxControllerComponent::RotationPreset*
	CameraFxControllerComponent::FindRotationPreset(
		const std::string_view id
	) const {
		for (const RotationPreset& preset : mRotationPresets) {
			if (preset.id == id) {
				return &preset;
			}
		}
		return nullptr;
	}

	EASE_TYPE CameraFxControllerComponent::ParseEase(const JsonReader& reader) {
		if (!reader.Valid()) {
			return EASE_TYPE::LINEAR;
		}

		const std::string text = reader.GetString();
		if (!text.empty()) {
			for (const EaseNamePair& pair : kEaseNames) {
				if (pair.name == text) {
					return pair.type;
				}
			}
		}

		const int idx = reader.GetInt();
		if (idx < 0 || std::cmp_greater_equal(idx, std::size(kEaseNames))) {
			return EASE_TYPE::LINEAR;
		}
		return kEaseNames[idx].type;
	}

	std::string_view CameraFxControllerComponent::EaseToString(
		const EASE_TYPE ease
	) {
		for (const auto& [name, type] : kEaseNames) {
			if (type == ease) {
				return name;
			}
		}
		return "LINEAR";
	}

	void CameraFxControllerComponent::ApplyOutputs(
		const Vec3& lookOffsetDeg, const float fovOffsetDeg
	) {
		mShakeTransform = ResolveShakeTransform();
		if (mShakeTransform) {
			const Quaternion offsetRotation = Quaternion::EulerDegrees(
				lookOffsetDeg
			);
			const Quaternion deltaRotation =
				mLastAppliedShakeRotation.Inverse() * offsetRotation;
			mShakeTransform->SetRotation(
				mShakeTransform->GetRotation() * deltaRotation
			);
			mLastAppliedShakeRotation = offsetRotation;
		} else {
			mLastAppliedShakeRotation = Quaternion::identity;
		}

		mCamera = ResolveCamera();
		if (mCamera) {
			mCamera->SetFovYDegrees(mBaseFovYDegrees + fovOffsetDeg);
		}
	}

	void CameraFxControllerComponent::ResetOutputs() {
		mCurrentLookOffset        = Vec3::zero;
		mCurrentRotationOffsetDeg = Vec3::zero;
		mCurrentFovOffset         = 0.0f;
		ApplyOutputs(Vec3::zero, 0.0f);
	}

	REGISTER_COMPONENT(CameraFxControllerComponent);
}
