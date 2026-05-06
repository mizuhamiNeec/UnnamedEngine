#include "GameStartCutsceneComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <ranges>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"
#include "core/math/Quaternion.h"

#include "engine/ImGui/Icons.h"
#include "engine/gui/Rect.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/gui/components/UiTransformComponent.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/audio/AudioSourceComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/world/World.h"

#include "game/parkour/runtime/ParkourFlowRuntimeState.h"

namespace Unnamed {
	namespace {
		constexpr float kMinDurationSec = 0.01f;
		constexpr std::string_view kDefaultLookLockStableName = "game.CameraRotator";

#ifdef _DEBUG
		template <size_t N>
		void DrawStringInput(const char* label, std::string& value) {
			std::array<char, N> buffer = {};
			const size_t        copyLen = std::min(value.size(), buffer.size() - 1);
			if (copyLen > 0) {
				std::memcpy(buffer.data(), value.data(), copyLen);
			}
			if (ImGui::InputText(label, buffer.data(), buffer.size())) {
				value = buffer.data();
			}
		}
#endif
	}

	void GameStartCutsceneComponent::OnAttached() {
		// シーンロード中は参照先エンティティが未生成のため、開始は初回Tickへ遅延します。
		mPendingStartSequence = true;
	}

	void GameStartCutsceneComponent::OnTick(const float deltaTime) {
		if (mPendingStartSequence) {
			mPendingStartSequence = false;
			StartSequence();
		}

		if (!mSequenceActive || mPhase == PHASE::GAMEPLAY) {
			return;
		}

		const float clampedDelta = std::max(0.0f, deltaTime);
		switch (mPhase) {
			case PHASE::TOUR: TickTour(clampedDelta); break;
			case PHASE::COUNTDOWN: TickCountdown(clampedDelta); break;
			case PHASE::GAMEPLAY:
			default: break;
		}
	}

	void GameStartCutsceneComponent::OnRenderTick(const float, const float) {
		if (!mSequenceActive) {
			return;
		}

		// Sequence等によるカメラ上書きがあっても、描画直前に狙いのカメラを再適用します。
		if (mPhase == PHASE::TOUR) {
			(void)SwitchToTourCamera();
		} else if (mPhase == PHASE::COUNTDOWN) {
			(void)SwitchToPlayerCamera();
		}
	}

	void GameStartCutsceneComponent::OnEditorTick(const float) {
		DrawShotDebugAxes();
	}

	void GameStartCutsceneComponent::OnDetached() {
		RestoreLockTargets();
		HideAllCutsceneWidgets();
		(void)SwitchToPlayerCamera();

		mSequenceActive          = false;
		mPhase                   = PHASE::GAMEPLAY;
		mLastCountdownCueStep    = -1;
		mPendingStartSequence    = false;
		mGameplayControlReleased = false;

		// シーン破棄中は Scene::FindEntity が安全に使えないため、再解決せずにキャッシュだけ破棄します。
		ClearResolvedBindings();
	}

	void GameStartCutsceneComponent::DrawShotDebugAxes() const {
		World* world = GetWorld();
		if (!world) {
			return;
		}

		auto& debugDraw = world->GetDebugDraw();
		for (const ShotSpec& shot : mShots) {
			const auto drawShotAxis = [&debugDraw](
				const Vec3& cameraPos,
				const Vec3& lookAtPos,
				const float axisLength
			) {
				Vec3 forward = lookAtPos - cameraPos;
				if (forward.IsZero()) {
					forward = Vec3::forward;
				} else {
					forward.Normalize();
				}

				// 視線と平行になりにくいアップ候補を選び、安定した直交基底を作ります。
				const Vec3 worldUp =
					std::abs(forward.Dot(Vec3::up)) > 0.99f ?
						Vec3::forward :
						Vec3::up;
				Vec3 right = worldUp.Cross(forward);
				if (right.IsZero()) {
					right = Vec3::right;
				} else {
					right.Normalize();
				}
				Vec3 up = forward.Cross(right);
				if (up.IsZero()) {
					up = Vec3::up;
				} else {
					up.Normalize();
				}

				debugDraw.DrawAxis(
					cameraPos,
					right * axisLength,
					up * axisLength,
					forward * axisLength
				);
			};

			// 各ショットの開始点/終了点の両方を可視化して、座標調整をしやすくします。
			constexpr float kAxisLength = 1.5f;
			drawShotAxis(shot.startPos, shot.startLook, kAxisLength);
			drawShotAxis(shot.endPos, shot.endLook, kAxisLength);
		}
	}

	std::string_view GameStartCutsceneComponent::GetStableName() const {
		return "parkour.GameStartCutscene";
	}

	std::string_view GameStartCutsceneComponent::GetComponentName() const {
		return "GameStartCutscene";
	}

