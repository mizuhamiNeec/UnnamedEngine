#include "CourseResultFlowComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/ImGui/Icons.h"
#include "engine/game/GamePathResolver.h"
#include "engine/game/IGameModule.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiDigitStripComponent.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/audio/AudioSourceComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

#include "CourseProgressComponent.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "CourseResultFlow";
		constexpr float            kMinDurationSec = 0.01f;

		[[nodiscard]] bool IsEngineRootRelativePath(const std::string_view path) {
			return path.rfind("content/", 0) == 0 ||
			       path.rfind("projects/", 0) == 0;
		}

		[[nodiscard]] std::string ResolveResultContentPath(
			const std::string_view configuredPath,
			const std::string_view fallbackRelativePath
		) {
			std::string effectivePath = configuredPath.empty() ?
				                            std::string(fallbackRelativePath) :
				                            std::string(configuredPath);
			if (effectivePath.empty()) {
				return {};
			}

			// 旧設定の "content/..." 指定はプロジェクトルート基準として扱います。
			if (IsEngineRootRelativePath(effectivePath)) {
				return "./" + effectivePath;
			}

			if (const IGameModule* gameModule = ServiceLocator::Get<IGameModule>()) {
				return ResolveGameContentPath(
					gameModule->GetGameModulePaths(),
					effectivePath
				);
			}
			return effectivePath;
		}

#ifdef _DEBUG
		template <size_t N>
		void DrawStringInput(const char* label, std::string& value) {
			std::array<char, N> buffer = {};
			const size_t copyLen = std::min(value.size(), buffer.size() - 1);
			if (copyLen > 0) {
				std::memcpy(buffer.data(), value.data(), copyLen);
			}
			if (ImGui::InputText(label, buffer.data(), buffer.size())) {
				value = buffer.data();
			}
		}
#endif
	}

	void CourseResultFlowComponent::OnAttached() {
		ResolveBindings();
		HideResultWidgets();
		SetFadeOverlayAlpha(0.0f);
	}

	void CourseResultFlowComponent::OnTick(const float deltaTime) {
		ResolveBindings();

		const float clampedDelta = std::max(0.0f, deltaTime);
		switch (mPhase) {
			case PHASE::WAITING: {
				const bool cleared =
					mCourseProgress &&
					mCourseProgress->GetSnapshot().courseCleared;
				if (cleared && !mWasCourseCleared) {
					BeginResult();
				}
				mWasCourseCleared = cleared;
				break;
			}
			case PHASE::SHOW_RESULT:
				TickShowResult(clampedDelta);
				break;
			case PHASE::FADE_OUT:
				TickFadeOut(clampedDelta);
				break;
			case PHASE::TRANSITIONED:
				UpdateResultWidgets(1.0f);
				SetFadeOverlayAlpha(1.0f);
				break;
			default: break;
		}
	}

	void CourseResultFlowComponent::OnEditorTick(const float deltaTime) {
		(void)deltaTime;
		ResolveBindings();
	}

	void CourseResultFlowComponent::OnDetached() {
		RestoreLockTargets();
		HideResultWidgets();
		SetFadeOverlayAlpha(0.0f);
		BaseComponent::OnDetached();
	}

	std::string_view CourseResultFlowComponent::GetStableName() const {
		return "parkour.CourseResultFlow";
	}

	std::string_view CourseResultFlowComponent::GetComponentName() const {
		return "CourseResultFlow";
	}

	uint32_t CourseResultFlowComponent::GetIcon() const {
		return kIconTimer;
	}

