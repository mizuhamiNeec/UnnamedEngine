#include "NewUICanvas.h"

#include "pch.h"

#include <algorithm>
#include <string_view>

#include "engine/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/content/ContentPathResolver.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/SceneLoadOptions.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/ui/UIContext.h"
#include "engine/unnamed/ui/UIFontAtlas.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kDefaultUIFontPath =
			"fonts/JetBrainsMono.ttf";

#ifdef _DEBUG
		constexpr const char* kTextAlignLabels[] = {
			"Left",
			"Center",
			"Right",
		};

		[[nodiscard]] int ToComboIndex(const UI::UI_TEXT_ALIGN align) {
			switch (align) {
				case UI::UI_TEXT_ALIGN::LEFT: return 0;
				case UI::UI_TEXT_ALIGN::RIGHT: return 2;
				case UI::UI_TEXT_ALIGN::CENTER:
				default: return 1;
			}
		}

		[[nodiscard]] UI::UI_TEXT_ALIGN ToTextAlign(const int index) {
			switch (index) {
				case 0: return UI::UI_TEXT_ALIGN::LEFT;
				case 2: return UI::UI_TEXT_ALIGN::RIGHT;
				case 1:
				default: return UI::UI_TEXT_ALIGN::CENTER;
			}
		}
#endif
	}

	void NewUICanvas::OnAttached() {
		BaseComponent::OnAttached();
		if (mFontAtlas == nullptr) {
			if (AssetManager* assetManager = GetAssetManager()) {
				(void)InitializeFontAtlas(
					*assetManager,
					AssetReferenceValidationPolicy::Permissive
				);
			}
		}
	}

	void NewUICanvas::OnTick(const float deltaTime) {
		BaseComponent::OnTick(deltaTime);
	}

	void NewUICanvas::OnFrameInputTick(const float frameDeltaTime) {
		const auto* input = GetWorld()->GetInputSystem();
		if (!input) {
			mDrawCommands.clear();
			return;
		}

		UI::UnnamedUiInputState inputState;
		inputState.mousePosition = input->GetMouseClientPosition();

		// 暫定的に攻撃ボタンを左クリックとして使用
		inputState.mousePressed  = input->IsPressed("attack1");
		inputState.mouseReleased = input->IsReleased("attack1");
		inputState.mouseDown     = input->IsHeld("attack1");

		mContext.BeginFrame(inputState, frameDeltaTime);
		mContext.SetTheme(mTheme);
		if (AssetManager* assetManager = GetAssetManager()) {
			mFontAtlas = ResolveFontAtlas(*assetManager);
		}
		mContext.SetFontAtlas(mFontAtlas);

		constexpr auto       panelPosition = Vec2(24.0f, 24.0f);
		constexpr auto       panelSize     = Vec2(1024.0f, 1024.0f);
		constexpr UI::UIRect panelRect{
			.position = panelPosition,
			.size     = panelSize,
		};
		mContext.BeginPanel(panelRect);

		const auto buttonSize = Vec2(128.0f, mTheme.buttonHeight);

		{
			mContext.BeginColumn(
				panelPosition + Vec2(mTheme.panelPadding, mTheme.panelPadding),
				mTheme.defaultGap
			);

			mContext.Label("Main Menu");
			mContext.Separator();
			mContext.Spacer(8.0f);

			if (mContext.Button("Play", buttonSize)) {
				Msg("UI", "Play button clicked!");
			}

			if (mContext.Button("Settings", buttonSize)) {
				Msg("UI", "Settings button clicked!");
			}

			if (mContext.Button("Exit", buttonSize)) {
				Msg("UI", "Exit button clicked!");
			}

			if (mContext.Checkbox("Show Debug", &mShowDebug)) {
				Msg("UI", "Show Debug checkbox toggled: {}", mShowDebug);
			}

			if (mContext.Checkbox("Enable Bloom", &mEnableBloom)) {
				Msg("UI", "Enable Bloom checkbox toggled: {}",
				    mEnableBloom);
			}

			if (mContext.SliderFloat("Volume", &mVolume, 0.0f, 1.0f)) {
				Msg("UI", "Volume slider changed: {:.2f}", mVolume);
			}

			mContext.PushID("Player");
			if (mContext.SliderFloat("Speed", &mPlayerSpeed, 0.0f, 10.0f)) {
				Msg("UI", "Player Speed slider changed: {:.2f}", mPlayerSpeed);
			}
			mContext.PopID();

			mContext.PushID("Enemy");
			if (mContext.SliderFloat("Speed", &mEnemySpeed, 0.0f, 10.0f)) {
				Msg("UI", "Enemy Speed slider changed: {:.2f}", mEnemySpeed);
			}
			mContext.PopID();

			mContext.Spacer(16.0f);

			mContext.Label("Actions");
			mContext.Separator();

			{
				mContext.BeginRow(
					panelPosition + Vec2(mTheme.panelPadding,
					                     256.0f + mTheme.defaultGap),
					mTheme.defaultGap);

				if (mContext.Button("OK", buttonSize)) {
					Msg("UI", "OK button clicked!");
				}

				if (mContext.Button("Cancel", buttonSize)) {
					Msg("UI", "Cancel button clicked!");
				}

				mContext.EndRow();
			}

			mContext.EndColumn();
		}

		mContext.EndPanel();

		mContext.EndFrame();

		const auto& commands = mContext.GetDrawList().GetCommands();
		mDrawCommands.assign(commands.begin(), commands.end());
	}

	void NewUICanvas::OnRenderTick(
		const float renderDeltaTime, const float interpolationAlpha
	) {
		BaseComponent::OnRenderTick(renderDeltaTime, interpolationAlpha);
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void NewUICanvas::DrawInspectorImGui() {
		bool fontSettingsChanged = false;
		ImGui::SeparatorText("==THEME==");

		ImGui::SeparatorText("Button");

		ImGui::DragFloat2("ButtonTextPadding", &mTheme.buttonTextPadding.x);

		ImGui::DragFloat("DefaultButtonHeight", &mTheme.buttonHeight);

		ImGui::ColorEdit4("ButtonBorderColor", &mTheme.buttonBorderColor.r);
		ImGui::DragFloat("ButtonBorderWidth", &mTheme.buttonBorderWidth);

		int buttonTextAlignIndex = ToComboIndex(mTheme.buttonTextAlign);
		if (
			ImGui::Combo(
				"ButtonTextAlign",
				&buttonTextAlignIndex,
				kTextAlignLabels,
				3
			)
		) {
			mTheme.buttonTextAlign = ToTextAlign(buttonTextAlignIndex);
		}

		ImGui::ColorEdit4("Normal", &mTheme.buttonNormalColor.r);
		ImGui::ColorEdit4("Hovered", &mTheme.buttonHoveredColor.r);
		ImGui::ColorEdit4("Pressed", &mTheme.buttonPressedColor.r);

		ImGui::SeparatorText("Panel");

		ImGui::ColorEdit4("Panel##Color", &mTheme.panelColor.r);
		ImGui::DragFloat("PanelPadding", &mTheme.panelPadding);

		ImGui::ColorEdit4("PanelBorderColor", &mTheme.panelBorderColor.r);
		ImGui::DragFloat("PanelBorderWidth", &mTheme.panelBorderWidth);

		ImGui::SeparatorText("Text");

		ImGui::ColorEdit4("Text##Color", &mTheme.textColor.r);

		ImGui::SeparatorText("Font");

		std::string fontPath = mFontPath.has_value() ?
			mFontPath->String() :
			std::string{};
		if (
			ImGuiWidgets::AssetPathPicker(
				"FontPath", fontPath, ImGuiWidgets::kAssetTypeMaskAny
			)
		) {
			mFontPath = VirtualPath::ParseContentReference(fontPath);
			fontSettingsChanged = true;
		}

		fontSettingsChanged |= ImGui::DragFloat(
			"FontSize", &mTheme.fontSize
		);

		int fontOversampleH = static_cast<int>(mTheme.fontOversampleH);
		if (ImGui::DragInt("FontOversampleH", &fontOversampleH, 1.0f, 1, 64)) {
			mTheme.fontOversampleH = static_cast<uint32_t>(std::max(
				fontOversampleH, 1
			));
			fontSettingsChanged = true;
		}

		int fontOversampleV = static_cast<int>(mTheme.fontOversampleV);
		if (ImGui::DragInt("FontOversampleV", &fontOversampleV, 1.0f, 1, 64)) {
			mTheme.fontOversampleV = static_cast<uint32_t>(std::max(
				fontOversampleV, 1
			));
			fontSettingsChanged = true;
		}

		if (fontSettingsChanged) {
			if (AssetManager* assetManager = GetAssetManager()) {
				(void)InitializeFontAtlas(
					*assetManager,
					AssetReferenceValidationPolicy::Permissive
				);
			} else {
				mFontAtlas = nullptr;
			}
		}

		ImGui::SeparatorText("FontAtlas Cache");
		const UI::UIFontAtlasCacheDebugInfo cacheInfo =
			UI::GetUIFontAtlasCache().
			GetDebugInfo();
		ImGui::Text(
			"Cache: %zu / %zu",
			cacheInfo.cacheCount,
			cacheInfo.maxCacheEntries
		);
		ImGui::Text(
			"CreateRuntimeAsset calls: %llu",
			static_cast<unsigned long long>(cacheInfo.
				createRuntimeAssetCallCount
			)
		);
		ImGui::Text(
			"DestroyRuntimeAsset calls: %llu (failed: %llu)",
			static_cast<unsigned long long>(cacheInfo.
				destroyRuntimeAssetCallCount
			),
			static_cast<unsigned long long>(
				cacheInfo.destroyRuntimeAssetFailedCount
			)
		);
		if (const AssetManager* assetManager = GetWorld() ?
			                                       GetWorld()->
			                                       GetAssetManager() :
			                                       nullptr) {
			const AssetManager::DebugStats assetStats =
				assetManager->GetDebugStats();
			ImGui::Text(
				"AssetManager runtime textures: %zu, destroyed: %zu",
				assetStats.runtimeTextureAssetCount,
				assetStats.destroyedRuntimeAssetCount
			);
			ImGui::Text(
				"Loaded TextureAssetData: %zu, DestroyRuntimeAsset total: %llu",
				assetStats.loadedTextureAssetCount,
				static_cast<unsigned long long>(
					assetStats.destroyRuntimeAssetCount
				)
			);
		}
		ImGui::Text(
			"Current key: size100=%d, oversample=%ux%u",
			cacheInfo.currentKey.fontSize100,
			cacheInfo.currentKey.oversampleH,
			cacheInfo.currentKey.oversampleV
		);
		ImGui::Text(
			"Current atlas assetId: %u",
			cacheInfo.currentTextureAssetId
		);
		ImGui::Text(
			"Current font path: %s",
			cacheInfo.currentKey.fontPath.String().c_str()
		);
		if (ImGui::Button("Clear Font Cache")) {
			if (AssetManager* assetManager = GetWorld() ?
				                                 GetWorld()->GetAssetManager() :
				                                 nullptr) {
				UI::GetUIFontAtlasCache().Clear(assetManager);
			} else {
				Warning("UI", "Clear Font Cache failed: AssetManager is null.");
			}
		}

		ImGui::DragFloat("DefaultGap", &mTheme.defaultGap);
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
		mFontAtlas = nullptr;
		mResolvedFontPath.Clear();
		const JsonReader fontPathNode = reader["fontPath"];
		const std::string fontPath = fontPathNode.Valid() &&
		                             fontPathNode.IsString() ?
			fontPathNode.GetString() :
			std::string(kDefaultUIFontPath);
		mFontPath = VirtualPath::ParseContentReference(fontPath);
		if (reader.Has("fontSize")) {
			mTheme.fontSize = reader["fontSize"].GetFloat(mTheme.fontSize);
		}
		if (reader.Has("fontOversampleH")) {
			mTheme.fontOversampleH = static_cast<uint32_t>(std::max(
				1, reader["fontOversampleH"].GetInt(1)
			));
		}
		if (reader.Has("fontOversampleV")) {
			mTheme.fontOversampleV = static_cast<uint32_t>(std::max(
				1, reader["fontOversampleV"].GetInt(1)
			));
		}
	}

	bool NewUICanvas::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		mFontAtlas = nullptr;
		mResolvedFontPath.Clear();
		const JsonReader fontPathNode = reader["fontPath"];
		const std::string fontPath = fontPathNode.Valid() ?
			fontPathNode.GetString() :
			std::string(kDefaultUIFontPath);
		if (fontPathNode.Valid() && !fontPathNode.IsString()) {
			Error("UI", "NewUICanvas fontPath must be a string.");
			if (IsStrictAssetValidation(context.loadOptions)) {
				return false;
			}
			mFontPath = VirtualPath::ParseContentReference(
				kDefaultUIFontPath
			);
		} else {
			mFontPath = VirtualPath::ParseContentReference(fontPath);
			if (!mFontPath.has_value()) {
				Error("UI", "Invalid UI font virtual path: {}", fontPath);
				if (IsStrictAssetValidation(context.loadOptions)) {
					return false;
				}
				mFontPath = VirtualPath::ParseContentReference(
					kDefaultUIFontPath
				);
			}
		}

		if (reader.Has("fontSize")) {
			mTheme.fontSize = reader["fontSize"].GetFloat(mTheme.fontSize);
		}
		if (reader.Has("fontOversampleH")) {
			mTheme.fontOversampleH = static_cast<uint32_t>(std::max(
				1, reader["fontOversampleH"].GetInt(1)
			));
		}
		if (reader.Has("fontOversampleV")) {
			mTheme.fontOversampleV = static_cast<uint32_t>(std::max(
				1, reader["fontOversampleV"].GetInt(1)
			));
		}

		if (!context.assetManager) {
			Error("UI", "AssetManager is unavailable for NewUICanvas font.");
			return false;
		}
		return InitializeFontAtlas(
			*context.assetManager,
			ToAssetReferenceValidationPolicy(
				context.loadOptions.assetValidationPolicy
			)
		);
	}

	void NewUICanvas::Serialize(JsonWriter& writer) const {
		if (mFontPath.has_value()) {
			writer.Key("fontPath");
			writer.Write(mFontPath->String());
		}
		writer.Key("fontSize");
		writer.Write(mTheme.fontSize);
		writer.Key("fontOversampleH");
		writer.Write(mTheme.fontOversampleH);
		writer.Key("fontOversampleV");
		writer.Write(mTheme.fontOversampleV);
	}

	const std::vector<UI::UIDrawCommand>& NewUICanvas::GetDrawCommands() const {
		return mDrawCommands;
	}

	UI::UIFontAtlas* NewUICanvas::ResolveFontAtlas(
		AssetManager& assetManager
	) {
		if (!mFontPath.has_value() || mResolvedFontPath.IsEmpty()) {
			mFontAtlas = nullptr;
			return nullptr;
		}
		mFontAtlas = UI::GetUIFontAtlasCache().GetOrCreate(
			UI::MakeUIFontAtlasKey(
				*mFontPath,
				mTheme.fontSize,
				mTheme.fontOversampleH,
				mTheme.fontOversampleV
			),
			mResolvedFontPath,
			assetManager
		);
		return mFontAtlas;
	}

	bool NewUICanvas::InitializeFontAtlas(
		AssetManager& assetManager,
		const AssetReferenceValidationPolicy validationPolicy
	) {
		mFontAtlas = nullptr;
		mResolvedFontPath.Clear();
		const std::optional<VirtualPath> defaultFontPath =
			VirtualPath::ParseContentReference(kDefaultUIFontPath);
		if (!defaultFontPath.has_value()) {
			return false;
		}
		if (!mFontPath.has_value()) {
			if (IsStrictAssetValidation(validationPolicy)) {
				return false;
			}
			mFontPath = *defaultFontPath;
		}

		if (TryInitializeFontAtlas(*mFontPath, assetManager)) {
			return true;
		}
		if (
			IsStrictAssetValidation(validationPolicy) ||
			*mFontPath == *defaultFontPath
		) {
			return false;
		}

		Warning(
			"UI",
			"Falling back to default UI font: {}",
			defaultFontPath->String()
		);
		mFontPath = *defaultFontPath;
		return TryInitializeFontAtlas(*mFontPath, assetManager);
	}

	bool NewUICanvas::TryInitializeFontAtlas(
		const VirtualPath& fontPath, AssetManager& assetManager
	) {
		const auto resolution = assetManager.GetContentPathResolver().ResolveFile(
			fontPath
		);
		if (!resolution.has_value()) {
			Error("UI", "Failed to resolve UI font file: {}", fontPath.String());
			return false;
		}

		mFontAtlas = UI::GetUIFontAtlasCache().GetOrCreate(
			UI::MakeUIFontAtlasKey(
				fontPath,
				mTheme.fontSize,
				mTheme.fontOversampleH,
				mTheme.fontOversampleV
			),
			resolution->resolvedPath,
			assetManager
		);
		if (!mFontAtlas) {
			Error(
				"UI",
				"Failed to build UI font atlas: virtualPath={} mount={} physicalPath={}",
				fontPath.String(),
				resolution->mountId,
				resolution->resolvedPath
			);
			return false;
		}
		mResolvedFontPath = resolution->resolvedPath;

		DevMsg(
			"UI",
			"Resolved UI font: virtualPath={} mount={} physicalPath={}",
			fontPath.String(),
			resolution->mountId,
			resolution->resolvedPath
		);
		return true;
	}

	REGISTER_COMPONENT(NewUICanvas);
}
