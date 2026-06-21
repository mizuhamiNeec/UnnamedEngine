#pragma once
#ifdef _DEBUG

#include <core/assets/AssetManager.h>

#include "engine/unnamed/subsystem/EditorLuaSystem/EditorLuaSystem.h"

namespace Unnamed {
	/// @brief luaでエディタを拡張するためのGUIパネル
	class EditorGuiScriptPanel {
	public:
		void Initialize(AssetManager* assetManager, EditorLuaSystem* luaSystem);
		void SetScriptPath(Path path);
		void Reload();
		void Draw();

	private:
		AssetManager*    mAssetManager    = nullptr;
		EditorLuaSystem* mEditorLuaSystem = nullptr;

		Path    mScriptPath;
		AssetID mAssetID = 0;

		bool mLoaded   = false;
		bool mHasError = false;

		std::string mLastError;
	};
}

#endif