#ifdef _DEBUG
	void CourseResultFlowComponent::DrawInspectorImGui() {
		DrawStringInput<64>("Course Id", mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		DrawStringInput<128>("Title Scene Path", mTitleScenePath);
		DrawStringInput<64>("Result Root Widget", mResultRootWidgetName);
		DrawStringInput<64>("Clear Image Widget", mClearImageWidgetName);
		DrawStringInput<64>("Elapsed Digits Widget", mElapsedDigitsWidgetName);
		DrawStringInput<64>("Fade Overlay Widget", mFadeOverlayWidgetName);
		DrawStringInput<128>("Clear Texture", mClearTexturePath);
		DrawStringInput<128>("Digit Texture", mDigitTexturePath);
		ImGui::DragFloat(
			"Result Hold Seconds",
			&mResultHoldSeconds,
			0.05f,
			kMinDurationSec,
			10.0f
		);
		ImGui::DragFloat(
			"Fade Out Seconds",
			&mFadeOutSeconds,
			0.05f,
			kMinDurationSec,
			5.0f
		);
		ImGui::InputScalar(
			"Hud Canvas Entity Guid",
			ImGuiDataType_U64,
			&mHudCanvasEntityGuid
		);
		ImGui::InputScalar(
			"Clear Audio Source Guid",
			ImGuiDataType_U64,
			&mClearAudioSourceGuid
		);
		ImGui::DragInt(
			"Elapsed Min Digits",
			&mElapsedDigitsMinDigits,
			1.0f,
			1,
			12
		);
		ImGui::DragFloat(
			"Elapsed Display Scale",
			&mElapsedDisplayScale,
			1.0f,
			1.0f,
			1000.0f
		);
	}
#endif

	void CourseResultFlowComponent::Deserialize(const JsonReader& reader) {
		mCourseId = reader["courseId"].GetString(mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		mTitleScenePath = reader["titleScenePath"].GetString(mTitleScenePath);
		if (const JsonReader node = reader["hudCanvasEntityGuid"]; node.Valid()) {
			mHudCanvasEntityGuid = node.GetUint64();
		}
		if (const JsonReader node = reader["clearAudioSourceGuid"]; node.Valid()) {
			mClearAudioSourceGuid = node.GetUint64();
		}

		mResultRootWidgetName =
			reader["resultRootWidgetName"].GetString(mResultRootWidgetName);
		mClearImageWidgetName =
			reader["clearImageWidgetName"].GetString(mClearImageWidgetName);
		mElapsedDigitsWidgetName =
			reader["elapsedDigitsWidgetName"].GetString(mElapsedDigitsWidgetName);
		mFadeOverlayWidgetName =
			reader["fadeOverlayWidgetName"].GetString(mFadeOverlayWidgetName);
		mClearTexturePath =
			reader["clearTexturePath"].GetString(mClearTexturePath);
		mDigitTexturePath =
			reader["digitTexturePath"].GetString(mDigitTexturePath);

		if (const JsonReader node = reader["resultHoldSeconds"]; node.Valid()) {
			mResultHoldSeconds =
				std::max(kMinDurationSec, node.GetFloat(mResultHoldSeconds));
		}
		if (const JsonReader node = reader["fadeOutSeconds"]; node.Valid()) {
			mFadeOutSeconds = std::max(kMinDurationSec, node.GetFloat(mFadeOutSeconds));
		}
		if (const JsonReader node = reader["elapsedDigitsMinDigits"];
			node.Valid()) {
			mElapsedDigitsMinDigits = std::max(1, node.GetInt(mElapsedDigitsMinDigits));
		}
		if (const JsonReader node = reader["elapsedDisplayScale"]; node.Valid()) {
			mElapsedDisplayScale =
				std::max(1.0f, node.GetFloat(mElapsedDisplayScale));
		}

		if (const JsonReader lockNode = reader["lockTargets"]; lockNode.Valid()) {
			const JsonReader lockArray = lockNode.GetArray();
			mLockTargets.clear();
			mLockTargets.reserve(lockArray.Size());
			for (size_t i = 0; i < lockArray.Size(); ++i) {
				const JsonReader item = lockArray[i];
				if (!item.Valid()) {
					continue;
				}

				LockTargetSpec spec = {};
				if (const JsonReader entityNode = item["entityGuid"];
					entityNode.Valid()) {
					spec.entityGuid = entityNode.GetUint64();
				}
				spec.componentStableName =
					item["componentStableName"].GetString(spec.componentStableName);
				if (!spec.componentStableName.empty()) {
					mLockTargets.emplace_back(std::move(spec));
				}
			}
		}
	}

	void CourseResultFlowComponent::Serialize(JsonWriter& writer) const {
		writer.Key("courseId");
		writer.Write(mCourseId);
		writer.Key("titleScenePath");
		writer.Write(mTitleScenePath);
		writer.Key("hudCanvasEntityGuid");
		writer.Write(mHudCanvasEntityGuid);
		writer.Key("clearAudioSourceGuid");
		writer.Write(mClearAudioSourceGuid);
		writer.Key("resultRootWidgetName");
		writer.Write(mResultRootWidgetName);
		writer.Key("clearImageWidgetName");
		writer.Write(mClearImageWidgetName);
		writer.Key("elapsedDigitsWidgetName");
		writer.Write(mElapsedDigitsWidgetName);
		writer.Key("fadeOverlayWidgetName");
		writer.Write(mFadeOverlayWidgetName);
		writer.Key("clearTexturePath");
		writer.Write(mClearTexturePath);
		writer.Key("digitTexturePath");
		writer.Write(mDigitTexturePath);
		writer.Key("resultHoldSeconds");
		writer.Write(mResultHoldSeconds);
		writer.Key("fadeOutSeconds");
		writer.Write(mFadeOutSeconds);
		writer.Key("elapsedDigitsMinDigits");
		writer.Write(mElapsedDigitsMinDigits);
		writer.Key("elapsedDisplayScale");
		writer.Write(mElapsedDisplayScale);

		writer.Key("lockTargets");
		writer.BeginArray();
		for (const LockTargetSpec& spec : mLockTargets) {
			SerializeLockTarget(writer, spec);
		}
		writer.EndArray();
	}

	void CourseResultFlowComponent::BeginResult() {
		if (!mCourseProgress) {
			return;
		}

		const CourseProgressSnapshot& snapshot = mCourseProgress->GetSnapshot();
		mLatchedElapsedSeconds =
			snapshot.clearedElapsedSeconds > 0.0f ?
				snapshot.clearedElapsedSeconds :
				snapshot.elapsedSeconds;
		mPhase               = PHASE::SHOW_RESULT;
		mPhaseElapsedSeconds = 0.0f;

		ApplyLockTargets();
		if (mClearAudio) {
			mClearAudio->Play();
		}

		UpdateResultWidgets(1.0f);
		SetFadeOverlayAlpha(0.0f);
		Msg(
			kChannel,
			"Course result started: course={} elapsed={:.2f}s",
			mCourseId,
			mLatchedElapsedSeconds
		);
	}

	void CourseResultFlowComponent::TickShowResult(const float deltaTime) {
		mPhaseElapsedSeconds += deltaTime;
		UpdateResultWidgets(1.0f);
		SetFadeOverlayAlpha(0.0f);

		if (mPhaseElapsedSeconds < mResultHoldSeconds) {
			return;
		}

		mPhase               = PHASE::FADE_OUT;
		mPhaseElapsedSeconds = 0.0f;
	}

	void CourseResultFlowComponent::TickFadeOut(const float deltaTime) {
		mPhaseElapsedSeconds += deltaTime;
		const float t = std::clamp(
			mPhaseElapsedSeconds / std::max(kMinDurationSec, mFadeOutSeconds),
			0.0f,
			1.0f
		);
		UpdateResultWidgets(1.0f);
		SetFadeOverlayAlpha(EvaluateEase(t));

		if (t >= 1.0f) {
			CommitTitleTransition();
		}
	}

	void CourseResultFlowComponent::CommitTitleTransition() {
		World* world = GetWorld();
		if (!world) {
			return;
		}

		const std::string titleScenePath = ResolveResultContentPath(
			mTitleScenePath,
			"scenes/title.json"
		);
		if (titleScenePath.empty()) {
			Warning(kChannel, "Title scene path is empty.");
			return;
		}

		mPhase = PHASE::TRANSITIONED;
		world->RequestSceneTransition(titleScenePath);
		RestoreLockTargets();
	}

	void CourseResultFlowComponent::ResolveBindings() {
		ClearResolvedBindings();

		Scene* scene = GetScene();
		if (!scene) {
			return;
		}

		const std::string normalizedCourseId =
			mCourseId.empty() ? std::string("default") : mCourseId;
		const auto tryBindCourseProgress = [&](Entity& entity) -> bool {
			bool found = false;
			entity.ForEachComponent(
				[&](BaseComponent& component) {
					auto* progress = dynamic_cast<CourseProgressComponent*>(&component);
					if (!progress || progress->GetCourseId() != normalizedCourseId) {
						return true;
					}
					mCourseProgress = progress;
					found           = true;
					return false;
				}
			);
			return found;
		};

		if (Entity* owner = GetOwner()) {
			(void)tryBindCourseProgress(*owner);
		}
		if (!mCourseProgress) {
			for (const auto& entityPtr : scene->GetEntities()) {
				if (entityPtr && tryBindCourseProgress(*entityPtr)) {
					break;
				}
			}
		}

		Entity* hudEntity = nullptr;
		if (mHudCanvasEntityGuid != 0) {
			hudEntity = scene->FindEntity(mHudCanvasEntityGuid);
		}
		if (!hudEntity) {
			hudEntity = GetOwner();
		}
		if (hudEntity) {
			mHudCanvas = hudEntity->GetComponent<UiCanvasComponent>();
		}

		if (mHudCanvas && mHudCanvas->EnsureRuntimeLoaded()) {
			Gui::UiRoot*   root       = mHudCanvas->GetRuntimeRoot();
			Gui::UiWidget* rootWidget = root ? root->GetRootWidget() : nullptr;
			if (rootWidget) {
				mResultRootWidget = FindWidgetByNameRecursive(
					rootWidget,
					mResultRootWidgetName
				);
				mClearImageWidget = FindWidgetByNameRecursive(
					rootWidget,
					mClearImageWidgetName
				);
				mElapsedDigitsWidget = FindWidgetByNameRecursive(
					rootWidget,
					mElapsedDigitsWidgetName
				);
				mFadeOverlayWidget = FindWidgetByNameRecursive(
					rootWidget,
					mFadeOverlayWidgetName
				);

				if (mClearImageWidget) {
					mClearImageTexture =
						mClearImageWidget->GetOrAddComponent<Gui::UiTextureComponent>();
				}
				if (mElapsedDigitsWidget) {
					mElapsedDigits =
						mElapsedDigitsWidget->GetOrAddComponent<
							Gui::UiDigitStripComponent>();
				}
				if (mFadeOverlayWidget) {
					mFadeOverlayTexture =
						mFadeOverlayWidget->GetOrAddComponent<Gui::UiTextureComponent>();
				}
			}
		}

		mClearAudio = ResolveAudioSourceByGuid(mClearAudioSourceGuid);
	}

	void CourseResultFlowComponent::ClearResolvedBindings() {
		mCourseProgress      = nullptr;
		mHudCanvas           = nullptr;
		mClearAudio          = nullptr;
		mResultRootWidget    = nullptr;
		mClearImageWidget    = nullptr;
		mElapsedDigitsWidget = nullptr;
		mFadeOverlayWidget   = nullptr;
		mClearImageTexture   = nullptr;
		mElapsedDigits       = nullptr;
		mFadeOverlayTexture  = nullptr;
	}

	void CourseResultFlowComponent::UpdateResultWidgets(const float alpha)
	const {
		const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
		if (mResultRootWidget) {
			mResultRootWidget->SetVisible(clampedAlpha > 0.0f);
			mResultRootWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}

		if (mClearImageWidget) {
			mClearImageWidget->SetVisible(clampedAlpha > 0.0f);
		}
		if (mClearImageTexture) {
			mClearImageTexture->SetTexturePath(
				ResolveResultContentPath(mClearTexturePath, "textures/clear.png")
			);
			Gui::Color color = mClearImageTexture->GetColor();
			color.a          = clampedAlpha;
			mClearImageTexture->SetColor(color);
		}

		if (mElapsedDigitsWidget) {
			mElapsedDigitsWidget->SetVisible(clampedAlpha > 0.0f);
		}
		if (mElapsedDigits) {
			const int displayValue = static_cast<int>(
				std::lround(std::max(0.0f, mLatchedElapsedSeconds) *
				            mElapsedDisplayScale)
			);
			mElapsedDigits->SetStripTexturePath(
				ResolveResultContentPath(mDigitTexturePath, "textures/digits.png")
			);
			mElapsedDigits->SetMinDigits(mElapsedDigitsMinDigits);
			mElapsedDigits->SetValue(displayValue);
			Gui::Color color = mElapsedDigits->GetColor();
			color.a          = clampedAlpha;
			mElapsedDigits->SetColor(color);
		}
	}

	void CourseResultFlowComponent::SetFadeOverlayAlpha(const float alpha)
	const {
		if (!mFadeOverlayWidget || !mFadeOverlayTexture) {
			return;
		}

		const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
		mFadeOverlayWidget->SetVisible(clampedAlpha > 0.0f);
		Gui::Color color = mFadeOverlayTexture->GetColor();
		color.a          = clampedAlpha;
		mFadeOverlayTexture->SetColor(color);
		mFadeOverlayWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
	}

	void CourseResultFlowComponent::HideResultWidgets() const {
		if (mResultRootWidget) {
			mResultRootWidget->SetVisible(false);
			mResultRootWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
		if (mClearImageWidget) {
			mClearImageWidget->SetVisible(false);
			mClearImageWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
		if (mElapsedDigitsWidget) {
			mElapsedDigitsWidget->SetVisible(false);
			mElapsedDigitsWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
	}

	void CourseResultFlowComponent::ApplyLockTargets() {
		RestoreLockTargets();
		std::vector<BaseComponent*> lockedComponents = {};
		lockedComponents.reserve(mLockTargets.size());

		for (const LockTargetSpec& spec : mLockTargets) {
			BaseComponent* target = ResolveLockTarget(spec);
			if (!target || target == this) {
				continue;
			}
			if (std::ranges::find(lockedComponents, target) != lockedComponents.end()) {
				continue;
			}

			mActiveLocks.emplace_back(
				ActiveLockState{
					.component      = target,
					.previousActive = target->IsActive(),
				}
			);
			lockedComponents.emplace_back(target);
			target->SetActive(false);
		}
	}

	void CourseResultFlowComponent::RestoreLockTargets() {
		for (const ActiveLockState& state : mActiveLocks) {
			if (state.component) {
				state.component->SetActive(state.previousActive);
			}
		}
		mActiveLocks.clear();
	}

	BaseComponent* CourseResultFlowComponent::ResolveLockTarget(
		const LockTargetSpec& spec
	) const {
		Scene* scene = GetScene();
		if (!scene || spec.componentStableName.empty()) {
			return nullptr;
		}

		const auto findInEntity = [&](Entity& entity) -> BaseComponent* {
			BaseComponent* found = nullptr;
			entity.ForEachComponent(
				[&](BaseComponent& component) {
					if (component.GetStableName() != spec.componentStableName) {
						return true;
					}
					found = &component;
					return false;
				}
			);
			return found;
		};

		if (spec.entityGuid != 0) {
			if (Entity* entity = scene->FindEntity(spec.entityGuid)) {
				return findInEntity(*entity);
			}
			return nullptr;
		}

		if (Entity* owner = GetOwner()) {
			if (BaseComponent* found = findInEntity(*owner)) {
				return found;
			}
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (entityPtr) {
				if (BaseComponent* found = findInEntity(*entityPtr)) {
					return found;
				}
			}
		}
		return nullptr;
	}

	AudioSourceComponent* CourseResultFlowComponent::ResolveAudioSourceByGuid(
		const uint64_t componentGuid
	) const {
		if (componentGuid == 0) {
			return nullptr;
		}

		Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}

			AudioSourceComponent* found = nullptr;
			entityPtr->ForEachComponent(
				[&](BaseComponent& component) {
					if (component.GetGuid() != componentGuid) {
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
		return nullptr;
	}

	Gui::UiWidget* CourseResultFlowComponent::FindWidgetByNameRecursive(
		Gui::UiWidget*          root,
		const std::string_view widgetName
	) {
		if (!root) {
			return nullptr;
		}
		if (root->GetName() == widgetName) {
			return root;
		}

		for (const auto& child : root->GetChildren()) {
			if (!child) {
				continue;
			}
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(
				child.get(),
				widgetName
			)) {
				return found;
			}
		}

		for (Gui::UiWidget* child : root->GetReferenceChildren()) {
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(child, widgetName)) {
				return found;
			}
		}
		return nullptr;
	}

	void CourseResultFlowComponent::SerializeLockTarget(
		JsonWriter&            writer,
		const LockTargetSpec& spec
	) {
		writer.BeginObject();
		writer.Key("entityGuid");
		writer.Write(spec.entityGuid);
		writer.Key("componentStableName");
		writer.Write(spec.componentStableName);
		writer.EndObject();
	}

	float CourseResultFlowComponent::EvaluateEase(const float t) {
		return Math::CubicBezier(
			std::clamp(t, 0.0f, 1.0f),
			0.2f,
			0.0f,
			0.0f,
			1.0f
		);
	}

	REGISTER_COMPONENT(CourseResultFlowComponent);
}
