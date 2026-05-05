#include "AudioFxControllerComponent.h"

#include <algorithm>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/random/Random.h"

#include "engine/ImGui/Icons.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/audio/AudioSourceComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
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
#endif
	}

	void AudioFxControllerComponent::OnAttached() {}

	void AudioFxControllerComponent::OnTick(const float) {}

	bool AudioFxControllerComponent::TriggerOneShot(
		const std::string_view presetId, const float intensityScale
	) const {
		const OneShotPreset* preset = FindPreset(presetId);
		if (!preset) {
			return false;
		}

		AudioSourceComponent* source = ResolveSource(*preset);
		if (!source) {
			return false;
		}

		const float safeIntensity = std::max(0.0f, intensityScale);
		const float pitchMin      = std::clamp(preset->pitchMin, 0.01f, 4.0f);
		const float pitchMax      = std::clamp(
			std::max(preset->pitchMin, preset->pitchMax), 0.01f, 4.0f
		);
		const float randomizedPitch = Random::FloatRange(pitchMin, pitchMax);
		source->SetLoop(false);
		source->SetPitch(randomizedPitch);
		source->SetVolume(
			std::clamp(preset->volume * safeIntensity, 0.0f, 4.0f)
		);
		source->Play();
		return true;
	}

	bool AudioFxControllerComponent::HasPreset(
		const std::string_view presetId
	)
	const {
		return FindPreset(presetId) != nullptr;
	}

	std::string_view AudioFxControllerComponent::GetStableName() const {
		return "game.AudioFxController";
	}

	std::string_view AudioFxControllerComponent::GetComponentName() const {
		return "AudioFxController";
	}

	uint32_t AudioFxControllerComponent::GetIcon() const {
		return kIconSpeaker;
	}

