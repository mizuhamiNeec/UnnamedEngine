#include "CourseProgressHudComponent.h"

#include <algorithm>
#include <array>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/game/GamePathResolver.h"
#include "engine/game/GameRuntimeContext.h"
#include "engine/game/IGameModule.h"
#include "engine/gui/Rect.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiDigitStripComponent.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/gui/components/UiTransformComponent.h"
#include "engine/ImGui/Icons.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

#include "CourseElapsedTimeFormat.h"
#include "CourseHudProjection.h"
#include "CourseProgressComponent.h"

namespace Unnamed {
	namespace {
#ifdef _DEBUG
		template <size_t N>
		void DrawStringInput(
			const char* label,
			std::string& value
		) {
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

		[[nodiscard]] bool IsEngineRootRelativePath(const std::string_view path) {
			return path.starts_with("content/") ||
			       path.starts_with("projects/");
		}

		[[nodiscard]] std::string ResolveHudContentPath(
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

			if (const GameRuntimeContext* runtimeContext =
				ServiceLocator::Get<GameRuntimeContext>()) {
				return ResolveGameContentPath(
					runtimeContext->modulePaths,
					effectivePath
				);
			}
			if (const IGameModule* gameModule = ServiceLocator::Get<IGameModule>()) {
				return ResolveGameContentPath(
					gameModule->GetGameModulePaths(),
					effectivePath
				);
			}
			return effectivePath;
		}
	}

	void CourseProgressHudComponent::OnAttached() {
		ResolveBindings();
	}

	void CourseProgressHudComponent::OnTick(const float deltaTime) {
		(void)deltaTime;
		TickHud();
	}

	void CourseProgressHudComponent::OnEditorTick(const float deltaTime) {
		(void)deltaTime;
		TickHud();
	}

	std::string_view CourseProgressHudComponent::GetStableName() const {
		return "parkour.CourseProgressHud";
	}

	std::string_view CourseProgressHudComponent::GetComponentName() const {
		return "CourseProgressHud";
	}