	uint32_t GameStartCutsceneComponent::GetIcon() const {
		return kIconVideoCam;
	}

#ifdef _DEBUG
	void GameStartCutsceneComponent::DrawInspectorImGui() {
		ImGui::InputScalar(
			"Tour Camera Entity Guid",
			ImGuiDataType_U64,
			&mTourCameraEntityGuid
		);
		ImGui::InputScalar(
			"Player Camera Entity Guid",
			ImGuiDataType_U64,
			&mPlayerCameraEntityGuid
		);
		ImGui::InputScalar(
			"Hud Canvas Entity Guid",
			ImGuiDataType_U64,
			&mHudCanvasEntityGuid
		);
		ImGui::InputScalar(
			"Count Audio Source Guid",
			ImGuiDataType_U64,
			&mCountAudioSourceGuid
		);
		ImGui::InputScalar(
			"Start Audio Source Guid",
			ImGuiDataType_U64,
			&mStartAudioSourceGuid
		);

		DrawStringInput<64>("Skip Action", mSkipAction);
		DrawStringInput<64>("Digit Widget Name", mCountdownDigitWidgetName);
		DrawStringInput<64>("Start Widget Name", mCountdownStartWidgetName);
		DrawStringInput<64>("Fade Widget Name", mFadeOverlayWidgetName);

		ImGui::DragFloat("Fade Out Seconds", &mFadeOutSeconds, 0.01f, 0.01f, 3.0f);
		ImGui::DragFloat("Fade In Seconds", &mFadeInSeconds, 0.01f, 0.01f, 3.0f);
		ImGui::DragFloat(
			"Countdown Digit Seconds",
			&mCountdownDigitSeconds,
			0.01f,
			0.01f,
			3.0f
		);
		ImGui::DragFloat(
			"Countdown Start Seconds",
			&mCountdownStartSeconds,
			0.01f,
			0.01f,
			3.0f
		);

		ImGui::SeparatorText("Shots");
		if (ImGui::Button("Add Shot")) {
			mShots.emplace_back();
		}
		for (size_t i = 0; i < mShots.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			ShotSpec& shot = mShots[i];
			ImGui::Text("Shot %llu", static_cast<unsigned long long>(i));
			ImGui::DragFloat3("Start Pos", &shot.startPos.x, 0.1f);
			ImGui::DragFloat3("End Pos", &shot.endPos.x, 0.1f);
			ImGui::DragFloat3("Start Look", &shot.startLook.x, 0.1f);
			ImGui::DragFloat3("End Look", &shot.endLook.x, 0.1f);
			ImGui::DragFloat("Duration", &shot.durationSeconds, 0.01f, 0.01f, 20.0f);
			if (ImGui::Button("Remove Shot")) {
				mShots.erase(mShots.begin() + static_cast<ptrdiff_t>(i));
				ImGui::PopID();
				break;
			}
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::SeparatorText("Lock Targets");
		if (ImGui::Button("Add Lock Target")) {
			mLockTargets.emplace_back();
		}
		for (size_t i = 0; i < mLockTargets.size(); ++i) {
			ImGui::PushID(1000 + static_cast<int>(i));
			LockTargetSpec& spec = mLockTargets[i];
			ImGui::InputScalar("Entity Guid", ImGuiDataType_U64, &spec.entityGuid);
			DrawStringInput<96>("Component StableName", spec.componentStableName);
			if (ImGui::Button("Remove Lock")) {
				mLockTargets.erase(mLockTargets.begin() + static_cast<ptrdiff_t>(i));
				ImGui::PopID();
				break;
			}
			ImGui::Separator();
			ImGui::PopID();
		}
	}
#endif

	void GameStartCutsceneComponent::Deserialize(const JsonReader& reader) {
		if (const JsonReader node = reader["tourCameraEntityGuid"]; node.Valid()) {
			mTourCameraEntityGuid = node.GetUint64();
		}
		if (const JsonReader node = reader["playerCameraEntityGuid"]; node.Valid()) {
			mPlayerCameraEntityGuid = node.GetUint64();
		}
		if (const JsonReader node = reader["hudCanvasEntityGuid"]; node.Valid()) {
			mHudCanvasEntityGuid = node.GetUint64();
		}
		if (const JsonReader node = reader["countAudioSourceGuid"]; node.Valid()) {
			mCountAudioSourceGuid = node.GetUint64();
		}
		if (const JsonReader node = reader["startAudioSourceGuid"]; node.Valid()) {
			mStartAudioSourceGuid = node.GetUint64();
		}

		mSkipAction = reader["skipAction"].GetString(mSkipAction);

		mCountdownDigitWidgetName =
			reader["countdownDigitWidgetName"].GetString(mCountdownDigitWidgetName);
		mCountdownStartWidgetName =
			reader["countdownStartWidgetName"].GetString(mCountdownStartWidgetName);
		mFadeOverlayWidgetName =
			reader["fadeOverlayWidgetName"].GetString(mFadeOverlayWidgetName);

		if (const JsonReader node = reader["fadeOutSeconds"]; node.Valid()) {
			mFadeOutSeconds = std::max(kMinDurationSec, node.GetFloat(mFadeOutSeconds));
		}
		if (const JsonReader node = reader["fadeInSeconds"]; node.Valid()) {
			mFadeInSeconds = std::max(kMinDurationSec, node.GetFloat(mFadeInSeconds));
		}
		if (const JsonReader node = reader["countdownDigitSeconds"]; node.Valid()) {
			mCountdownDigitSeconds =
				std::max(kMinDurationSec, node.GetFloat(mCountdownDigitSeconds));
		}
		if (const JsonReader node = reader["countdownStartSeconds"]; node.Valid()) {
			mCountdownStartSeconds =
				std::max(kMinDurationSec, node.GetFloat(mCountdownStartSeconds));
		}

		if (const JsonReader shotsNode = reader["shots"]; shotsNode.Valid()) {
			const JsonReader         shotArray = shotsNode.GetArray();
			std::vector<ShotSpec> parsedShots = {};
			parsedShots.reserve(shotArray.Size());
			for (size_t i = 0; i < shotArray.Size(); ++i) {
				const JsonReader shotNode = shotArray[i];
				if (!shotNode.Valid()) {
					continue;
				}

				ShotSpec shot        = {};
				shot.startPos        = shotNode["startPos"].GetVec3(shot.startPos);
				shot.endPos          = shotNode["endPos"].GetVec3(shot.endPos);
				shot.startLook       = shotNode["startLook"].GetVec3(shot.startLook);
				shot.endLook         = shotNode["endLook"].GetVec3(shot.endLook);
				shot.durationSeconds = std::max(
					kMinDurationSec,
					shotNode["durationSeconds"].GetFloat(shot.durationSeconds)
				);
				parsedShots.emplace_back(shot);
			}
			if (!parsedShots.empty()) {
				mShots = std::move(parsedShots);
			}
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
				if (const JsonReader entityNode = item["entityGuid"]; entityNode.Valid()) {
					spec.entityGuid = entityNode.GetUint64();
				}
				spec.componentStableName =
					item["componentStableName"].GetString(spec.componentStableName);
				if (spec.componentStableName.empty()) {
					continue;
				}
				mLockTargets.emplace_back(std::move(spec));
			}
		}
	}

