#pragma once
#ifdef _DEBUG

#include <core/assets/AssetManager.h>
#include <core/filesystem/VirtualPath.h>

#include "engine/editor/lua/EditorLuaSystem.h"

namespace Unnamed {
	/// @brief luaでエディタを拡張するためのGUIパネル
	class EditorGuiScriptPanel {
	public:
		void Initialize(AssetManager* assetManager, EditorLuaSystem* luaSystem);
		/// @brief Core mount内のEditor GUIスクリプト論理パスを設定します。
		void SetScriptPath(VirtualPath path);
		void Reload();
		void Draw();

	private:
		AssetManager*    mAssetManager    = nullptr;
		EditorLuaSystem* mEditorLuaSystem = nullptr;

		VirtualPath mScriptPath;
		AssetID mAssetID = 0;

		bool mLoaded   = false;
		bool mHasError = false;

		std::string mLastError;
	};
}

#endif
