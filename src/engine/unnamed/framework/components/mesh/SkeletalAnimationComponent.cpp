#include "SkeletalAnimationComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include "core/ComponentRegistry.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"

namespace Unnamed {
	namespace {
		struct LocalBonePose {
			Vec3       translation = Vec3::zero;
			Quaternion rotation    = Quaternion::identity;
			Vec3       scale       = Vec3::one;
		};

		struct BlendAccumVec3 {
			Vec3  weightedSum = Vec3::zero;
			float weightSum   = 0.0f;
		};

		struct BlendAccumQuat {
			float      x            = 0.0f;
			float      y            = 0.0f;
			float      z            = 0.0f;
			float      w            = 0.0f;
			float      weightSum    = 0.0f;
			bool       hasReference = false;
			Quaternion reference    = Quaternion::identity;
		};

		const AnimationClipAssetData* ResolveClip(
			const MeshAssetData& mesh, const std::string_view clipName
		) {
			if (mesh.animationClips.empty()) {
				return nullptr;
			}

			if (!clipName.empty()) {
				for (const auto& clip : mesh.animationClips) {
					if (clip.name == clipName) {
						return &clip;
					}
				}
			}
			return &mesh.animationClips.front();
		}

		float ToSampleTime(
			const float playbackTime, const float duration, const bool loop
		) {
			if (duration <= 0.0f) {
				return 0.0f;
			}
			if (!loop) {
				return std::clamp(playbackTime, 0.0f, duration);
			}
			const float wrapped = std::fmod(
				std::max(playbackTime, 0.0f), duration
			);
			return wrapped < 0.0f ? wrapped + duration : wrapped;
		}

		Vec3 SampleVec3Track(
			const std::vector<AnimationKeyVec3AssetData>& keys,
			const float                                   sampleTime,
			const Vec3&                                   fallback
		) {
			if (keys.empty()) {
				return fallback;
			}
			if (keys.size() == 1 || sampleTime <= keys.front().timeSeconds) {
				return keys.front().value;
			}

			for (size_t i = 0; i + 1 < keys.size(); ++i) {
				const auto& lhs = keys[i];
				const auto& rhs = keys[i + 1];
				if (sampleTime > rhs.timeSeconds) {
					continue;
				}

				const float span = rhs.timeSeconds - lhs.timeSeconds;
				if (span <= 1e-6f) {
					return rhs.value;
				}
				const float t = (sampleTime - lhs.timeSeconds) / span;
				return lhs.value + (rhs.value - lhs.value) * t;
			}

			return keys.back().value;
		}

		Quaternion SampleQuatTrack(
			const std::vector<AnimationKeyQuatAssetData>& keys,
			const float                                   sampleTime,
			const Quaternion&                             fallback
		) {
			if (keys.empty()) {
				return fallback;
			}
			if (keys.size() == 1 || sampleTime <= keys.front().timeSeconds) {
				return keys.front().value;
			}

			for (size_t i = 0; i + 1 < keys.size(); ++i) {
				const auto& lhs = keys[i];
				const auto& rhs = keys[i + 1];
				if (sampleTime > rhs.timeSeconds) {
					continue;
				}

				const float span = rhs.timeSeconds - lhs.timeSeconds;
				if (span <= 1e-6f) {
					return rhs.value;
				}
				const float t = (sampleTime - lhs.timeSeconds) / span;
				return Quaternion::Slerp(lhs.value, rhs.value, t);
			}

			return keys.back().value;
		}

		void AddWeightedQuaternion(
			BlendAccumQuat& accum, Quaternion q, const float weight
		) {
			if (weight <= 0.0f) {
				return;
			}

			q = q.Normalized();
			if (!accum.hasReference) {
				accum.reference    = q;
				accum.hasReference = true;
			}

			const float dot =
				accum.reference.x * q.x +
				accum.reference.y * q.y +
				accum.reference.z * q.z +
				accum.reference.w * q.w;
			if (dot < 0.0f) {
				q.x = -q.x;
				q.y = -q.y;
				q.z = -q.z;
				q.w = -q.w;
			}

			accum.x         += q.x * weight;
			accum.y         += q.y * weight;
			accum.z         += q.z * weight;
			accum.w         += q.w * weight;
			accum.weightSum += weight;
		}

		Quaternion NormalizeWeightedQuaternion(
			const BlendAccumQuat& accum, const Quaternion& fallback
		) {
			const float lenSq =
				accum.x * accum.x +
				accum.y * accum.y +
				accum.z * accum.z +
				accum.w * accum.w;
			if (lenSq <= 1e-12f) {
				return fallback;
			}

			const float invLen = 1.0f / std::sqrt(lenSq);
			return {
				accum.x * invLen,
				accum.y * invLen,
				accum.z * invLen,
				accum.w * invLen
			};
		}

		SkeletalAnimationComponent::AnimationLayerDesc SanitizeLayerDesc(
			SkeletalAnimationComponent::AnimationLayerDesc layer
		) {
			layer.weight = std::clamp(layer.weight, 0.0f, 64.0f);
			layer.speed  = std::clamp(layer.speed, 0.0f, 8.0f);
			return layer;
		}