	uint32_t CourseProgressHudComponent::GetIcon() const {
		return kIconMonitor;
	}

#ifdef _DEBUG
	void CourseProgressHudComponent::DrawInspectorImGui() {
		DrawStringInput<64>("Course Id", mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		ImGui::Checkbox("Require Course HUD Enabled", &mRequireCourseHudEnabled);
		ImGui::DragFloat(
			"Screen Clamp Margin Px",
			&mScreenClampMarginPx,
			1.0f,
			0.0f,
			1024.0f
		);
		ImGui::DragFloat("Pin Size Px", &mPinSizePx, 0.5f, 1.0f, 512.0f);
		ImGui::DragFloat("Arrow Size Px", &mArrowSizePx, 0.5f, 1.0f, 512.0f);
		DrawStringInput<64>("Pin Widget Name", mPinWidgetName);
		DrawStringInput<64>("Arrow Widget Name", mArrowWidgetName);
		DrawStringInput<64>("Elapsed Minutes Widget", mElapsedMinutesWidgetName);
		DrawStringInput<64>("Elapsed Seconds Widget", mElapsedSecondsWidgetName);
		DrawStringInput<64>("Elapsed Fraction Widget", mElapsedFractionWidgetName);
		DrawStringInput<64>("Elapsed Comma Widget", mElapsedCommaWidgetName);
		DrawStringInput<64>("Elapsed Dot Widget", mElapsedDotWidgetName);
		DrawStringInput<128>("Pin Texture Path", mPinTexturePath);
		DrawStringInput<128>("Arrow Texture Path", mArrowTexturePath);
		DrawStringInput<128>("Digit Texture Path", mDigitTexturePath);
		DrawStringInput<128>("Comma Texture Path", mCommaTexturePath);
		DrawStringInput<128>("Dot Texture Path", mDotTexturePath);
		ImGui::DragFloat(
			"Elapsed Text Alpha",
			&mElapsedTextAlpha,
			0.01f,
			0.0f,
			1.0f
		);
	}
#endif

	void CourseProgressHudComponent::Deserialize(const JsonReader& reader) {
		mCourseId = reader["courseId"].GetString(mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		if (const JsonReader requireHud = reader["requireCourseHudEnabled"];
			requireHud.Valid()) {
			mRequireCourseHudEnabled = requireHud.GetBool(mRequireCourseHudEnabled);
		}
		if (const JsonReader margin = reader["screenClampMarginPx"]; margin.Valid()) {
			mScreenClampMarginPx = std::max(0.0f, margin.GetFloat(mScreenClampMarginPx));
		}
		if (const JsonReader pinSize = reader["pinSizePx"]; pinSize.Valid()) {
			mPinSizePx = std::max(1.0f, pinSize.GetFloat(mPinSizePx));
		}
		if (const JsonReader arrowSize = reader["arrowSizePx"]; arrowSize.Valid()) {
			mArrowSizePx = std::max(1.0f, arrowSize.GetFloat(mArrowSizePx));
		}
		mPinWidgetName   = reader["pinWidgetName"].GetString(mPinWidgetName);
		mArrowWidgetName = reader["arrowWidgetName"].GetString(mArrowWidgetName);
		mElapsedMinutesWidgetName =
			reader["elapsedMinutesWidgetName"].GetString(mElapsedMinutesWidgetName);
		mElapsedSecondsWidgetName =
			reader["elapsedSecondsWidgetName"].GetString(mElapsedSecondsWidgetName);
		mElapsedFractionWidgetName = reader["elapsedFractionWidgetName"].GetString(
			mElapsedFractionWidgetName
		);
		mElapsedCommaWidgetName =
			reader["elapsedCommaWidgetName"].GetString(mElapsedCommaWidgetName);
		mElapsedDotWidgetName =
			reader["elapsedDotWidgetName"].GetString(mElapsedDotWidgetName);
		mPinTexturePath  = reader["pinTexturePath"].GetString(mPinTexturePath);
		mArrowTexturePath = reader["arrowTexturePath"].GetString(mArrowTexturePath);
		mDigitTexturePath =
			reader["digitTexturePath"].GetString(mDigitTexturePath);
		mCommaTexturePath =
			reader["commaTexturePath"].GetString(mCommaTexturePath);
		mDotTexturePath = reader["dotTexturePath"].GetString(mDotTexturePath);
		if (const JsonReader elapsedTextAlpha = reader["elapsedTextAlpha"];
			elapsedTextAlpha.Valid()) {
			mElapsedTextAlpha = std::clamp(
				elapsedTextAlpha.GetFloat(mElapsedTextAlpha),
				0.0f,
				1.0f
			);
		}
	}

	void CourseProgressHudComponent::Serialize(JsonWriter& writer) const {
		writer.Key("courseId");
		writer.Write(mCourseId);
		writer.Key("requireCourseHudEnabled");
		writer.Write(mRequireCourseHudEnabled);
		writer.Key("screenClampMarginPx");
		writer.Write(mScreenClampMarginPx);
		writer.Key("pinSizePx");
		writer.Write(mPinSizePx);
		writer.Key("arrowSizePx");
		writer.Write(mArrowSizePx);
		writer.Key("pinWidgetName");
		writer.Write(mPinWidgetName);
		writer.Key("arrowWidgetName");
		writer.Write(mArrowWidgetName);
		writer.Key("elapsedMinutesWidgetName");
		writer.Write(mElapsedMinutesWidgetName);
		writer.Key("elapsedSecondsWidgetName");
		writer.Write(mElapsedSecondsWidgetName);
		writer.Key("elapsedFractionWidgetName");
		writer.Write(mElapsedFractionWidgetName);
		writer.Key("elapsedCommaWidgetName");
		writer.Write(mElapsedCommaWidgetName);
		writer.Key("elapsedDotWidgetName");
		writer.Write(mElapsedDotWidgetName);
		writer.Key("pinTexturePath");
		writer.Write(mPinTexturePath);
		writer.Key("arrowTexturePath");
		writer.Write(mArrowTexturePath);
		writer.Key("digitTexturePath");
		writer.Write(mDigitTexturePath);
		writer.Key("commaTexturePath");
		writer.Write(mCommaTexturePath);
		writer.Key("dotTexturePath");
		writer.Write(mDotTexturePath);
		writer.Key("elapsedTextAlpha");
		writer.Write(mElapsedTextAlpha);
	}

	void CourseProgressHudComponent::TickHud() {
		ResolveBindings();
		if (!mUiCanvas || !mUiCanvas->EnsureRuntimeLoaded()) {
			return;
		}

		const Gui::UiRoot* runtimeRoot = mUiCanvas->GetRuntimeRoot();
		Gui::UiWidget* rootWidget = runtimeRoot ? runtimeRoot->GetRootWidget() : nullptr;
		if (!rootWidget) {
			return;
		}

		Gui::UiWidget* pinWidget = FindWidgetByNameRecursive(
			rootWidget,
			mPinWidgetName
		);
		Gui::UiWidget* arrowWidget = FindWidgetByNameRecursive(
			rootWidget,
			mArrowWidgetName
		);
		Gui::UiWidget* elapsedMinutesWidget = FindWidgetByNameRecursive(
			rootWidget,
			mElapsedMinutesWidgetName
		);
		Gui::UiWidget* elapsedSecondsWidget = FindWidgetByNameRecursive(
			rootWidget,
			mElapsedSecondsWidgetName
		);
		Gui::UiWidget* elapsedFractionWidget = FindWidgetByNameRecursive(
			rootWidget,
			mElapsedFractionWidgetName
		);
		Gui::UiWidget* elapsedCommaWidget = FindWidgetByNameRecursive(
			rootWidget,
			mElapsedCommaWidgetName
		);
		Gui::UiWidget* elapsedDotWidget = FindWidgetByNameRecursive(
			rootWidget,
			mElapsedDotWidgetName
		);
		auto* elapsedMinutes = elapsedMinutesWidget ?
			                       elapsedMinutesWidget->GetOrAddComponent<
				                       Gui::UiDigitStripComponent>() :
			                       nullptr;
		auto* elapsedSeconds = elapsedSecondsWidget ?
			                       elapsedSecondsWidget->GetOrAddComponent<
				                       Gui::UiDigitStripComponent>() :
			                       nullptr;
		auto* elapsedFraction = elapsedFractionWidget ?
			                        elapsedFractionWidget->GetOrAddComponent<
				                        Gui::UiDigitStripComponent>() :
			                        nullptr;
		auto* elapsedComma = elapsedCommaWidget ?
			                     elapsedCommaWidget->GetOrAddComponent<
				                     Gui::UiTextureComponent>() :
			                     nullptr;
		auto* elapsedDot = elapsedDotWidget ?
			                   elapsedDotWidget->GetOrAddComponent<
				                   Gui::UiTextureComponent>() :
			                   nullptr;
		const auto UpdateElapsedTimeWidgets = [&](const bool visible,
		                                          const float elapsedSecondsValue) {
			const auto HideWidget = [](Gui::UiWidget* widget) {
				if (!widget) {
					return;
				}
				widget->SetVisible(false);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			if (!(elapsedMinutes && elapsedSeconds && elapsedFraction && elapsedComma &&
			      elapsedDot)) {
				HideWidget(elapsedMinutesWidget);
				HideWidget(elapsedSecondsWidget);
				HideWidget(elapsedFractionWidget);
				HideWidget(elapsedCommaWidget);
				HideWidget(elapsedDotWidget);
				return;
			}

			const float alpha = std::clamp(mElapsedTextAlpha, 0.0f, 1.0f);
			if (!visible) {
				HideWidget(elapsedMinutesWidget);
				HideWidget(elapsedSecondsWidget);
				HideWidget(elapsedFractionWidget);
				HideWidget(elapsedCommaWidget);
				HideWidget(elapsedDotWidget);
				return;
			}

			const CourseElapsedTimeParts time =
				SplitCourseElapsedTime(elapsedSecondsValue);
			const auto ApplyDigits = [&](Gui::UiWidget* widget,
			                             Gui::UiDigitStripComponent* strip,
			                             const int value) {
				if (!widget || !strip) {
					return;
				}
				widget->SetVisible(true);
				strip->SetStripTexturePath(
					ResolveHudContentPath(mDigitTexturePath, "textures/digits.png")
				);
				strip->SetMinDigits(2);
				strip->SetValue(value);
				Gui::Color color = strip->GetColor();
				color.a          = alpha;
				strip->SetColor(color);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			ApplyDigits(elapsedMinutesWidget, elapsedMinutes, time.minutes);
			ApplyDigits(elapsedSecondsWidget, elapsedSeconds, time.seconds);
			ApplyDigits(elapsedFractionWidget, elapsedFraction, time.fraction);

			const auto ApplySeparator = [&](Gui::UiWidget* widget,
			                                Gui::UiTextureComponent* texture,
			                                const std::string_view path,
			                                const std::string_view fallbackPath) {
				if (!widget || !texture) {
					return;
				}
				widget->SetVisible(true);
				texture->SetTexturePath(ResolveHudContentPath(path, fallbackPath));
				Gui::Color color = texture->GetColor();
				color.a          = alpha;
				texture->SetColor(color);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			ApplySeparator(
				elapsedCommaWidget,
				elapsedComma,
				mCommaTexturePath,
				"textures/colon.png"
			);
			ApplySeparator(
				elapsedDotWidget,
				elapsedDot,
				mDotTexturePath,
				"textures/dot.png"
			);
		};
		const bool hasGuideWidgets = pinWidget && arrowWidget;

		Gui::UiTransformComponent* pinTransform = hasGuideWidgets ?
			                                        pinWidget
				                                        ->GetComponent<
					                                        Gui::
						                                        UiTransformComponent>() :
			                                        nullptr;
		Gui::UiTransformComponent* arrowTransform = hasGuideWidgets ?
			                                          arrowWidget
				                                          ->GetComponent<
					                                          Gui::
						                                          UiTransformComponent>() :
			                                          nullptr;
		Gui::UiTextureComponent* pinTexture = hasGuideWidgets ?
			                                    pinWidget
				                                    ->GetOrAddComponent<
					                                    Gui::
						                                    UiTextureComponent>() :
			                                    nullptr;
		Gui::UiTextureComponent* arrowTexture = hasGuideWidgets ?
			                                      arrowWidget
				                                      ->GetOrAddComponent<
					                                      Gui::
						                                      UiTextureComponent>() :
			                                      nullptr;
		const bool canDrawGuides =
			hasGuideWidgets &&
			pinTransform &&
			arrowTransform &&
			pinTexture &&
			arrowTexture;
		if (canDrawGuides) {
			pinTexture->SetTexturePath(
				ResolveHudContentPath(mPinTexturePath, "textures/ping.png")
			);
			arrowTexture->SetTexturePath(
				ResolveHudContentPath(mArrowTexturePath, "textures/arrow.png")
			);
		}

		const auto HideBoth = [&]() {
			UpdateGuideWidget(
				pinWidget,
				pinTransform,
				pinTexture,
				Vec2::zero,
				mPinSizePx,
				0.0f,
				0.0f,
				false
			);
			UpdateGuideWidget(
				arrowWidget,
				arrowTransform,
				arrowTexture,
				Vec2::zero,
				mArrowSizePx,
				0.0f,
				0.0f,
				false
			);
		};

		if (!mCourseProgress || !mCourseProgress->IsActive()) {
			UpdateElapsedTimeWidgets(false, 0.0f);
			HideBoth();
			return;
		}
		if (mRequireCourseHudEnabled && !mCourseProgress->IsHudEnabled()) {
			UpdateElapsedTimeWidgets(false, 0.0f);
			HideBoth();
			return;
		}

		const CourseProgressSnapshot& snapshot = mCourseProgress->GetSnapshot();
		UpdateElapsedTimeWidgets(!snapshot.courseCleared, snapshot.elapsedSeconds);
		if (!canDrawGuides) {
			HideBoth();
			return;
		}
		if (snapshot.courseCleared || !snapshot.hasNextTarget) {
			HideBoth();
			return;
		}

		World* world = GetWorld();
		if (!world) {
			HideBoth();
			return;
		}
		const auto cameraInfo = world->GetCameraManager().GetCurrentCameraInfo();
		if (!cameraInfo.valid) {
			HideBoth();
			return;
		}

		Vec2 viewportSizePx = Vec2::zero;
		if (!ResolveViewportSize(viewportSizePx)) {
			HideBoth();
			return;
		}

		CourseHudProjectionResult projection = {};
		if (!BuildCourseHudProjection(
			snapshot.nextTargetWorldPosition,
			cameraInfo.camera,
			viewportSizePx,
			mScreenClampMarginPx,
			projection
		)) {
			HideBoth();
			return;
		}

		UpdateGuideWidget(
			pinWidget,
			pinTransform,
			pinTexture,
			projection.screenPositionPx,
			mPinSizePx,
			projection.alpha,
			0.0f,
			true
		);
		UpdateGuideWidget(
			arrowWidget,
			arrowTransform,
			arrowTexture,
			projection.screenPositionPx,
			mArrowSizePx,
			0.95f,
			projection.arrowRotationRad,
			projection.outOfScreen
		);
	}

	void CourseProgressHudComponent::ResolveBindings() {
		mCourseProgress = nullptr;
		mUiCanvas       = nullptr;

		Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		mUiCanvas = owner->GetComponent<UiCanvasComponent>();

		const std::string normalizedCourseId = mCourseId.empty() ?
			                                       std::string("default") :
			                                       mCourseId;
		owner->ForEachComponent(
			[&](BaseComponent& component) {
				auto* progress = dynamic_cast<CourseProgressComponent*>(&component);
				if (!progress || !progress->IsActive()) {
					return true;
				}
				if (progress->GetCourseId() != normalizedCourseId) {
					return true;
				}
				mCourseProgress = progress;
				return false;
			}
		);
	}

	Gui::UiWidget* CourseProgressHudComponent::FindWidgetByNameRecursive(
		Gui::UiWidget*       root,
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

	bool CourseProgressHudComponent::ResolveViewportSize(
		Vec2& outViewportSizePx
	) const {
		outViewportSizePx = Vec2::zero;
		if (const InputSystem* input = GetInputSystem()) {
			outViewportSizePx = input->GetMouseClientViewportSize();
		}
		if (outViewportSizePx.x <= 1.0f || outViewportSizePx.y <= 1.0f) {
			outViewportSizePx = mUiCanvas ? mUiCanvas->GetPixelSize() : Vec2::zero;
		}
		return outViewportSizePx.x > 1.0f && outViewportSizePx.y > 1.0f;
	}

	void CourseProgressHudComponent::UpdateGuideWidget(
		Gui::UiWidget*             widget,
		Gui::UiTransformComponent* transform,
		Gui::UiTextureComponent*   texture,
		const Vec2&                centerPx,
		const float                sizePx,
		const float                alpha,
		const float                rotationRad,
		const bool                 visible
	) {
		if (!widget || !transform || !texture) {
			return;
		}

		widget->SetVisible(visible);
		if (!visible) {
			widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			return;
		}

		// 絶対ピクセル指定で追従させるため、原点アンカー + 中心ピボットに揃えます。
		// 投影座標は画面全体基準なので、親基準のローカル座標へ変換します。
		Vec2 localCenterPx = centerPx;
		if (const Gui::UiWidget* parent = widget->GetParent()) {
			const Gui::Rect& parentRect = parent->GetGlobalRect();
			localCenterPx.x -= parentRect.x;
			localCenterPx.y -= parentRect.y;
		}

		transform->SetAnchors(Gui::Anchors{});
		transform->SetMargins(Gui::Margins{});
		transform->SetPivot(Gui::Pivot{.x = 0.5f, .y = 0.5f});
		transform->SetRect(
			Gui::Rect{
				.x = localCenterPx.x,
				.y = localCenterPx.y,
				.width = sizePx,
				.height = sizePx
			}
		);

		Gui::Color color = texture->GetColor();
		color.a          = std::clamp(alpha, 0.0f, 1.0f);
		texture->SetColor(color);
		texture->SetAnchor(Vec2(0.5f, 0.5f));
		texture->SetRotationRad(rotationRad);
		widget->MarkDirty(Gui::DIRTY_FLAGS::LAYOUT | Gui::DIRTY_FLAGS::DRAW);
	}

	REGISTER_COMPONENT(CourseProgressHudComponent);
}