	void GameStartCutsceneComponent::Serialize(JsonWriter& writer) const {
		writer.Key("tourCameraEntityGuid");
		writer.Write(mTourCameraEntityGuid);
		writer.Key("playerCameraEntityGuid");
		writer.Write(mPlayerCameraEntityGuid);
		writer.Key("hudCanvasEntityGuid");
		writer.Write(mHudCanvasEntityGuid);
		writer.Key("countAudioSourceGuid");
		writer.Write(mCountAudioSourceGuid);
		writer.Key("startAudioSourceGuid");
		writer.Write(mStartAudioSourceGuid);
		writer.Key("skipAction");
		writer.Write(mSkipAction);

		writer.Key("countdownDigitWidgetName");
		writer.Write(mCountdownDigitWidgetName);
		writer.Key("countdownStartWidgetName");
		writer.Write(mCountdownStartWidgetName);
		writer.Key("fadeOverlayWidgetName");
		writer.Write(mFadeOverlayWidgetName);

		writer.Key("fadeOutSeconds");
		writer.Write(mFadeOutSeconds);
		writer.Key("fadeInSeconds");
		writer.Write(mFadeInSeconds);
		writer.Key("countdownDigitSeconds");
		writer.Write(mCountdownDigitSeconds);
		writer.Key("countdownStartSeconds");
		writer.Write(mCountdownStartSeconds);

		writer.Key("shots");
		writer.BeginArray();
		for (const ShotSpec& shot : mShots) {
			SerializeShot(writer, shot);
		}
		writer.EndArray();

		writer.Key("lockTargets");
		writer.BeginArray();
		for (const LockTargetSpec& spec : mLockTargets) {
			SerializeLockTarget(writer, spec);
		}
		writer.EndArray();
	}