#ifdef _DEBUG
	void AudioFxControllerComponent::DrawInspectorImGui() {
		ImGui::Text(
			"OneShot Presets: %d",
			static_cast<int>(mOneShotPresets.size())
		);

		size_t removeIndex = static_cast<size_t>(-1);
		for (size_t i = 0; i < mOneShotPresets.size(); ++i) {
			OneShotPreset& preset = mOneShotPresets[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::SeparatorText(("OneShot " + std::to_string(i)).c_str());

			(void)EditStringField("ID", preset.id);
			(void)ImGui::InputScalar(
				"Source Entity GUID",
				ImGuiDataType_U64,
				&preset.sourceEntityGuid
			);
			(void)ImGui::InputScalar(
				"Source Component GUID",
				ImGuiDataType_U64,
				&preset.sourceComponentGuid
			);
			(void)ImGui::DragFloat(
				"Volume",
				&preset.volume,
				0.01f,
				0.0f,
				4.0f,
				"%.2f"
			);
			(void)ImGui::DragFloat(
				"Pitch Min",
				&preset.pitchMin,
				0.01f,
				0.01f,
				4.0f,
				"%.2f"
			);
			(void)ImGui::DragFloat(
				"Pitch Max",
				&preset.pitchMax,
				0.01f,
				0.01f,
				4.0f,
				"%.2f"
			);
			preset.pitchMin = std::clamp(preset.pitchMin, 0.01f, 4.0f);
			preset.pitchMax = std::clamp(preset.pitchMax, 0.01f, 4.0f);
			if (preset.pitchMin > preset.pitchMax) {
				std::swap(preset.pitchMin, preset.pitchMax);
			}

			if (ImGui::Button("Trigger")) {
				(void)TriggerOneShot(preset.id, 1.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove")) {
				removeIndex = i;
			}

			ImGui::PopID();
		}

		if (
			removeIndex != static_cast<size_t>(-1) &&
			removeIndex < mOneShotPresets.size()
		) {
			mOneShotPresets.erase(
				mOneShotPresets.begin() + static_cast<std::ptrdiff_t>(
					removeIndex)
			);
		}

		if (ImGui::Button("Add OneShot Preset")) {
			OneShotPreset preset = {};
			preset.id            = "oneshot";
			mOneShotPresets.emplace_back(std::move(preset));
		}
	}
#endif

	void AudioFxControllerComponent::Deserialize(const JsonReader& reader) {
		mOneShotPresets.clear();

		const JsonReader presetsNode = reader["oneShotPresets"];
		if (presetsNode.Valid() && presetsNode.IsArray()) {
			for (size_t i = 0; i < presetsNode.Size(); ++i) {
				const JsonReader node = presetsNode[i];
				if (!node.Valid() || !node.IsObject()) {
					continue;
				}

				OneShotPreset preset = {};
				if (node.Has("id")) {
					preset.id = node["id"].GetString("");
				}
				if (preset.id.empty()) {
					continue;
				}
				if (node.Has("sourceEntityGuid")) {
					preset.sourceEntityGuid = node["sourceEntityGuid"].
						GetUint64();
				}
				if (node.Has("sourceComponentGuid")) {
					preset.sourceComponentGuid =
						node["sourceComponentGuid"].GetUint64();
				}
				if (node.Has("volume")) {
					preset.volume = node["volume"].GetFloat(preset.volume);
				}
				const bool hasPitchRange =
					node.Has("pitchMin") || node.Has("pitchMax");
				if (hasPitchRange) {
					if (node.Has("pitchMin")) {
						preset.pitchMin = node["pitchMin"].GetFloat(
							preset.pitchMin
						);
					}
					if (node.Has("pitchMax")) {
						preset.pitchMax = node["pitchMax"].GetFloat(
							preset.pitchMax
						);
					}
				} else if (node.Has("pitch")) {
					const float legacyPitch = node["pitch"].GetFloat(1.0f);
					preset.pitchMin         = legacyPitch;
					preset.pitchMax         = legacyPitch;
				}
				preset.volume   = std::clamp(preset.volume, 0.0f, 4.0f);
				preset.pitchMin = std::clamp(preset.pitchMin, 0.01f, 4.0f);
				preset.pitchMax = std::clamp(preset.pitchMax, 0.01f, 4.0f);
				if (preset.pitchMin > preset.pitchMax) {
					std::swap(preset.pitchMin, preset.pitchMax);
				}
				mOneShotPresets.emplace_back(std::move(preset));
			}
		}
	}

	void AudioFxControllerComponent::Serialize(JsonWriter& writer) const {
		writer.Key("oneShotPresets");
		writer.BeginArray();
		for (const OneShotPreset& preset : mOneShotPresets) {
			writer.BeginObject();
			writer.Key("id");
			writer.Write(preset.id);
			writer.Key("sourceEntityGuid");
			writer.Write(preset.sourceEntityGuid);
			writer.Key("sourceComponentGuid");
			writer.Write(preset.sourceComponentGuid);
			writer.Key("volume");
			writer.Write(preset.volume);
			writer.Key("pitchMin");
			writer.Write(preset.pitchMin);
			writer.Key("pitchMax");
			writer.Write(preset.pitchMax);
			writer.EndObject();
		}
		writer.EndArray();
	}

	const AudioFxControllerComponent::OneShotPreset*
	AudioFxControllerComponent::FindPreset(const std::string_view id) const {
		for (const OneShotPreset& preset : mOneShotPresets) {
			if (preset.id == id) {
				return &preset;
			}
		}
		return nullptr;
	}

	AudioSourceComponent* AudioFxControllerComponent::ResolveSource(
		const OneShotPreset& preset
	) const {
		World* world = GetWorld();
		Scene* scene = world ? world->GetScenePtr() : nullptr;

		if (scene && preset.sourceComponentGuid != 0) {
			for (const auto& entityPtr : scene->GetEntities()) {
				Entity* entity = entityPtr.get();
				if (!entity) {
					continue;
				}

				AudioSourceComponent* found = nullptr;
				(void)entity->ForEachComponent(
					[&](BaseComponent& component) {
						if (component.GetGuid() != preset.sourceComponentGuid) {
							return true;
						}
						found = dynamic_cast<AudioSourceComponent*>(&component);
						return false;
					}
				);
				if (found) {
					return found;
				}
			}
		}

		Entity* target = nullptr;
		if (scene && preset.sourceEntityGuid != 0) {
			target = scene->FindEntity(preset.sourceEntityGuid);
		}
		if (!target) {
			target = GetOwner();
		}
		return target ? target->GetComponent<AudioSourceComponent>() : nullptr;
	}

	REGISTER_COMPONENT(AudioFxControllerComponent);
}
