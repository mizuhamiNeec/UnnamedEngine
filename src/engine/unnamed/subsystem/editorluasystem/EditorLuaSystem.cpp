#include "EditorLuaSystem.h"

#include <thirdparty/lua/lua.hpp>
#ifdef UNNAMED_WITH_EDITOR
#include <imgui.h>
#endif

namespace Unnamed {
	namespace {
		void SetError(std::string* outError, const char* message) {
			if (outError) {
				*outError = message ? message : "Unknown Lua error";
			}
		}

		int LuaUiText(lua_State* lua) {
			const char* text = luaL_checkstring(lua, 1);
#ifdef UNNAMED_WITH_EDITOR
			ImGui::TextUnformatted(text);
#else
			(void)text;
#endif
			return 0;
		}

		int LuaUiButton(lua_State* lua) {
			const char* text = luaL_checkstring(lua, 1);
#ifdef UNNAMED_WITH_EDITOR
			const bool pressed = ImGui::Button(text);
#else
			(void)text;
			constexpr bool pressed = false;
#endif

			lua_pushboolean(lua, pressed);
			return 1;
		}

		int LuaUiBeginWindow(lua_State* lua) {
			const char* title  = luaL_checkstring(lua, 1);
#ifdef UNNAMED_WITH_EDITOR
			const bool  opened = ImGui::Begin(title);
#else
			(void)title;
			constexpr bool opened = false;
#endif
			lua_pushboolean(lua, opened);
			return 1;
		}

		int LuaUiEndWindow(lua_State*) {
#ifdef UNNAMED_WITH_EDITOR
			ImGui::End();
#endif
			return 0;
		}

		void RegisterEditorLuaBindings(lua_State* lua) {
			lua_register(lua, "Ui_Text", LuaUiText);
			lua_register(lua, "Ui_Button", LuaUiButton);
			lua_register(lua, "Ui_BeginWindow", LuaUiBeginWindow);
			lua_register(lua, "Ui_EndWindow", LuaUiEndWindow);
		}

		auto kEditorUiBootStrapLua = R"(
			Ui = Ui or {}
			
			function Ui.Text(text)
				Ui_Text(text)
			end

			function Ui.Button(text)
				return Ui_Button(text)
			end

			function Ui.BeginWindow(title)
				return Ui_BeginWindow(title)
			end

			function Ui.EndWindow()
				Ui_EndWindow()
			end
		)";
	}

	bool EditorLuaSystem::Init() {
		if (mState) {
			return true;
		}

		mState = luaL_newstate();
		if (!mState) {
			return false;
		}

		luaL_openlibs(mState);
		RegisterEditorLuaBindings(mState);

		std::string error;
		ExecuteString(kEditorUiBootStrapLua, &error);

		return true;
	}

	void EditorLuaSystem::Shutdown() {
		if (mState) {
			lua_close(mState);
			mState = nullptr;
		}
	}

	const std::string_view EditorLuaSystem::GetName() const {
		return "EditorLua";
	}

	bool EditorLuaSystem::ExecuteString(
		const std::string& src, std::string* outError
	) const {
		if (!mState) {
			SetError(outError, "Lua state is not initialized");
			return false;
		}

		if (outError) {
			outError->clear();
		}

		const int loadResult = luaL_loadbuffer(
			mState,
			src.data(),
			src.size(),
			"EditorGuiScript"
		);

		if (loadResult != LUA_OK) {
			SetError(outError, lua_tostring(mState, -1));
			lua_pop(mState, 1);
			return false;
		}

		const int callResult = lua_pcall(mState, 0, 0, 0);
		if (callResult != LUA_OK) {
			SetError(outError, lua_tostring(mState, -1));
			lua_pop(mState, 1);
			return false;
		}

		return true;
	}

	bool EditorLuaSystem::CallFunction(
		const char* name, std::string* outError
	) const {
		if (!mState) {
			SetError(outError, "Lua state is not initialized");
			return false;
		}

		if (!name || name[0] == '\0') {
			SetError(outError, "Lua function name is empty");
			return false;
		}

		if (outError) {
			outError->clear();
		}

		lua_getglobal(mState, name);

		if (!lua_isfunction(mState, -1)) {
			lua_pop(mState, 1);

			std::string message = "Lua function not found: ";
			message             += name;
			SetError(outError, message.c_str());
			return false;
		}

		const int callResult = lua_pcall(mState, 0, 0, 0);
		if (callResult != LUA_OK) {
			SetError(outError, lua_tostring(mState, -1));
			lua_pop(mState, 1);
			return false;
		}

		return true;
	}
}