	void GameStartCutsceneComponent::StartSequence() {
		ResolveBindings();
		HideAllCutsceneWidgets();

		mFadeAlpha               = 0.0f;
		mShotIndex               = 0;
		mShotElapsedSeconds      = 0.0f;
		mCountdownElapsedSeconds = 0.0f;
		mShotFadeActive          = false;
		mShotFadeSwapped         = false;
		mLastCountdownCueStep    = -1;
		mGameplayControlReleased = false;
		mSequenceActive          = true;

		// タイトル経由で遷移した場合のみ開始演出を再生します。
		if (!ParkourFlowRuntimeState::ConsumePendingGameStartCutscene()) {
			EnterGameplay();
			return;
		}

		ApplyLockTargets();
		mPhase = PHASE::TOUR;
		if (mShots.empty()) {
			EnterCountdown();
			return;
		}

		if (!SwitchToTourCamera()) {
			EnterCountdown();
			return;
		}
		const ShotSpec& firstShot = mShots[0];
		ApplyTourCameraPose(firstShot.startPos, firstShot.startLook);
		UpdateHudWidgets();
	}

	void GameStartCutsceneComponent::TickTour(const float deltaTime) {
		if (!SwitchToTourCamera()) {
			EnterCountdown();
			return;
		}

		if (!mSkipAction.empty()) {
			if (const InputSystem* input = GetInputSystem()) {
				if (input->IsPressed(mSkipAction)) {
					EnterCountdown();
					return;
				}
			}
		}

		if (mShots.empty()) {
			EnterCountdown();
			return;
		}

		const ShotSpec& shot     = mShots[mShotIndex];
		const float     duration = std::max(kMinDurationSec, shot.durationSeconds);
		mShotElapsedSeconds += deltaTime;

		const float rawT = std::clamp(mShotElapsedSeconds / duration, 0.0f, 1.0f);
		const float easedT = EvaluateEase(rawT);
		const Vec3  cameraPos = Math::Lerp(shot.startPos, shot.endPos, easedT);
		const Vec3  lookAtPos = Math::Lerp(shot.startLook, shot.endLook, easedT);
		ApplyTourCameraPose(cameraPos, lookAtPos);

		const bool hasNextShot = mShotIndex + 1 < mShots.size();
		if (hasNextShot && !mShotFadeSwapped) {
			const float fadeOutDuration = std::max(kMinDurationSec, mFadeOutSeconds);
			const float fadeOutStartSec = std::max(0.0f, duration - fadeOutDuration);
			if (mShotElapsedSeconds >= fadeOutStartSec) {
				const float fadeOutT = std::clamp(
					(mShotElapsedSeconds - fadeOutStartSec) / fadeOutDuration,
					0.0f,
					1.0f
				);
				mShotFadeActive = true;
				mFadeAlpha      = EvaluateEase(fadeOutT);
			} else if (!mShotFadeActive) {
				mFadeAlpha = 0.0f;
			}
		}

		if (rawT >= 1.0f) {
			if (hasNextShot) {
				++mShotIndex;
				mShotElapsedSeconds = 0.0f;
				mShotFadeActive     = true;
				mShotFadeSwapped    = true;
				mFadeAlpha          = 1.0f;

				const ShotSpec& nextShot = mShots[mShotIndex];
				ApplyTourCameraPose(nextShot.startPos, nextShot.startLook);
			} else {
				mFadeAlpha = 0.0f;
				EnterCountdown();
				return;
			}
		}

		if (mShotFadeActive && mShotFadeSwapped) {
			const float fadeInDuration = std::max(kMinDurationSec, mFadeInSeconds);
			const float fadeInT = std::clamp(
				mShotElapsedSeconds / fadeInDuration,
				0.0f,
				1.0f
			);
			mFadeAlpha = 1.0f - EvaluateEase(fadeInT);
			if (fadeInT >= 1.0f) {
				mFadeAlpha       = 0.0f;
				mShotFadeActive  = false;
				mShotFadeSwapped = false;
			}
		}

		UpdateHudWidgets();
	}

	void GameStartCutsceneComponent::TickCountdown(const float deltaTime) {
		(void)SwitchToPlayerCamera();

		mCountdownElapsedSeconds += deltaTime;
		const float digitDuration = std::max(kMinDurationSec, mCountdownDigitSeconds);
		const float digitTotalDuration = digitDuration * 3.0f;
		const float totalDuration =
			digitTotalDuration + std::max(kMinDurationSec, mCountdownStartSeconds);

		// START表示に入るタイミングでプレイヤー操作を解放します。
		if (!mGameplayControlReleased && mCountdownElapsedSeconds >= digitTotalDuration)
		{
			RestoreLockTargets();
			mGameplayControlReleased = true;
		}

		UpdateCountdownAudio();
		UpdateHudWidgets();

		if (mCountdownElapsedSeconds >= totalDuration) {
			EnterGameplay();
		}
	}

	void GameStartCutsceneComponent::EnterCountdown() {
		mPhase                   = PHASE::COUNTDOWN;
		mCountdownElapsedSeconds = 0.0f;
		mShotElapsedSeconds      = 0.0f;
		mFadeAlpha               = 0.0f;
		mShotFadeActive          = false;
		mShotFadeSwapped         = false;
		mLastCountdownCueStep    = -1;
		mGameplayControlReleased = false;

		(void)SwitchToPlayerCamera();
		// 3カウントSEは次Tickで発火し、同フレーム初期化順による取りこぼしを防ぎます。
		UpdateHudWidgets();
	}