		SkeletalAnimationComponent::AnimationStateDesc SanitizeStateDesc(
			SkeletalAnimationComponent::AnimationStateDesc state
		) {
			state.weight   = std::clamp(state.weight, 0.0f, 64.0f);
			state.speed    = std::clamp(state.speed, 0.0f, 8.0f);
			state.blendSec = std::clamp(state.blendSec, 0.0f, 4.0f);
			return state;
		}

		std::string_view StripAnimStatePrefix(const std::string_view stateId) {
			static constexpr std::string_view kPrefix = "animstate_";
			if (stateId.starts_with(kPrefix)) {
				return stateId.substr(kPrefix.size());
			}
			return stateId;
		}

		void BuildGlobalPoseFromLocals(
			const MeshAssetData&     mesh,
			const uint32_t           boneCount,
			const std::vector<Mat4>& localPose,
			std::vector<Mat4>&       outGlobalPose
		) {
			outGlobalPose.assign(boneCount, Mat4::identity);
			if (boneCount == 0) {
				return;
			}

			std::vector<uint8_t> visitState(boneCount, 0u);
			// 0:未訪問, 1:訪問中, 2:確定
			auto ResolveBoneGlobal = [&](
				auto&& self, const uint32_t boneIndex
			)
				-> void {
				if (visitState[boneIndex] == 2u) {
					return;
				}
				if (visitState[boneIndex] == 1u) {
					// 循環参照を検出した場合はローカルをそのまま採用する
					outGlobalPose[boneIndex] = localPose[boneIndex];
					visitState[boneIndex]    = 2u;
					return;
				}

				visitState[boneIndex]     = 1u;
				const int32_t parentIndex = mesh.skeleton[boneIndex].
					parentIndex;
				if (
					parentIndex >= 0 &&
					std::cmp_less(parentIndex, boneCount) &&
					std::cmp_not_equal(parentIndex, boneIndex)
				) {
					const uint32_t parentBoneIndex = static_cast<uint32_t>(
						parentIndex
					);
					self(self, parentBoneIndex);
					outGlobalPose[boneIndex] = localPose[boneIndex] *
					                           outGlobalPose[parentBoneIndex];
				} else {
					outGlobalPose[boneIndex] = localPose[boneIndex];
				}
				visitState[boneIndex] = 2u;
			};

			for (uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
				ResolveBoneGlobal(ResolveBoneGlobal, boneIndex);
			}
		}
	}

	std::string_view SkeletalAnimationComponent::GetStableName() const {
		return "engine.SkeletalAnimation";
	}

	std::string_view SkeletalAnimationComponent::GetComponentName() const {
		return "SkeletalAnimation";
	}

	uint32_t SkeletalAnimationComponent::GetIcon() const {
		return kIconEmojiPeople;
	}

	void SkeletalAnimationComponent::OnAttached() {
		EnsureHasAtLeastOneLayer();
		for (auto& layer : mLayers) {
			layer.playbackTime        = 0.0f;
			layer.cachedClipDuration  = 0.0f;
			layer.isPlaying           = false;
			layer.playOnStartConsumed = false;
			layer.transition          = {};
		}
	}

	void SkeletalAnimationComponent::OnRenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		(void)interpolationAlpha;
		if (renderDeltaTime <= 0.0f) {
			return;
		}

