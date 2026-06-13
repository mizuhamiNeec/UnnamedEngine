#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../base/BaseComponent.h"

#include "core/math/Mat4.h"

namespace Unnamed {
	struct MeshAssetData;
	class JsonReader;
	class JsonWriter;

	class SkeletalAnimationComponent final : public BaseComponent {
	public:
		struct AnimationLayerDesc {
			std::string clipName;
			float       weight      = 1.0f;
			float       speed       = 1.0f;
			bool        loop        = true;
			bool        playOnStart = true;
		};

		struct AnimationStateDesc {
			std::string stateId;
			size_t      layerIndex    = 0;
			std::string clipName;
			float       weight        = 1.0f;
			float       speed         = 1.0f;
			float       blendSec      = 0.0f;
			bool        loop          = true;
			bool        restartOnPlay = true;
		};

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

		void OnAttached() override;
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
		[[nodiscard]] std::vector<std::string> DebugGetStateIds() const;
#endif

		void Play();
		void Pause();
		void Stop();
		bool PlayState(std::string_view stateId);
		[[nodiscard]] bool HasState(std::string_view stateId) const;

		void SetClipName(const std::string& clipName);
		[[nodiscard]] const std::string& GetClipName() const noexcept;

		void               SetLoop(bool enabled);
		[[nodiscard]] bool GetLoop() const noexcept;

		void               SetPlayOnStart(bool enabled);
		[[nodiscard]] bool GetPlayOnStart() const noexcept;

		void                SetSpeed(float speed);
		[[nodiscard]] float GetSpeed() const noexcept;

		void                SetPlaybackTime(float timeSeconds);
		[[nodiscard]] float GetPlaybackTime() const noexcept;

		[[nodiscard]] bool IsPlaying() const noexcept;

		[[nodiscard]] size_t GetLayerCount() const noexcept;
		size_t AddLayer(const AnimationLayerDesc& layer);
		bool RemoveLayer(size_t layerIndex);
		void ClearLayers();
		[[nodiscard]] const AnimationLayerDesc* GetLayerDesc(
			size_t layerIndex
		) const;
		bool SetLayerDesc(size_t layerIndex, const AnimationLayerDesc& layer);
		bool SetLayerWeight(size_t layerIndex, float weight);
		bool SetLayerPlaybackTime(size_t layerIndex, float timeSeconds);
		bool SetLayerPlaying(size_t layerIndex, bool playing);

		void BuildSkinningPalette(
			const MeshAssetData& mesh, std::vector<Mat4>& outBoneMatrices
		);
		static void BuildBindPosePalette(
			const MeshAssetData& mesh, std::vector<Mat4>& outBoneMatrices
		);

	private:
		struct RuntimeLayerState {
			AnimationLayerDesc desc;
			float              playbackTime        = 0.0f;
			float              cachedClipDuration  = 0.0f;
			bool               isPlaying           = false;
			bool               playOnStartConsumed = false;

			struct TransitionState {
				std::string fromClipName;
				float       fromPlaybackTime       = 0.0f;
				float       fromCachedClipDuration = 0.0f;
				float       fromSpeed              = 1.0f;
				bool        fromLoop               = true;
				float       durationSec            = 0.0f;
				float       elapsedSec             = 0.0f;
				bool        active                 = false;
			};

			TransitionState transition;
		};

		void EnsureHasAtLeastOneLayer();
		void ClampLayerPlaybackIfPossible(size_t layerIndex);

		std::vector<RuntimeLayerState> mLayers;
		std::unordered_map<std::string, AnimationStateDesc> mStateMap;
	};
}
