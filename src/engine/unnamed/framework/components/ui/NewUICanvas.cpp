#include "NewUICanvas.h"

#include "pch.h"

#include <algorithm>
#include <array>

#include "core/ComponentRegistry.h"

#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/ui/UIContext.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		struct FontOversamplePreset {
			uint32_t h = 1;
			uint32_t v = 1;
		};

		constexpr std::array<float, 4> kTestFontSizes = {16.0f, 18.0f, 20.0f, 24.0f};
		constexpr std::array<FontOversamplePreset, 4> kTestOversamplePresets = {
			FontOversamplePreset{1, 1},
			FontOversamplePreset{2, 2},
			FontOversamplePreset{3, 1},
			FontOversamplePreset{3, 2},
		};
		constexpr size_t kDefaultFontSizeIndex   = 0;
		constexpr size_t kDefaultOversampleIndex = 1;

		void ApplyTextQualityPreset(UI::UITheme& theme) {
			int fontSizePresetIndex   = static_cast<int>(kDefaultFontSizeIndex);
			int oversamplePresetIndex = static_cast<int>(kDefaultOversampleIndex);

			if (auto* console = ServiceLocator::Get<ConsoleSystem>();
				console != nullptr) {
				if (const auto* fontSizePresetVar =
					    console->GetConVarAs<ConVar<int>>(
						    "ui_new_font_size_preset"
					    );
					fontSizePresetVar != nullptr) {
					fontSizePresetIndex = std::clamp(
						fontSizePresetVar->GetValue(),
						0,
						static_cast<int>(kTestFontSizes.size() - 1)
					);
				}

				if (const auto* oversamplePresetVar =
					    console->GetConVarAs<ConVar<int>>(
						    "ui_new_font_oversample_preset"
					    );
					oversamplePresetVar != nullptr) {
					oversamplePresetIndex = std::clamp(
						oversamplePresetVar->GetValue(),
						0,
						static_cast<int>(kTestOversamplePresets.size() - 1)
					);
				}
			}

			theme.fontSize = kTestFontSizes[static_cast<size_t>(fontSizePresetIndex)];
			theme.fontOversampleH =
				kTestOversamplePresets[static_cast<size_t>(oversamplePresetIndex)].h;
			theme.fontOversampleV =
				kTestOversamplePresets[static_cast<size_t>(oversamplePresetIndex)].v;

#ifdef _DEBUG
			static int sLastFontPresetIndex   = -1;
			static int sLastOversamplePresetIndex = -1;
			if (
				sLastFontPresetIndex != fontSizePresetIndex ||
				sLastOversamplePresetIndex != oversamplePresetIndex
			) {
				DevMsg(
					"UI",
					"NewUICanvas text quality preset: fontSizePreset={}({}), oversamplePreset={}({}x{}).",
					fontSizePresetIndex,
					theme.fontSize,
					oversamplePresetIndex,
					theme.fontOversampleH,
					theme.fontOversampleV
				);
				sLastFontPresetIndex       = fontSizePresetIndex;
				sLastOversamplePresetIndex = oversamplePresetIndex;
			}
#endif
		}
	}

	void NewUICanvas::OnAttached() {
		BaseComponent::OnAttached();
		mTheme.buttonNormalColor = UI::UIColor{
			.r = 0.14f,
			.g = 0.18f,
			.b = 0.28f,
			.a = 1.0f
		};
		mTheme.buttonHoveredColor = UI::UIColor{
			.r = 0.10f,
			.g = 0.55f,
			.b = 0.95f,
			.a = 1.0f
		};
		mTheme.buttonPressedColor = UI::UIColor{
			.r = 0.96f,
			.g = 0.34f,
			.b = 0.08f,
			.a = 1.0f
		};
		mTheme.panelColor = UI::UIColor{
			.r = 0.07f,
			.g = 0.09f,
			.b = 0.14f,
			.a = 0.90f
		};
		mTheme.textColor = UI::UIColor{
			.r = 0.98f,
			.g = 0.98f,
			.b = 0.98f,
			.a = 1.0f
		};
		mTheme.panelPadding        = 16.0f;
		mTheme.buttonTextPadding   = Vec2(14.0f, 12.0f);
		mTheme.defaultGap          = 8.0f;
		mTheme.defaultButtonHeight = 48.0f;
		ApplyTextQualityPreset(mTheme);

#ifdef _DEBUG
		static bool sLoggedThemeFontSize = false;
		if (!sLoggedThemeFontSize) {
			DevMsg(
				"UI",
				"NewUICanvas test font settings: sizes=[{}, {}, {}, {}], oversamplePresets=[1x1, 2x2, 3x1, 3x2], activeSize={}, activeOversample={}x{}",
				kTestFontSizes[0],
				kTestFontSizes[1],
				kTestFontSizes[2],
				kTestFontSizes[3],
				mTheme.fontSize,
				mTheme.fontOversampleH,
				mTheme.fontOversampleV
			);
			sLoggedThemeFontSize = true;
		}
#endif
	}

	void NewUICanvas::OnTick(float deltaTime) {
		BaseComponent::OnTick(deltaTime);
	}

	void NewUICanvas::OnFrameInputTick(float frameDeltaTime) {
		BaseComponent::OnFrameInputTick(frameDeltaTime);
		ApplyTextQualityPreset(mTheme);

		const auto* input = GetWorld()->GetInputSystem();
		if (!input) {
			mDrawCommands.clear();
			return;
		}

		UI::UnnamedUiInputState inputState;
		inputState.mousePosition = input->GetMouseClientPosition();
		inputState.mousePressed  = input->IsPressed("attack1");
		inputState.mouseReleased = input->IsReleased("attack1");
		inputState.mouseDown     = input->IsHeld("attack1");

		UI::UIContext uiContext;
		uiContext.BeginFrame(inputState);
		uiContext.SetTheme(mTheme);

		const Vec2 panelPosition = Vec2(24.0f, 24.0f);
		const Vec2 panelSize     = Vec2(288.0f, 224.0f);
		const UI::UIRect panelRect{
			.position = panelPosition,
			.size     = panelSize,
		};
		uiContext.BeginPanel(panelRect);

		uiContext.BeginColumn(
			panelPosition + Vec2(mTheme.panelPadding, mTheme.panelPadding),
			mTheme.defaultGap
		);

		const Vec2 buttonSize = Vec2(240.0f, mTheme.defaultButtonHeight);
		if (uiContext.Button("Play", buttonSize)) {
			Msg("UI", "Play clicked");
		}
		if (uiContext.Button("Settings", buttonSize)) {
			Msg("UI", "Settings clicked");
		}
		if (uiContext.Button("Exit", buttonSize)) {
			Msg("UI", "Exit clicked");
		}

		uiContext.EndColumn();
		uiContext.EndPanel();

		uiContext.EndFrame();

		const auto& commands = uiContext.GetDrawList().GetCommands();
		mDrawCommands.assign(commands.begin(), commands.end());

#ifdef _DEBUG
		static bool sLoggedDrawCommandCount = false;
		if (!sLoggedDrawCommandCount) {
			DevMsg(
				"UI",
				"NewUICanvas generated {} UIDrawCommand(s).",
				mDrawCommands.size()
			);
			sLoggedDrawCommandCount = true;
		}
#endif
	}

	void NewUICanvas::OnRenderTick(
		float renderDeltaTime, float interpolationAlpha
	) {
		BaseComponent::OnRenderTick(renderDeltaTime, interpolationAlpha);
	}

#ifdef _DEBUG
	void NewUICanvas::DrawInspectorImGui() {
		BaseComponent::DrawInspectorImGui();
	}
#endif

	BaseComponent::TICK_GROUP NewUICanvas::GetTickGroup() const {
		return BaseComponent::GetTickGroup();
	}

	std::string_view NewUICanvas::GetStableName() const {
		return "engine.NewUICanvas";
	}

	std::string_view NewUICanvas::GetComponentName() const {
		return "NewUICanvas";
	}

	void NewUICanvas::Deserialize(const JsonReader& reader) {
		(void)reader;
	}

	void NewUICanvas::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	const std::vector<UI::UIDrawCommand>& NewUICanvas::GetDrawCommands() const {
		return mDrawCommands;
	}

	REGISTER_COMPONENT(NewUICanvas);
}
