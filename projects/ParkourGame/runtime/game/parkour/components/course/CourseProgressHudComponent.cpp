#include "CourseProgressHudComponent.h"

#include <algorithm>

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
#include <imgui.h>
#include "engine/ImGui/ImGuiWidgets.h"
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

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
#include "engine/world/World.h"

#include "CourseElapsedTimeFormat.h"
#include "CourseHudProjection.h"
#include "CourseProgressComponent.h"

namespace Unnamed {
	namespace {
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		template <size_t N>
		void DrawPathInput(
			const char* label,
			Path&       value
		) {
			std::string pathText = value.ToGenericUtf8();
			if (ImGuiWidgets::InputText<N>(label, pathText)) {
				value = Path(pathText);
			}
		}
#endif
	}

	void CourseProgressHudComponent::OnAttached() {
		ResolveBindings();
	}

	void CourseProgressHudComponent::OnFrameInputTick(float) {
		TickHud();
	}

	void CourseProgressHudComponent::OnEditorTick(float) {
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

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void CourseProgressHudComponent::DrawInspectorImGui() {
		(void)ImGuiWidgets::InputText<64>("Course Id", mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		ImGui::Checkbox("Require Course HUD Enabled",
		                &mRequireCourseHudEnabled);
		ImGui::DragFloat(
			"Screen Clamp Margin Px",
			&mScreenClampMarginPx,
			1.0f,
			0.0f,
			1024.0f
		);
		ImGui::DragFloat("Pin Size Px", &mPinSizePx, 0.5f, 1.0f, 512.0f);
		ImGui::DragFloat("Arrow Size Px", &mArrowSizePx, 0.5f, 1.0f, 512.0f);
		(void)ImGuiWidgets::InputText<64>("Pin Widget Name", mPinWidgetName);
		(void)ImGuiWidgets::InputText<64>("Arrow Widget Name", mArrowWidgetName);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Minutes Widget", mElapsedMinutesWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Seconds Widget", mElapsedSecondsWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Fraction Widget", mElapsedFractionWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Comma Widget", mElapsedCommaWidgetName
		);
		(void)ImGuiWidgets::InputText<64>(
			"Elapsed Dot Widget", mElapsedDotWidgetName
		);
		DrawPathInput<128>("Pin Texture Path", mPinTexturePath);
		DrawPathInput<128>("Arrow Texture Path", mArrowTexturePath);
		DrawPathInput<128>("Digit Texture Path", mDigitTexturePath);
		DrawPathInput<128>("Comma Texture Path", mCommaTexturePath);
		DrawPathInput<128>("Dot Texture Path", mDotTexturePath);
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
			mRequireCourseHudEnabled = requireHud.GetBool(
				mRequireCourseHudEnabled);
		}
		if (const JsonReader margin = reader["screenClampMarginPx"];
			margin.Valid()) {
			mScreenClampMarginPx = std::max(
				0.0f, margin.GetFloat(mScreenClampMarginPx));
		}
		if (const JsonReader pinSize = reader["pinSizePx"];
			pinSize.Valid()) {
			mPinSizePx = std::max(1.0f, pinSize.GetFloat(mPinSizePx));
		}
		if (const JsonReader arrowSize = reader["arrowSizePx"];
			arrowSize.Valid()) {
			mArrowSizePx = std::max(1.0f, arrowSize.GetFloat(mArrowSizePx));
		}
		mPinWidgetName   = reader["pinWidgetName"].GetString(mPinWidgetName);
		mArrowWidgetName = reader["arrowWidgetName"].
			GetString(mArrowWidgetName);
		mElapsedMinutesWidgetName =
			reader["elapsedMinutesWidgetName"].GetString(
				mElapsedMinutesWidgetName);
		mElapsedSecondsWidgetName =
			reader["elapsedSecondsWidgetName"].GetString(
				mElapsedSecondsWidgetName);
		mElapsedFractionWidgetName = reader["elapsedFractionWidgetName"].
			GetString(
				mElapsedFractionWidgetName
			);
		mElapsedCommaWidgetName =
			reader["elapsedCommaWidgetName"].GetString(mElapsedCommaWidgetName);
		mElapsedDotWidgetName =
			reader["elapsedDotWidgetName"].GetString(mElapsedDotWidgetName);
		mPinTexturePath = Path(
			reader["pinTexturePath"].GetString(
				mPinTexturePath.ToGenericUtf8()
			)
		);
		mArrowTexturePath = Path(
			reader["arrowTexturePath"].GetString(
				mArrowTexturePath.ToGenericUtf8()
			)
		);
		mDigitTexturePath = Path(
			reader["digitTexturePath"].GetString(
				mDigitTexturePath.ToGenericUtf8()
			)
		);
		mCommaTexturePath = Path(
			reader["commaTexturePath"].GetString(
				mCommaTexturePath.ToGenericUtf8()
			)
		);
		mDotTexturePath = Path(
			reader["dotTexturePath"].GetString(
				mDotTexturePath.ToGenericUtf8()
			)
		);
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
		writer.Write(mPinTexturePath.ToGenericUtf8());
		writer.Key("arrowTexturePath");
		writer.Write(mArrowTexturePath.ToGenericUtf8());
		writer.Key("digitTexturePath");
		writer.Write(mDigitTexturePath.ToGenericUtf8());
		writer.Key("commaTexturePath");
		writer.Write(mCommaTexturePath.ToGenericUtf8());
		writer.Key("dotTexturePath");
		writer.Write(mDotTexturePath.ToGenericUtf8());
		writer.Key("elapsedTextAlpha");
		writer.Write(mElapsedTextAlpha);
	}

