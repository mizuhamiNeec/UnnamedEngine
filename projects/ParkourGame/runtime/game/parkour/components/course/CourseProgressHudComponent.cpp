#include "CourseProgressHudComponent.h"

#include <algorithm>
#include <array>
#include <cstring>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/game/GamePathResolver.h"
#include "engine/game/IGameModule.h"
#include "engine/gui/Rect.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/gui/components/UiTransformComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

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
			return path.rfind("content/", 0) == 0 ||
			       path.rfind("projects/", 0) == 0;
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
		DrawStringInput<128>("Pin Texture Path", mPinTexturePath);
		DrawStringInput<128>("Arrow Texture Path", mArrowTexturePath);
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
		mPinTexturePath  = reader["pinTexturePath"].GetString(mPinTexturePath);
		mArrowTexturePath = reader["arrowTexturePath"].GetString(mArrowTexturePath);
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
		writer.Key("pinTexturePath");
		writer.Write(mPinTexturePath);
		writer.Key("arrowTexturePath");
		writer.Write(mArrowTexturePath);
	}

	void CourseProgressHudComponent::TickHud() {
		ResolveBindings();
		if (!mUiCanvas || !mUiCanvas->EnsureRuntimeLoaded()) {
			return;
		}

		Gui::UiRoot* runtimeRoot = mUiCanvas->GetRuntimeRoot();
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
		if (!pinWidget || !arrowWidget) {
			return;
		}

		auto* pinTransform = pinWidget->GetComponent<Gui::UiTransformComponent>();
		auto* arrowTransform = arrowWidget->GetComponent<Gui::UiTransformComponent>();
		auto* pinTexture = pinWidget->GetOrAddComponent<Gui::UiTextureComponent>();
		auto* arrowTexture = arrowWidget->GetOrAddComponent<Gui::UiTextureComponent>();
		if (!pinTransform || !arrowTransform || !pinTexture || !arrowTexture) {
			return;
		}

		pinTexture->SetTexturePath(
			ResolveHudContentPath(mPinTexturePath, "textures/ping.png")
		);
		arrowTexture->SetTexturePath(
			ResolveHudContentPath(mArrowTexturePath, "textures/arrow.png")
		);

		const auto hideBoth = [&]() {
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
			hideBoth();
			return;
		}
		if (mRequireCourseHudEnabled && !mCourseProgress->IsHudEnabled()) {
			hideBoth();
			return;
		}

		const CourseProgressSnapshot& snapshot = mCourseProgress->GetSnapshot();
		if (snapshot.courseCleared || !snapshot.hasNextTarget) {
			hideBoth();
			return;
		}

		World* world = GetWorld();
		if (!world) {
			hideBoth();
			return;
		}
		const auto cameraInfo = world->GetCameraManager().GetCurrentCameraInfo();
		if (!cameraInfo.valid) {
			hideBoth();
			return;
		}

		Vec2 viewportSizePx = Vec2::zero;
		if (!ResolveViewportSize(viewportSizePx)) {
			hideBoth();
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
			hideBoth();
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
	) const {
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
		if (Gui::UiWidget* parent = widget->GetParent()) {
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