	void GameStartCutsceneComponent::EnterGameplay() {
		mPhase                   = PHASE::GAMEPLAY;
		mSequenceActive          = false;
		mFadeAlpha               = 0.0f;
		mGameplayControlReleased = true;

		RestoreLockTargets();
		(void)SwitchToPlayerCamera();
		if (mTourCamera) {
			mTourCamera->SetCameraActive(false);
		}
		ResolveBindings();
		HideAllCutsceneWidgets();
	}

	void GameStartCutsceneComponent::UpdateCountdownAudio() {
		const float digitDuration = std::max(kMinDurationSec, mCountdownDigitSeconds);
		const float digitTotalDuration = digitDuration * 3.0f;
		const float totalDuration =
			digitTotalDuration + std::max(kMinDurationSec, mCountdownStartSeconds);

		int cueStep = -1;
		if (mCountdownElapsedSeconds < digitTotalDuration) {
			const int digit =
				3 - static_cast<int>(mCountdownElapsedSeconds / digitDuration);
			cueStep = std::clamp(digit, 1, 3);
		} else if (mCountdownElapsedSeconds < totalDuration) {
			cueStep = 0;
		}

		if (cueStep == mLastCountdownCueStep) {
			return;
		}
		mLastCountdownCueStep = cueStep;

		if (cueStep >= 1) {
			if (mCountAudioSource) {
				mCountAudioSource->Play();
			}
		} else if (cueStep == 0) {
			if (mStartAudioSource) {
				mStartAudioSource->Play();
			}
		}
	}

	void GameStartCutsceneComponent::UpdateHudWidgets() {
		ResolveBindings();
		if (!mHudCanvas || !mHudCanvas->EnsureRuntimeLoaded()) {
			return;
		}

		UpdateFadeOverlayWidget();
		if (mPhase == PHASE::COUNTDOWN) {
			UpdateCountdownWidgets();
		} else {
			HideCountdownWidgets();
		}
	}

	void GameStartCutsceneComponent::UpdateFadeOverlayWidget() const {
		if (!mFadeOverlayWidget || !mFadeOverlayTexture) {
			return;
		}

		const float alpha   = std::clamp(mFadeAlpha, 0.0f, 1.0f);
		const bool  visible = alpha > 0.001f && mPhase != PHASE::GAMEPLAY;
		mFadeOverlayWidget->SetVisible(visible);

		Gui::Color color = mFadeOverlayTexture->GetColor();
		color.r          = 0.0f;
		color.g          = 0.0f;
		color.b          = 0.0f;
		color.a          = visible ? alpha : 0.0f;
		mFadeOverlayTexture->SetColor(color);
		mFadeOverlayWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
	}