		EnsureHasAtLeastOneLayer();
		for (size_t i = 0; i < mLayers.size(); ++i) {
			auto& layer = mLayers[i];
			if (layer.desc.playOnStart && !layer.playOnStartConsumed) {
				layer.isPlaying           = true;
				layer.playOnStartConsumed = true;
			}

			if (!layer.isPlaying || layer.desc.speed <= 0.0f) {
				continue;
			}

			layer.playbackTime += renderDeltaTime * layer.desc.speed;
			ClampLayerPlaybackIfPossible(i);
			if (layer.transition.active) {
				layer.transition.elapsedSec += renderDeltaTime;
				if (layer.transition.fromSpeed > 0.0f) {
					layer.transition.fromPlaybackTime +=
						renderDeltaTime * layer.transition.fromSpeed;
					layer.transition.fromPlaybackTime = std::max(
						0.0f, layer.transition.fromPlaybackTime
					);
					if (
						layer.transition.fromCachedClipDuration > 0.0f &&
						!layer.transition.fromLoop
					) {
						layer.transition.fromPlaybackTime = std::min(
							layer.transition.fromPlaybackTime,
							layer.transition.fromCachedClipDuration
						);
					}
				}
				if (
					layer.transition.durationSec <= 1.0e-6f ||
					layer.transition.elapsedSec >= layer.transition.durationSec
				) {
					layer.transition.active = false;
				}
			}
		}
	}

	void SkeletalAnimationComponent::Deserialize(const JsonReader& reader) {
		mLayers.clear();
		mStateMap.clear();

		const JsonReader layers = reader["layers"];
		if (layers.Valid() && layers.IsArray()) {
			for (size_t i = 0; i < layers.Size(); ++i) {
				const JsonReader layerNode = layers[i];
				if (!layerNode.Valid()) {
					continue;
				}

				AnimationLayerDesc desc = {};
				if (layerNode.Has("clipName")) {
					desc.clipName = layerNode["clipName"].GetString();
				}
				if (layerNode.Has("weight")) {
					desc.weight = layerNode["weight"].GetFloat();
				}
				if (layerNode.Has("speed")) {
					desc.speed = layerNode["speed"].GetFloat();
				}
				if (layerNode.Has("loop")) {
					desc.loop = layerNode["loop"].GetBool();
				}
				if (layerNode.Has("playOnStart")) {
					desc.playOnStart = layerNode["playOnStart"].GetBool();
				}
				(void)AddLayer(desc);
			}
		}

		if (mLayers.empty()) {
			AnimationLayerDesc legacy = {};
			if (reader.Has("clipName")) {
				legacy.clipName = reader["clipName"].GetString();
			}
			if (reader.Has("playOnStart")) {
				legacy.playOnStart = reader["playOnStart"].GetBool();
			}
			if (reader.Has("loop")) {
				legacy.loop = reader["loop"].GetBool();
			}
			if (reader.Has("speed")) {
				legacy.speed = reader["speed"].GetFloat();
			}
			(void)AddLayer(legacy);
		}

		const JsonReader stateMap = reader["stateMap"];
		if (stateMap.Valid() && stateMap.IsArray()) {
			for (size_t i = 0; i < stateMap.Size(); ++i) {
				const JsonReader stateNode = stateMap[i];
				if (!stateNode.Valid()) {
					continue;
				}

				AnimationStateDesc state = {};
				if (stateNode.Has("stateId")) {
					state.stateId = StrUtil::TrimSpaces(
						stateNode["stateId"].GetString()
					);
				}
				if (state.stateId.empty()) {
					continue;
				}
				if (stateNode.Has("layerIndex")) {
					state.layerIndex = static_cast<size_t>(
						std::max(0, stateNode["layerIndex"].GetInt())
					);
				}
				if (stateNode.Has("clipName")) {
					state.clipName = StrUtil::TrimSpaces(
						stateNode["clipName"].GetString()
					);
				}
				if (stateNode.Has("weight")) {
					state.weight = stateNode["weight"].GetFloat();
				}
				if (stateNode.Has("speed")) {
					state.speed = stateNode["speed"].GetFloat();
				}
				if (stateNode.Has("blendSec")) {
					state.blendSec = stateNode["blendSec"].GetFloat();
				}
				if (stateNode.Has("loop")) {
					state.loop = stateNode["loop"].GetBool();
				}
				if (stateNode.Has("restartOnPlay")) {
					state.restartOnPlay = stateNode["restartOnPlay"].GetBool();
				}
				std::string mapKey = state.stateId;
				int         suffix = 1;
				while (mStateMap.contains(mapKey)) {
					mapKey = state.stateId + "#" + std::to_string(suffix++);
				}
				mStateMap[mapKey] = SanitizeStateDesc(std::move(state));
			}
		}

		EnsureHasAtLeastOneLayer();
	}

	void SkeletalAnimationComponent::Serialize(JsonWriter& writer) const {
		const AnimationLayerDesc* first    = GetLayerDesc(0);
		const AnimationLayerDesc  fallback = {};
		const AnimationLayerDesc& primary  = first ? *first : fallback;

		writer.Key("clipName");
		writer.Write(primary.clipName);
		writer.Key("playOnStart");
		writer.Write(primary.playOnStart);
		writer.Key("loop");
		writer.Write(primary.loop);
		writer.Key("speed");
		writer.Write(primary.speed);

		writer.Key("layers");
		writer.BeginArray();
		for (const auto& layer : mLayers) {
			writer.BeginObject();
			writer.Key("clipName");
			writer.Write(layer.desc.clipName);
			writer.Key("weight");
			writer.Write(layer.desc.weight);
			writer.Key("speed");
			writer.Write(layer.desc.speed);
			writer.Key("loop");
			writer.Write(layer.desc.loop);
			writer.Key("playOnStart");
			writer.Write(layer.desc.playOnStart);
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("stateMap");
		writer.BeginArray();
		for (const auto& [stateId, state] : mStateMap) {
			const std::string serializedStateId = StrUtil::TrimSpaces(
				state.stateId.empty() ? stateId : state.stateId
			);
			writer.BeginObject();
			writer.Key("stateId");
			writer.Write(serializedStateId);
			writer.Key("layerIndex");
			writer.Write(static_cast<int>(state.layerIndex));
			writer.Key("clipName");
			writer.Write(state.clipName);
			writer.Key("weight");
			writer.Write(state.weight);
			writer.Key("speed");
			writer.Write(state.speed);
			writer.Key("blendSec");
			writer.Write(state.blendSec);
			writer.Key("loop");
			writer.Write(state.loop);
			writer.Key("restartOnPlay");
			writer.Write(state.restartOnPlay);
			writer.EndObject();
		}
		writer.EndArray();
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void SkeletalAnimationComponent::DrawInspectorImGui() {
		auto editStringField = [](
			const char*  label,
			std::string& value,
			const size_t capacity = 256
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
		};

		if (ImGui::Button("Play All")) {
			Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause All")) {
			Pause();
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop All")) {
			Stop();
		}

		size_t removeIndex = static_cast<size_t>(-1);
		for (size_t i = 0; i < mLayers.size(); ++i) {
			auto& layer = mLayers[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::SeparatorText(("Layer " + std::to_string(i)).c_str());

			std::array<char, 256> clipName = {};
			std::memcpy(
				clipName.data(),
				layer.desc.clipName.c_str(),
				std::min(layer.desc.clipName.size(), clipName.size() - 1)
			);
			if (ImGui::InputText("Clip", clipName.data(), clipName.size())) {
				layer.desc.clipName      = clipName.data();
				layer.playbackTime       = 0.0f;
				layer.cachedClipDuration = 0.0f;
			}

			if (
				ImGui::DragFloat(
					"Weight", &layer.desc.weight, 0.01f, 0.0f, 1.0f, "%.2f"
				)
			) {
				layer.desc = SanitizeLayerDesc(layer.desc);
			}
			if (
				ImGui::DragFloat(
					"Speed", &layer.desc.speed, 0.01f, 0.0f, 8.0f, "%.2f"
				)
			) {
				layer.desc = SanitizeLayerDesc(layer.desc);
			}
			ImGui::Checkbox("Loop", &layer.desc.loop);
			ImGui::Checkbox("Play On Start", &layer.desc.playOnStart);
			ImGui::Text("Time: %.3f sec", layer.playbackTime);
			ImGui::TextUnformatted(
				layer.isPlaying ? "State: Playing" : "State: Stopped"
			);

			if (ImGui::Button("Play")) {
				layer.isPlaying           = true;
				layer.playOnStartConsumed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Pause")) {
				layer.isPlaying           = false;
				layer.playOnStartConsumed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop")) {
				layer.isPlaying           = false;
				layer.playOnStartConsumed = true;
				layer.playbackTime        = 0.0f;
			}

			if (mLayers.size() > 1 && ImGui::Button("Remove Layer")) {
				removeIndex = i;
			}
			ImGui::PopID();
		}

		if (std::cmp_not_equal(removeIndex, -1)) {
			(void)RemoveLayer(removeIndex);
		}

		if (ImGui::Button("Add Layer")) {
			AnimationLayerDesc layer = {};
			layer.weight             = 0.0f;
			(void)AddLayer(layer);
		}

		ImGui::SeparatorText("State Map");
		std::vector<std::string> stateKeys;
		stateKeys.reserve(mStateMap.size());
		for (const auto& [stateId, state] : mStateMap) {
			(void)state;
			stateKeys.emplace_back(stateId);
		}
		std::ranges::sort(stateKeys);

		std::string removeStateId;

		for (size_t i = 0; i < stateKeys.size(); ++i) {
			const std::string key = stateKeys[i];
			auto              it  = mStateMap.find(key);
			if (it == mStateMap.end()) {
				continue;
			}
			AnimationStateDesc& state = it->second;
			ImGui::PushID(200000 + static_cast<int>(i));
			ImGui::SeparatorText(("State " + std::to_string(i)).c_str());

			state.stateId = StrUtil::TrimSpaces(
				state.stateId.empty() ? key : state.stateId
			);
			std::array<char, 256> stateIdBuffer = {};
			std::memcpy(
				stateIdBuffer.data(),
				state.stateId.c_str(),
				std::min(state.stateId.size(), stateIdBuffer.size() - 1)
			);
			if (ImGui::InputText(
				"State ID", stateIdBuffer.data(), stateIdBuffer.size()
			)) {
				state.stateId = StrUtil::TrimSpaces(stateIdBuffer.data());
			}
			state.stateId = StrUtil::TrimSpaces(state.stateId);

			int layerIndex = static_cast<int>(state.layerIndex);
			if (ImGui::DragInt("Layer Index", &layerIndex, 0.1f, 0, 16)) {
				state.layerIndex = static_cast<size_t>(std::max(0, layerIndex));
			}
			(void)editStringField("Clip Name", state.clipName);
			if (ImGui::DragFloat(
				"Weight", &state.weight, 0.01f, 0.0f, 64.0f, "%.2f"
			)) {
				state.weight = std::clamp(state.weight, 0.0f, 64.0f);
			}
			if (ImGui::DragFloat(
				"Speed", &state.speed, 0.01f, 0.0f, 8.0f, "%.2f"
			)) {
				state.speed = std::clamp(state.speed, 0.0f, 8.0f);
			}
			if (
				ImGui::DragFloat(
					"Blend Sec", &state.blendSec, 0.01f, 0.0f, 4.0f, "%.2f"
				)
			) {
				state.blendSec = std::clamp(state.blendSec, 0.0f, 4.0f);
			}
			(void)ImGui::Checkbox("Loop", &state.loop);
			(void)ImGui::Checkbox("Restart On Play", &state.restartOnPlay);
			state = SanitizeStateDesc(std::move(state));

			const std::string currentStateId = StrUtil::TrimSpaces(
				state.stateId.empty() ? key : state.stateId
			);
			if (currentStateId.empty()) {
				ImGui::TextUnformatted("Warning: State ID is empty.");
			} else {
				bool hasDuplicate = false;
				for (const std::string& otherKey : stateKeys) {
					if (otherKey == key) {
						continue;
					}
					auto otherIt = mStateMap.find(otherKey);
					if (otherIt == mStateMap.end()) {
						continue;
					}
					const std::string otherId = StrUtil::TrimSpaces(
						otherIt->second.stateId.empty() ?
							otherKey :
							otherIt->second.stateId
					);
					if (otherId == currentStateId) {
						hasDuplicate = true;
						break;
					}
				}
				if (hasDuplicate) {
					ImGui::TextUnformatted(
						"Warning: duplicate State ID exists."
					);
				}
			}

			if (ImGui::Button("Play State")) {
				(void)PlayState(state.stateId);
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove State")) {
				removeStateId = key;
			}

			ImGui::PopID();
		}

		if (!removeStateId.empty()) {
			(void)mStateMap.erase(removeStateId);
		}
		if (ImGui::Button("Add State")) {
			AnimationStateDesc state = {};
			state.stateId            = "new_state";
			int suffix               = 1;
			while (mStateMap.contains(state.stateId)) {
				state.stateId = "new_state_" + std::to_string(suffix++);
			}
			if (!mLayers.empty()) {
				state.clipName = mLayers.front().desc.clipName;
			}
			mStateMap[state.stateId] = std::move(state);
		}
	}

	std::vector<std::string>
	SkeletalAnimationComponent::DebugGetStateIds() const {
		std::vector<std::string> ids;
		ids.reserve(mStateMap.size());
		for (const auto& [id, state] : mStateMap) {
			const std::string normalized = StrUtil::TrimSpaces(
				state.stateId.empty() ? id : state.stateId
			);
			if (!normalized.empty()) {
				ids.emplace_back(normalized);
			}
		}
		std::ranges::sort(ids);
		return ids;
	}
#endif

	void SkeletalAnimationComponent::Play() {
		EnsureHasAtLeastOneLayer();
		for (auto& layer : mLayers) {
			layer.isPlaying           = true;
			layer.playOnStartConsumed = true;
		}
	}

	void SkeletalAnimationComponent::Pause() {
		for (auto& layer : mLayers) {
			layer.isPlaying           = false;
			layer.playOnStartConsumed = true;
		}
	}

	void SkeletalAnimationComponent::Stop() {
		for (auto& layer : mLayers) {
			layer.isPlaying           = false;
			layer.playOnStartConsumed = true;
			layer.playbackTime        = 0.0f;
			layer.transition.active   = false;
		}
	}

	bool SkeletalAnimationComponent::PlayState(const std::string_view stateId) {
		auto FindState = [&](const std::string_view id) {
			const std::string trimmedId = StrUtil::TrimSpaces(id);
			auto              it        = mStateMap.find(trimmedId);
			if (it != mStateMap.end()) {
				return it;
			}

			const std::string strippedText = StrUtil::TrimSpaces(
				StripAnimStatePrefix(trimmedId)
			);
			const std::string_view stripped = strippedText;
			if (stripped != trimmedId) {
				it = mStateMap.find(strippedText);
				if (it != mStateMap.end()) {
					return it;
				}
			}

			const std::string lowered = StrUtil::ToLowerCase(stripped);
			for (auto jt = mStateMap.begin(); jt != mStateMap.end(); ++jt) {
				const std::string keyNormalized = StrUtil::ToLowerCase(
					StrUtil::TrimSpaces(jt->first)
				);
				const std::string valueNormalized = StrUtil::ToLowerCase(
					StrUtil::TrimSpaces(jt->second.stateId)
				);
				if (keyNormalized == lowered || valueNormalized == lowered) {
					return jt;
				}
			}
			return mStateMap.end();
		};

		const auto it = FindState(stateId);
		if (it == mStateMap.end()) {
			return false;
		}

		const AnimationStateDesc& state = it->second;
		while (mLayers.size() <= state.layerIndex) {
			AnimationLayerDesc extra = {};
			extra.weight             = 0.0f;
			(void)AddLayer(extra);
		}

		auto& layer = mLayers[state.layerIndex];
		const AnimationLayerDesc previousLayerDesc = layer.desc;
		const float previousPlaybackTime = layer.playbackTime;
		const float previousCachedDuration = layer.cachedClipDuration;
		const bool clipChanged = layer.desc.clipName != state.clipName;

		layer.desc.clipName = state.clipName;
		layer.desc.weight   = std::clamp(state.weight, 0.0f, 64.0f);
		layer.desc.speed    = std::clamp(state.speed, 0.0f, 8.0f);
		layer.desc.loop     = state.loop;
		layer.desc          = SanitizeLayerDesc(layer.desc);

		if (clipChanged || state.restartOnPlay) {
			layer.playbackTime       = 0.0f;
			layer.cachedClipDuration = 0.0f;
		}

		if (
			clipChanged &&
			state.blendSec > 0.0f &&
			!previousLayerDesc.clipName.empty()
		) {
			layer.transition.fromClipName     = previousLayerDesc.clipName;
			layer.transition.fromPlaybackTime = std::max(
				0.0f, previousPlaybackTime
			);
			layer.transition.fromCachedClipDuration = std::max(
				0.0f, previousCachedDuration
			);
			layer.transition.fromSpeed = std::clamp(
				previousLayerDesc.speed, 0.0f, 8.0f
			);
			layer.transition.fromLoop    = previousLayerDesc.loop;
			layer.transition.durationSec = state.blendSec;
			layer.transition.elapsedSec  = 0.0f;
			layer.transition.active      = true;
		} else {
			layer.transition.active = false;
		}

		layer.isPlaying           = true;
		layer.playOnStartConsumed = true;
		return true;
	}

	bool SkeletalAnimationComponent::HasState(
		const std::string_view stateId
	)
	const {
		const std::string trimmedId = StrUtil::TrimSpaces(stateId);
		if (mStateMap.contains(trimmedId)) {
			return true;
		}

		const std::string strippedText = StrUtil::TrimSpaces(
			StripAnimStatePrefix(trimmedId)
		);
		const std::string_view stripped = strippedText;
		if (stripped != trimmedId && mStateMap.contains(strippedText)) {
			return true;
		}

		const std::string lowered = StrUtil::ToLowerCase(stripped);
		return std::ranges::any_of(
			mStateMap,
			[&](const auto& entry) {
				const auto& [id, state] = entry;
				return
					StrUtil::ToLowerCase(StrUtil::TrimSpaces(id)) == lowered ||
					StrUtil::ToLowerCase(StrUtil::TrimSpaces(state.stateId)) ==
						lowered;
			}
		);
	}

	void SkeletalAnimationComponent::SetClipName(const std::string& clipName) {
		EnsureHasAtLeastOneLayer();
		mLayers.front().desc.clipName      = clipName;
		mLayers.front().playbackTime       = 0.0f;
		mLayers.front().cachedClipDuration = 0.0f;
		mLayers.front().transition.active  = false;
	}

	std::string_view SkeletalAnimationComponent::GetClipName() const noexcept {
		if (mLayers.empty()) {
			return {};
		}
		return mLayers.front().desc.clipName;
	}

	void SkeletalAnimationComponent::SetLoop(const bool enabled) {
		EnsureHasAtLeastOneLayer();
		mLayers.front().desc.loop = enabled;
		ClampLayerPlaybackIfPossible(0);
	}

	bool SkeletalAnimationComponent::GetLoop() const noexcept {
		return !mLayers.empty() && mLayers.front().desc.loop;
	}

	void SkeletalAnimationComponent::SetPlayOnStart(const bool enabled) {
		EnsureHasAtLeastOneLayer();
		mLayers.front().desc.playOnStart = enabled;
		if (enabled && !mLayers.front().isPlaying) {
			mLayers.front().playOnStartConsumed = false;
		}
	}

	bool SkeletalAnimationComponent::GetPlayOnStart() const noexcept {
		return !mLayers.empty() && mLayers.front().desc.playOnStart;
	}

	void SkeletalAnimationComponent::SetSpeed(const float speed) {
		EnsureHasAtLeastOneLayer();
		mLayers.front().desc.speed = std::clamp(speed, 0.0f, 8.0f);
	}

	float SkeletalAnimationComponent::GetSpeed() const noexcept {
		return mLayers.empty() ? 1.0f : mLayers.front().desc.speed;
	}

	void SkeletalAnimationComponent::SetPlaybackTime(const float timeSeconds) {
		EnsureHasAtLeastOneLayer();
		mLayers.front().playbackTime = std::max(timeSeconds, 0.0f);
		ClampLayerPlaybackIfPossible(0);
	}

	float SkeletalAnimationComponent::GetPlaybackTime() const noexcept {
		return mLayers.empty() ? 0.0f : mLayers.front().playbackTime;
	}

	bool SkeletalAnimationComponent::IsPlaying() const noexcept {
		return std::ranges::any_of(
			mLayers, [](const auto& layer) {
				return layer.isPlaying;
			}
		);
	}

	size_t SkeletalAnimationComponent::GetLayerCount() const noexcept {
		return mLayers.size();
	}

	size_t SkeletalAnimationComponent::AddLayer(
		const AnimationLayerDesc& layer
	) {
		RuntimeLayerState runtime = {};
		runtime.desc              = SanitizeLayerDesc(layer);
		mLayers.emplace_back(std::move(runtime));
		return mLayers.size() - 1;
	}

	bool SkeletalAnimationComponent::RemoveLayer(const size_t layerIndex) {
		if (layerIndex >= mLayers.size()) {
			return false;
		}
		mLayers.erase(
			mLayers.begin() + static_cast<std::ptrdiff_t>(layerIndex)
		);
		EnsureHasAtLeastOneLayer();
		return true;
	}

	void SkeletalAnimationComponent::ClearLayers() {
		mLayers.clear();
		EnsureHasAtLeastOneLayer();
	}

	const SkeletalAnimationComponent::AnimationLayerDesc*
	SkeletalAnimationComponent::GetLayerDesc(const size_t layerIndex) const {
		if (layerIndex >= mLayers.size()) {
			return nullptr;
		}
		return &mLayers[layerIndex].desc;
	}

	bool SkeletalAnimationComponent::SetLayerDesc(
		const size_t layerIndex, const AnimationLayerDesc& layer
	) {
		if (layerIndex >= mLayers.size()) {
			return false;
		}

		mLayers[layerIndex].desc               = SanitizeLayerDesc(layer);
		mLayers[layerIndex].playbackTime       = 0.0f;
		mLayers[layerIndex].cachedClipDuration = 0.0f;
		mLayers[layerIndex].transition.active  = false;
		return true;
	}

	bool SkeletalAnimationComponent::SetLayerWeight(
		const size_t layerIndex, const float weight
	) {
		if (layerIndex >= mLayers.size()) {
			return false;
		}
		mLayers[layerIndex].desc.weight = std::clamp(weight, 0.0f, 64.0f);
		return true;
	}

	bool SkeletalAnimationComponent::SetLayerPlaybackTime(
		const size_t layerIndex, const float timeSeconds
	) {
		if (layerIndex >= mLayers.size()) {
			return false;
		}
		mLayers[layerIndex].playbackTime = std::max(timeSeconds, 0.0f);
		ClampLayerPlaybackIfPossible(layerIndex);
		return true;
	}

	bool SkeletalAnimationComponent::SetLayerPlaying(
		const size_t layerIndex, const bool playing
	) {
		if (layerIndex >= mLayers.size()) {
			return false;
		}
		mLayers[layerIndex].isPlaying           = playing;
		mLayers[layerIndex].playOnStartConsumed = true;
		return true;
	}

	void SkeletalAnimationComponent::BuildSkinningPalette(
		const MeshAssetData& mesh, std::vector<Mat4>& outBoneMatrices
	) {
		BuildBindPosePalette(mesh, outBoneMatrices);
		EnsureHasAtLeastOneLayer();
		if (outBoneMatrices.empty()) {
			return;
		}

		const uint32_t boneCount = std::min<uint32_t>(
			static_cast<uint32_t>(mesh.skeleton.size()),
			static_cast<uint32_t>(outBoneMatrices.size())
		);
		if (boneCount == 0) {
			return;
		}

		std::vector<LocalBonePose> bindPose(boneCount);
		for (uint32_t i = 0; i < boneCount; ++i) {
			bindPose[i].translation = mesh.skeleton[i].bindLocalTranslation;
			bindPose[i].rotation    = mesh.skeleton[i].bindLocalRotation;
			bindPose[i].scale       = mesh.skeleton[i].bindLocalScale;
		}

		std::vector<BlendAccumVec3> blendedTranslation(boneCount);
		std::vector<BlendAccumQuat> blendedRotation(boneCount);
		std::vector<BlendAccumVec3> blendedScale(boneCount);

		for (size_t layerIndex = 0; layerIndex < mLayers.size(); ++layerIndex) {
			auto&       layer       = mLayers[layerIndex];
			const float layerWeight = std::max(layer.desc.weight, 0.0f);
			if (layerWeight <= 1e-6f) {
				continue;
			}

			const AnimationClipAssetData* currentClip = ResolveClip(
				mesh, layer.desc.clipName
			);
			if (!currentClip) {
				layer.cachedClipDuration = 0.0f;
				layer.transition.active  = false;
				continue;
			}

			layer.cachedClipDuration = std::max(
				currentClip->durationSeconds, 0.0f
			);
			ClampLayerPlaybackIfPossible(layerIndex);

			float fromWeight = 0.0f;
			if (
				layer.transition.active &&
				layer.transition.durationSec > 1.0e-6f
			) {
				const float t = std::clamp(
					layer.transition.elapsedSec / layer.transition.durationSec,
					0.0f,
					1.0f
				);
				fromWeight = layerWeight * (1.0f - t);
			}
			const float toWeight = std::max(0.0f, layerWeight - fromWeight);

			const auto AccumulateClip = [&](
				const AnimationClipAssetData* clip,
				const float                   playbackTime,
				const bool                    loop,
				const float                   weight
			) {
				if (!clip || weight <= 1.0e-6f) {
					return;
				}

				const float clipDuration = std::max(
					clip->durationSeconds, 0.0f
				);
				const float sampleTime = ToSampleTime(
					playbackTime, clipDuration, loop
				);

				std::unordered_map<int32_t, const SkeletonBoneTrackAssetData*>
					trackByBone;
				trackByBone.reserve(clip->boneTracks.size());
				for (const auto& track : clip->boneTracks) {
					trackByBone.emplace(track.boneIndex, &track);
				}

				for (uint32_t boneIndex = 0; boneIndex < boneCount; ++
				     boneIndex) {
					LocalBonePose sampled = bindPose[boneIndex];
					const auto    it      = trackByBone.find(
						static_cast<int32_t>(boneIndex)
					);
					if (it != trackByBone.end() && it->second) {
						const auto& track   = *it->second;
						sampled.translation = SampleVec3Track(
							track.translationKeys, sampleTime,
							sampled.translation
						);
						sampled.rotation = SampleQuatTrack(
							track.rotationKeys, sampleTime, sampled.rotation
						);
						sampled.scale = SampleVec3Track(
							track.scaleKeys, sampleTime, sampled.scale
						);
					}

					blendedTranslation[boneIndex].weightedSum +=
						sampled.translation * weight;
					blendedTranslation[boneIndex].weightSum += weight;

					AddWeightedQuaternion(
						blendedRotation[boneIndex], sampled.rotation, weight
					);

					blendedScale[boneIndex].weightedSum += sampled.scale *
						weight;
					blendedScale[boneIndex].weightSum += weight;
				}
			};

			AccumulateClip(
				currentClip,
				layer.playbackTime,
				layer.desc.loop,
				toWeight
			);

			if (fromWeight > 1.0e-6f && !layer.transition.fromClipName.
			                                   empty()) {
				const AnimationClipAssetData* fromClip = ResolveClip(
					mesh, layer.transition.fromClipName
				);
				if (fromClip) {
					layer.transition.fromCachedClipDuration = std::max(
						fromClip->durationSeconds, 0.0f
					);
					AccumulateClip(
						fromClip,
						layer.transition.fromPlaybackTime,
						layer.transition.fromLoop,
						fromWeight
					);
				} else {
					layer.transition.active = false;
				}
			}
		}

		std::vector<LocalBonePose> finalLocalPose(boneCount);
		for (uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
			const auto& bind = bindPose[boneIndex];

			const float transBlendWeight = blendedTranslation[boneIndex].
				weightSum;
			const float transBindWeight = std::max(
				0.0f, 1.0f - transBlendWeight
			);
			const float transDenom = transBlendWeight + transBindWeight;
			finalLocalPose[boneIndex].translation = transDenom > 1e-6f ?
				                                        (
					                                        blendedTranslation[
						                                        boneIndex]
					                                        .weightedSum +
					                                        bind.translation *
					                                        transBindWeight
				                                        ) /
				                                        transDenom :
				                                        bind.translation;

			const float scaleBlendWeight = blendedScale[boneIndex].weightSum;
			const float scaleBindWeight  = std::max(
				0.0f, 1.0f - scaleBlendWeight
			);
			const float scaleDenom = scaleBlendWeight + scaleBindWeight;
			finalLocalPose[boneIndex].scale = scaleDenom > 1e-6f ?
				                                  (
					                                  blendedScale[boneIndex]
					                                  .weightedSum +
					                                  bind.scale *
					                                  scaleBindWeight
				                                  ) /
				                                  scaleDenom :
				                                  bind.scale;

			BlendAccumQuat rotAccum = blendedRotation[boneIndex];
			AddWeightedQuaternion(
				rotAccum, bind.rotation, std::max(
					0.0f, 1.0f - rotAccum.weightSum
				)
			);
			finalLocalPose[boneIndex].rotation = NormalizeWeightedQuaternion(
				rotAccum, bind.rotation
			);
		}

		std::vector<Mat4> localPoseMatrices(boneCount, Mat4::identity);
		for (uint32_t i = 0; i < boneCount; ++i) {
			localPoseMatrices[i] = Mat4::Affine(
				finalLocalPose[i].scale,
				finalLocalPose[i].rotation,
				finalLocalPose[i].translation
			);
		}

		std::vector<Mat4> globalPose;
		BuildGlobalPoseFromLocals(
			mesh, boneCount, localPoseMatrices, globalPose
		);
		for (uint32_t i = 0; i < boneCount; ++i) {
			outBoneMatrices[i] =
				mesh.skeleton[i].inverseBindPose * globalPose[i];
		}
	}

	void SkeletalAnimationComponent::BuildBindPosePalette(
		const MeshAssetData& mesh, std::vector<Mat4>& outBoneMatrices
	) {
		if (outBoneMatrices.empty()) {
			return;
		}

		const uint32_t boneCount = std::min<uint32_t>(
			static_cast<uint32_t>(mesh.skeleton.size()),
			static_cast<uint32_t>(outBoneMatrices.size())
		);
		if (boneCount == 0) {
			std::ranges::fill(
				outBoneMatrices, Mat4::identity
			);
			return;
		}

		std::vector<Mat4> localPoseMatrices(boneCount, Mat4::identity);
		for (uint32_t i = 0; i < boneCount; ++i) {
			const auto& bone     = mesh.skeleton[i];
			localPoseMatrices[i] = Mat4::Affine(
				bone.bindLocalScale,
				bone.bindLocalRotation,
				bone.bindLocalTranslation
			);
		}

		std::vector<Mat4> globalPose;
		BuildGlobalPoseFromLocals(
			mesh, boneCount, localPoseMatrices, globalPose
		);
		for (uint32_t i = 0; i < boneCount; ++i) {
			const auto& bone   = mesh.skeleton[i];
			outBoneMatrices[i] = bone.inverseBindPose * globalPose[i];
		}

		for (uint32_t i = boneCount; i < outBoneMatrices.size(); ++i) {
			outBoneMatrices[i] = Mat4::identity;
		}
	}

	void SkeletalAnimationComponent::EnsureHasAtLeastOneLayer() {
		if (!mLayers.empty()) {
			return;
		}
		RuntimeLayerState layer = {};
		layer.desc              = SanitizeLayerDesc(layer.desc);
		mLayers.emplace_back(std::move(layer));
	}

	void SkeletalAnimationComponent::ClampLayerPlaybackIfPossible(
		const size_t layerIndex
	) {
		if (layerIndex >= mLayers.size()) {
			return;
		}

		auto& layer        = mLayers[layerIndex];
		layer.playbackTime = std::max(layer.playbackTime, 0.0f);

		if (layer.cachedClipDuration <= 0.0f) {
			return;
		}

		if (layer.desc.loop) {
			if (layer.playbackTime >= layer.cachedClipDuration) {
				const float wrapped = std::fmod(
					layer.playbackTime, layer.cachedClipDuration
				);
				layer.playbackTime = wrapped < 0.0f ?
					                     wrapped + layer.cachedClipDuration :
					                     wrapped;
			}
		} else if (layer.playbackTime >= layer.cachedClipDuration) {
			layer.playbackTime = layer.cachedClipDuration;
			layer.isPlaying    = false;
		}
	}

	REGISTER_COMPONENT(SkeletalAnimationComponent);
}
