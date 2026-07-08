#ifdef _DEBUG
#include "EditorGuiScriptPanel.h"

#include <imgui.h>

#include "engine/content/ContentMountDefinitions.h"

namespace Unnamed {
	constexpr ImVec4 kTextColorError(1.0f, 0.35f, 0.35f, 1.0f);

	void EditorGuiScriptPanel::Initialize(
		AssetManager* assetManager, EditorLuaSystem* luaSystem
	) {
		mAssetManager    = assetManager;
		mEditorLuaSystem = luaSystem;
	}

	void EditorGuiScriptPanel::SetScriptPath(VirtualPath path) {
		if (mScriptPath == path) {
			return;
		}

		mScriptPath = std::move(path);
		mAssetID    = 0;
		mLoaded     = false;
		mHasError   = false;
		mLastError.clear();

		Reload();
	}

	void EditorGuiScriptPanel::Reload() {
		mHasError = false;
		mLastError.clear();

		if (!mAssetManager) {
			mHasError  = true;
			mLastError = "AssetManager is null.";
			return;
		}

		if (!mEditorLuaSystem) {
			mHasError  = true;
			mLastError = "EditorLuaSystem is null.";
			return;
		}

		if (mScriptPath.IsEmpty()) {
			mHasError  = true;
			mLastError = "Editor GUI script path is empty.";
			return;
		}

		mAssetID = mAssetManager->LoadAssetFromMount(
			mScriptPath,
			ContentMountId::kCore,
			ASSET_TYPE::EDITOR_GUI
		);

		const auto* editorGuiAsset = mAssetManager->Get<
			EditorGuiData>(mAssetID);
		if (!editorGuiAsset) {
			mHasError  = true;
			mLastError = "Failed to load editor GUI asset: " +
			             mScriptPath.String();
			return;
		}

		std::string error;
		if (!mEditorLuaSystem->ExecuteString(editorGuiAsset->source, &error)) {
			mHasError  = true;
			mLastError = error;
			return;
		}

		mLoaded = true;
	}

	void EditorGuiScriptPanel::Draw() {
		if (mScriptPath.IsEmpty()) {
			ImGui::TextUnformatted("No editor GUI script selected.");
			return;
		}

		if (!mLoaded) {
			Reload();
		}

		if (mHasError) {
			ImGui::TextColored(
				kTextColorError,
				"Editor GUI Lua Error:"
			);
			ImGui::TextWrapped("%s", mLastError.c_str());

			if (ImGui::Button("Reload Script")) {
				Reload();
			}

			return;
		}

		std::string error;
		if (!mEditorLuaSystem->CallFunction("Draw", &error)) {
			mHasError  = true;
			mLastError = error;

			ImGui::TextColored(
				kTextColorError,
				"Editor GUI Lua Error:"
			);
			ImGui::TextWrapped("%s", mLastError.c_str());
		}
	}
}

#endif