	void GameStartCutsceneComponent::UpdateCountdownWidgets() const {
		if (!mCountdownDigitWidget || !mCountdownDigitTransform ||
		    !mCountdownDigitTexture || !mCountdownStartWidget ||
		    !mCountdownStartTransform || !mCountdownStartTexture) {
			return;
		}

		Vec2 viewportSize = Vec2::zero;
		if (!ResolveViewportSize(viewportSize)) {
			return;
		}

		const float viewW         = std::max(1.0f, viewportSize.x);
		const float viewH         = std::max(1.0f, viewportSize.y);
		const float centerOffsetY = viewH * 0.44f - viewH * 0.5f;

		const Gui::Rect digitBaseRect   = mCountdownDigitTransform->GetRect();
		const Gui::Rect startBaseRect   = mCountdownStartTransform->GetRect();
		const float     digitBaseWidth  = std::max(1.0f, digitBaseRect.width);
		const float     digitBaseHeight = std::max(1.0f, digitBaseRect.height);

		const float digitTarget =
			std::clamp(std::min(viewW, viewH) * 0.24f, 96.0f, 320.0f);
		const float digitScale =
			digitTarget / std::max(digitBaseWidth, digitBaseHeight);

		const float startBaseWidth  = std::max(1.0f, startBaseRect.width);
		const float startBaseHeight = std::max(1.0f, startBaseRect.height);
		const float startScale =
			std::clamp(viewW * 0.42f / startBaseWidth, 0.45f, 1.45f);
		float startWidth      = startBaseWidth * startScale;
		float startHeight     = startBaseHeight * startScale;
		const float startOffsetY = centerOffsetY + digitTarget * 0.05f;

		const float digitDuration      = std::max(kMinDurationSec, mCountdownDigitSeconds);
		const float digitTotalDuration = digitDuration * 3.0f;
		if (mCountdownElapsedSeconds < digitTotalDuration) {
			const int digit =
				3 - static_cast<int>(mCountdownElapsedSeconds / digitDuration);
			const float phase = std::clamp(
				std::fmod(mCountdownElapsedSeconds, digitDuration) / digitDuration,
				0.0f,
				1.0f
			);
			const float pulseScale = 1.0f + (1.0f - phase) * 0.24f;
			const float alpha      = std::clamp(1.0f - phase * 0.75f, 0.0f, 1.0f);

			mCountdownDigitWidget->SetVisible(true);
			mCountdownDigitTransform->SetAnchors(
				Gui::Anchors{.minX = 0.5f, .minY = 0.5f, .maxX = 0.5f, .maxY = 0.5f}
			);
			mCountdownDigitTransform->SetMargins(Gui::Margins{});
			mCountdownDigitTransform->SetPivot(Gui::Pivot{.x = 0.5f, .y = 0.5f});
			mCountdownDigitTransform->SetRect(
				Gui::Rect{
					.x      = 0.0f,
					.y      = centerOffsetY,
					.width  = digitBaseWidth * digitScale * pulseScale,
					.height = digitBaseHeight * digitScale * pulseScale,
				}
			);

			const float clampedDigit = std::clamp(static_cast<float>(digit), 0.0f, 9.0f);
			const float uvMinX       = clampedDigit * 0.1f;
			mCountdownDigitTexture->SetUvMin(Vec2(uvMinX, 0.0f));
			mCountdownDigitTexture->SetUvMax(Vec2(uvMinX + 0.1f, 1.0f));
			Gui::Color digitColor = mCountdownDigitTexture->GetColor();
			digitColor.a          = alpha;
			mCountdownDigitTexture->SetColor(digitColor);
			mCountdownDigitWidget->MarkDirty(
				Gui::DIRTY_FLAGS::LAYOUT | Gui::DIRTY_FLAGS::DRAW
			);

			mCountdownStartWidget->SetVisible(false);
			Gui::Color startColor = mCountdownStartTexture->GetColor();
			startColor.a          = 0.0f;
			mCountdownStartTexture->SetColor(startColor);
			mCountdownStartWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			return;
		}

		const float startDuration = std::max(kMinDurationSec, mCountdownStartSeconds);
		const float phase = std::clamp(
			(mCountdownElapsedSeconds - digitTotalDuration) / startDuration,
			0.0f,
			1.0f
		);
		const float alpha = std::sin(phase * Math::pi);
		const float pulse = 1.0f + 0.08f * std::sin(phase * Math::pi);
		startWidth *= pulse;
		startHeight *= pulse;

		mCountdownDigitWidget->SetVisible(false);
		Gui::Color digitColor = mCountdownDigitTexture->GetColor();
		digitColor.a          = 0.0f;
		mCountdownDigitTexture->SetColor(digitColor);
		mCountdownDigitWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);