	void CourseProgressHudComponent::TickHud() {
		ResolveBindings();
		if (!mUiCanvas || !mUiCanvas->EnsureRuntimeLoaded()) {
			return;
		}
		AssetManager* const assetManager = GetAssetManager();

		const Gui::UiRoot* runtimeRoot = mUiCanvas->GetRuntimeRoot();
		Gui::UiWidget*     rootWidget  = runtimeRoot ?
			                            runtimeRoot->GetRootWidget() :
			                            nullptr;
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
		const auto UpdateElapsedTimeWidgets = [&](
			const bool  visible,
			const float elapsedSecondsValue
		) {
			const auto HideWidget = [](Gui::UiWidget* widget) {
				if (!widget) {
					return;
				}
				widget->SetVisible(false);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			if (!(elapsedMinutes && elapsedSeconds && elapsedFraction &&
			      elapsedComma &&
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
			const auto ApplyDigits = [&](
				Gui::UiWidget*              widget,
				Gui::UiDigitStripComponent* strip,
				const int                   value
			) {
				if (!widget || !strip) {
					return;
				}
				widget->SetVisible(true);
				if (assetManager) {
					(void)strip->SetStripTexturePath(
						mDigitTexturePath.IsEmpty() ?
							"textures/digits.png" :
							mDigitTexturePath.ToGenericUtf8(),
						*assetManager
					);
				}
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

			const auto ApplySeparator = [&](
				Gui::UiWidget*           widget,
				Gui::UiTextureComponent* texture,
				const Path&              path,
				const Path&              fallbackPath
			) {
				if (!widget || !texture) {
					return;
				}
				widget->SetVisible(true);
				if (assetManager) {
					(void)texture->SetTexturePath(
						path.IsEmpty() ?
							fallbackPath.ToGenericUtf8() :
							path.ToGenericUtf8(),
						*assetManager
					);
				}
				Gui::Color color = texture->GetColor();
				color.a          = alpha;
				texture->SetColor(color);
				widget->MarkDirty(Gui::DIRTY_FLAGS::DRAW);
			};
			ApplySeparator(
				elapsedCommaWidget,
				elapsedComma,
				mCommaTexturePath,
				Path("textures/colon.png")
			);
			ApplySeparator(
				elapsedDotWidget,
				elapsedDot,
				mDotTexturePath,
				Path("textures/dot.png")
			);
		};
		const bool hasGuideWidgets = pinWidget && arrowWidget;

		Gui::UiTransformComponent* pinTransform =
			hasGuideWidgets ?
				pinWidget->GetComponent<Gui::UiTransformComponent>() :
				nullptr;
		Gui::UiTransformComponent* arrowTransform =
			hasGuideWidgets ?
				arrowWidget->GetComponent<Gui::UiTransformComponent>() :
				nullptr;
		Gui::UiTextureComponent* pinTexture =
			hasGuideWidgets ?
				pinWidget->GetOrAddComponent<Gui::UiTextureComponent>() :
				nullptr;
		Gui::UiTextureComponent* arrowTexture =
			hasGuideWidgets ?
				arrowWidget->GetOrAddComponent<Gui::UiTextureComponent>() :
				nullptr;
		const bool canDrawGuides =
			hasGuideWidgets &&
			pinTransform &&
			arrowTransform &&
			pinTexture &&
			arrowTexture;
		if (canDrawGuides) {
			if (assetManager) {
				(void)pinTexture->SetTexturePath(
					mPinTexturePath.IsEmpty() ?
						"textures/ping.png" :
						mPinTexturePath.ToGenericUtf8(),
					*assetManager
				);
				(void)arrowTexture->SetTexturePath(
					mArrowTexturePath.IsEmpty() ?
						"textures/arrow.png" :
						mArrowTexturePath.ToGenericUtf8(),
					*assetManager
				);
			}
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

		UpdateElapsedTimeWidgets(
			!snapshot.courseCleared, snapshot.elapsedSeconds
		);

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
		const auto cameraInfo =
			world->GetCameraManager().GetCurrentCameraInfo();
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
				auto* progress = dynamic_cast<CourseProgressComponent*>(&
					component);
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
		Gui::UiWidget*         root,
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
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(
				child, widgetName)) {
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
			outViewportSizePx = mUiCanvas ?
				                    mUiCanvas->GetPixelSize() :
				                    Vec2::zero;
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
			localCenterPx.x             -= parentRect.x;
			localCenterPx.y             -= parentRect.y;
		}

		transform->SetAnchors(Gui::Anchors{});
		transform->SetMargins(Gui::Margins{});
		transform->SetPivot(Gui::Pivot{.x = 0.5f, .y = 0.5f});
		transform->SetRect(
			Gui::Rect{
				.x      = localCenterPx.x,
				.y      = localCenterPx.y,
				.width  = sizePx,
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