		mCountdownStartWidget->SetVisible(true);
		mCountdownStartTransform->SetAnchors(
			Gui::Anchors{.minX = 0.5f, .minY = 0.5f, .maxX = 0.5f, .maxY = 0.5f}
		);
		mCountdownStartTransform->SetMargins(Gui::Margins{});
		mCountdownStartTransform->SetPivot(Gui::Pivot{.x = 0.5f, .y = 0.5f});
		mCountdownStartTransform->SetRect(
			Gui::Rect{
				.x      = 0.0f,
				.y      = startOffsetY,
				.width  = startWidth,
				.height = startHeight,
			}
		);
		Gui::Color startColor = mCountdownStartTexture->GetColor();
		startColor.a          = std::clamp(alpha, 0.0f, 1.0f);
		mCountdownStartTexture->SetColor(startColor);
		mCountdownStartWidget->MarkDirty(
			Gui::DIRTY_FLAGS::LAYOUT | Gui::DIRTY_FLAGS::DRAW
		);
	}

	void GameStartCutsceneComponent::HideCountdownWidgets() const {
		if (mCountdownDigitWidget) {
			mCountdownDigitWidget->SetVisible(false);
			if (mCountdownDigitTexture) {
				Gui::Color color = mCountdownDigitTexture->GetColor();
				color.a          = 0.0f;
				mCountdownDigitTexture->SetColor(color);
			}
			mCountdownDigitWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
		if (mCountdownStartWidget) {
			mCountdownStartWidget->SetVisible(false);
			if (mCountdownStartTexture) {
				Gui::Color color = mCountdownStartTexture->GetColor();
				color.a          = 0.0f;
				mCountdownStartTexture->SetColor(color);
			}
			mCountdownStartWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
	}

	void GameStartCutsceneComponent::HideAllCutsceneWidgets() const {
		HideCountdownWidgets();
		if (mFadeOverlayWidget) {
			mFadeOverlayWidget->SetVisible(false);
			if (mFadeOverlayTexture) {
				Gui::Color color = mFadeOverlayTexture->GetColor();
				color.a          = 0.0f;
				mFadeOverlayTexture->SetColor(color);
			}
			mFadeOverlayWidget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
		}
	}

	void GameStartCutsceneComponent::ClearResolvedBindings() {
		mTourCameraEntity    = nullptr;
		mTourCameraTransform = nullptr;
		mTourCamera          = nullptr;
		mPlayerCameraEntity  = nullptr;
		mPlayerCamera        = nullptr;
		mHudCanvas           = nullptr;
		mCountAudioSource    = nullptr;
		mStartAudioSource    = nullptr;

		mCountdownDigitWidget    = nullptr;
		mCountdownDigitTransform = nullptr;
		mCountdownDigitTexture   = nullptr;
		mCountdownStartWidget    = nullptr;
		mCountdownStartTransform = nullptr;
		mCountdownStartTexture   = nullptr;
		mFadeOverlayWidget       = nullptr;
		mFadeOverlayTexture      = nullptr;
	}

	void GameStartCutsceneComponent::ResolveBindings() {
		ClearResolvedBindings();

		Scene* scene = GetScene();
		if (!scene) {
			return;
		}

		if (mTourCameraEntityGuid != 0) {
			mTourCameraEntity = scene->FindEntity(mTourCameraEntityGuid);
		}
		if (mTourCameraEntity) {
			mTourCameraTransform =
				mTourCameraEntity->GetComponent<TransformComponent>();
			mTourCamera = mTourCameraEntity->GetComponent<CameraComponent>();
		}

		if (mPlayerCameraEntityGuid != 0) {
			mPlayerCameraEntity = scene->FindEntity(mPlayerCameraEntityGuid);
		}
		if (mPlayerCameraEntity) {
			mPlayerCamera = mPlayerCameraEntity->GetComponent<CameraComponent>();
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
				mCountdownDigitWidget = FindWidgetByNameRecursive(
					rootWidget,
					mCountdownDigitWidgetName
				);
				mCountdownStartWidget = FindWidgetByNameRecursive(
					rootWidget,
					mCountdownStartWidgetName
				);
				mFadeOverlayWidget =
					FindWidgetByNameRecursive(rootWidget, mFadeOverlayWidgetName);

				if (mCountdownDigitWidget) {
					mCountdownDigitTransform =
						mCountdownDigitWidget
							->GetOrAddComponent<Gui::UiTransformComponent>();
					mCountdownDigitTexture =
						mCountdownDigitWidget
							->GetOrAddComponent<Gui::UiTextureComponent>();
				}
				if (mCountdownStartWidget) {
					mCountdownStartTransform =
						mCountdownStartWidget
							->GetOrAddComponent<Gui::UiTransformComponent>();
					mCountdownStartTexture =
						mCountdownStartWidget
							->GetOrAddComponent<Gui::UiTextureComponent>();
				}
				if (mFadeOverlayWidget) {
					mFadeOverlayTexture =
						mFadeOverlayWidget
							->GetOrAddComponent<Gui::UiTextureComponent>();
				}
			}
		}

		mCountAudioSource = ResolveAudioSourceByGuid(mCountAudioSourceGuid);
		mStartAudioSource = ResolveAudioSourceByGuid(mStartAudioSourceGuid);
	}

	void GameStartCutsceneComponent::ApplyLockTargets() {
		RestoreLockTargets();
		std::vector<BaseComponent*> lockedComponents = {};
		std::vector<LockTargetSpec> effectiveLockTargets = mLockTargets;
		effectiveLockTargets.reserve(mLockTargets.size() + 1);

		// 視点入力漏れ防止のため、CameraRotator は未指定でも既定でロック対象に含めます。
		const bool hasDefaultLookLock = std::ranges::any_of(
			effectiveLockTargets,
			[](const LockTargetSpec& spec) {
				return spec.componentStableName == kDefaultLookLockStableName;
			}
		);
		if (!hasDefaultLookLock) {
			effectiveLockTargets.emplace_back(
				LockTargetSpec{
					.entityGuid = 0,
					.componentStableName = std::string(kDefaultLookLockStableName),
				}
			);
		}

		lockedComponents.reserve(effectiveLockTargets.size());
		for (const LockTargetSpec& spec : effectiveLockTargets) {
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

	void GameStartCutsceneComponent::RestoreLockTargets() {
		for (const ActiveLockState& state : mActiveLocks) {
			if (!state.component) {
				continue;
			}
			state.component->SetActive(state.previousActive);
		}
		mActiveLocks.clear();
	}

	void GameStartCutsceneComponent::ApplyTourCameraPose(
		const Vec3& cameraPos,
		const Vec3& lookAtPos
	) {
		if (!mTourCameraTransform) {
			return;
		}

		mTourCameraTransform->SetPosition(cameraPos);

		Vec3 viewDir = lookAtPos - cameraPos;
		if (viewDir.SqrLength() <= 1.0e-8f) {
			return;
		}
		viewDir.Normalize();

		const Quaternion lookRotation = Quaternion::LookRotation(viewDir, Vec3::up);
		mTourCameraTransform->SetRotation(lookRotation);
		mTourCameraTransform->RequestInterpolationResync();
	}

	bool GameStartCutsceneComponent::SwitchToTourCamera() {
		World* world = GetWorld();
		if (!world || !mTourCameraEntity || !mTourCamera) {
			return false;
		}

		// ツアー中はプレイヤーカメラを無効化して、外部からの切替上書きを防ぎます。
		if (mPlayerCamera) {
			mPlayerCamera->SetCameraActive(false);
		}
		mTourCamera->SetCameraActive(true);
		return world->GetCameraManager().SetCurrentCamera(mTourCameraEntity->GetGuid());
	}

	bool GameStartCutsceneComponent::SwitchToPlayerCamera() {
		World* world = GetWorld();
		if (!world || !mPlayerCameraEntity || !mPlayerCamera) {
			return false;
		}

		if (mTourCamera) {
			mTourCamera->SetCameraActive(false);
		}
		mPlayerCamera->SetCameraActive(true);
		return world->GetCameraManager().SetCurrentCamera(mPlayerCameraEntity->GetGuid());
	}

	BaseComponent* GameStartCutsceneComponent::ResolveLockTarget(
		const LockTargetSpec& spec
	) const {
		Scene* scene = GetScene();
		if (!scene || spec.componentStableName.empty()) {
			return nullptr;
		}

		const auto matches = [&](BaseComponent& component) {
			return component.GetStableName() == spec.componentStableName;
		};
		const auto findInEntity = [&](Entity& entity) -> BaseComponent* {
			BaseComponent* found = nullptr;
			entity.ForEachComponent(
				[&](BaseComponent& component) {
					if (!matches(component)) {
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
			if (!entityPtr) {
				continue;
			}
			if (BaseComponent* found = findInEntity(*entityPtr)) {
				return found;
			}
		}
		return nullptr;
	}

	AudioSourceComponent* GameStartCutsceneComponent::ResolveAudioSourceByGuid(
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

	bool GameStartCutsceneComponent::ResolveViewportSize(Vec2& outViewportSizePx)
		const {
		outViewportSizePx = Vec2::zero;
		if (const InputSystem* input = GetInputSystem()) {
			outViewportSizePx = input->GetMouseClientViewportSize();
		}
		if (outViewportSizePx.x <= 1.0f || outViewportSizePx.y <= 1.0f) {
			outViewportSizePx = mHudCanvas ? mHudCanvas->GetPixelSize() : Vec2::zero;
		}
		return outViewportSizePx.x > 1.0f && outViewportSizePx.y > 1.0f;
	}

	Gui::UiWidget* GameStartCutsceneComponent::FindWidgetByNameRecursive(
		Gui::UiWidget*        root,
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
			if (Gui::UiWidget* found =
				FindWidgetByNameRecursive(child.get(), widgetName)) {
				return found;
			}
		}
		for (Gui::UiWidget* child : root->GetReferenceChildren()) {
			if (Gui::UiWidget* found =
				FindWidgetByNameRecursive(child, widgetName)) {
				return found;
			}
		}
		return nullptr;
	}

	void GameStartCutsceneComponent::SerializeLockTarget(
		JsonWriter&           writer,
		const LockTargetSpec& spec
	) {
		writer.BeginObject();
		writer.Key("entityGuid");
		writer.Write(spec.entityGuid);
		writer.Key("componentStableName");
		writer.Write(spec.componentStableName);
		writer.EndObject();
	}

	void GameStartCutsceneComponent::SerializeShot(
		JsonWriter&     writer,
		const ShotSpec& shot
	) {
		writer.BeginObject();
		writer.WriteVec3("startPos", shot.startPos);
		writer.WriteVec3("endPos", shot.endPos);
		writer.WriteVec3("startLook", shot.startLook);
		writer.WriteVec3("endLook", shot.endLook);
		writer.Key("durationSeconds");
		writer.Write(shot.durationSeconds);
		writer.EndObject();
	}

	float GameStartCutsceneComponent::EvaluateEase(const float t) {
		return Math::CubicBezier(
			std::clamp(t, 0.0f, 1.0f),
			0.2f,
			0.0f,
			0.0f,
			1.0f
		);
	}

	REGISTER_COMPONENT(GameStartCutsceneComponent);
}
