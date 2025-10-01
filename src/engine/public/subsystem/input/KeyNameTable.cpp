#include <pch.h>

#include "KeyNameTable.h"

#include <algorithm>
#include <iterator>


namespace Unnamed {
	std::optional<InputKey> KeyNameTable::FromString(
		const std::string_view name) {
		const std::unordered_map<std::string, InputKey>& map = NameToKey();
		const auto it = map.find(Normalize(name));
		if (it == map.end()) {
			return std::nullopt;
		}
		return it->second;
	}

	std::string_view KeyNameTable::ToString(const InputKey& key) {
		const auto& map = KeyToName();
		const auto  it  = map.find(key);
		return it == map.end() ? std::string_view{} : it->second;
	}

	const std::unordered_map<std::string, InputKey>& KeyNameTable::NameToKey() {
		return sNameToKey;
	}

	const std::unordered_map<InputKey, std::string_view, KeyHash>&
	KeyNameTable::KeyToName() {
		return sKeyToName;
	}

	std::string KeyNameTable::Normalize(std::string_view str) {
		std::string ret;
		ret.reserve(str.size());
		std::ranges::transform(
			str,
			std::back_inserter(ret), [](const unsigned char c) {
				return std::tolower(c);
			}
		);

		return ret;
	}

#define KEY(name, dev, code) { name, { InputDeviceType::dev, static_cast<uint32_t>(code) } }

	const std::unordered_map<std::string, InputKey> KeyNameTable::sNameToKey = {
		//---------------------------------------------------------------------
		// キーボード
		//---------------------------------------------------------------------
		// ファンクションキー
		KEY("f1", KEYBOARD, VK_F1),
		KEY("f2", KEYBOARD, VK_F2),
		KEY("f3", KEYBOARD, VK_F3),
		KEY("f4", KEYBOARD, VK_F4),
		KEY("f5", KEYBOARD, VK_F5),
		KEY("f6", KEYBOARD, VK_F6),
		KEY("f7", KEYBOARD, VK_F7),
		KEY("f8", KEYBOARD, VK_F8),
		KEY("f9", KEYBOARD, VK_F9),
		KEY("f10", KEYBOARD, VK_F10),
		KEY("f11", KEYBOARD, VK_F11),
		KEY("f12", KEYBOARD, VK_F12),
		// F13以降は一般的ではないッ!

		// 数字キー
		KEY("0", KEYBOARD, 0x30), // 0
		KEY("1", KEYBOARD, 0x31), // 1
		KEY("2", KEYBOARD, 0x32), // 2
		KEY("3", KEYBOARD, 0x33), // 3
		KEY("4", KEYBOARD, 0x34), // 4
		KEY("5", KEYBOARD, 0x35), // 5
		KEY("6", KEYBOARD, 0x36), // 6
		KEY("7", KEYBOARD, 0x37), // 7
		KEY("8", KEYBOARD, 0x38), // 8
		KEY("9", KEYBOARD, 0x39), // 9

		// アルファベット
		KEY("a", KEYBOARD, 0x41), // A
		KEY("b", KEYBOARD, 0x42), // B
		KEY("c", KEYBOARD, 0x43), // C
		KEY("d", KEYBOARD, 0x44), // D
		KEY("e", KEYBOARD, 0x45), // E
		KEY("f", KEYBOARD, 0x46), // F
		KEY("g", KEYBOARD, 0x47), // G
		KEY("h", KEYBOARD, 0x48), // H
		KEY("i", KEYBOARD, 0x49), // I
		KEY("j", KEYBOARD, 0x4A), // J
		KEY("k", KEYBOARD, 0x4B), // K
		KEY("l", KEYBOARD, 0x4C), // L
		KEY("m", KEYBOARD, 0x4D), // M
		KEY("n", KEYBOARD, 0x4E), // N
		KEY("o", KEYBOARD, 0x4F), // O
		KEY("p", KEYBOARD, 0x50), // P
		KEY("q", KEYBOARD, 0x51), // Q
		KEY("r", KEYBOARD, 0x52), // R
		KEY("s", KEYBOARD, 0x53), // S
		KEY("t", KEYBOARD, 0x54), // T
		KEY("u", KEYBOARD, 0x55), // U
		KEY("v", KEYBOARD, 0x56), // V
		KEY("w", KEYBOARD, 0x57), // W
		KEY("x", KEYBOARD, 0x58), // X
		KEY("y", KEYBOARD, 0x59), // Y
		KEY("z", KEYBOARD, 0x5A), // Z

		// System, Modifier, Other

		// 特殊キー (※は非推奨 キーボードによっては存在しないキー・特殊な人のキーボード)
		KEY("escape", KEYBOARD, VK_ESCAPE),
		KEY("tab", KEYBOARD, VK_TAB),
		KEY("capslock", KEYBOARD, VK_CAPITAL),  // CapsLock(※
		KEY("shift", KEYBOARD, VK_LSHIFT),      // 左シフト
		KEY("rshift", KEYBOARD, VK_RSHIFT),     // 右シフト
		KEY("ctrl", KEYBOARD, VK_LCONTROL),     // 左コントロール
		KEY("rctrl", KEYBOARD, VK_RCONTROL),    // 右コントロール
		KEY("alt", KEYBOARD, VK_LMENU),         // 左Alt
		KEY("ralt", KEYBOARD, VK_RMENU),        // 右Alt
		KEY("space", KEYBOARD, VK_SPACE),       // スペース
		KEY("backspace", KEYBOARD, VK_BACK),    // バックスペース
		KEY("enter", KEYBOARD, VK_RETURN),      // エンター
		KEY("semicolon", KEYBOARD, VK_OEM_1),   // セミコロン
		KEY("lwin", KEYBOARD, VK_LWIN),         // 左Windowsキー
		KEY("rwin", KEYBOARD, VK_RWIN),         // 右Windowsキー(※
		KEY("apps", KEYBOARD, VK_APPS),         // アプリケーションキー(※
		KEY("numlock", KEYBOARD, VK_NUMLOCK),   // NumLock
		KEY("scrolllock", KEYBOARD, VK_SCROLL), // ScrollLock

		// Nav
		KEY("uparrow", KEYBOARD, VK_UP),       // 上矢印
		KEY("downarrow", KEYBOARD, VK_DOWN),   // 下矢印
		KEY("leftarrow", KEYBOARD, VK_LEFT),   // 左矢印
		KEY("rightarrow", KEYBOARD, VK_RIGHT), // 右矢印
		KEY("ins", KEYBOARD, VK_INSERT),       // Insert
		KEY("del", KEYBOARD, VK_DELETE),       // Delete
		KEY("pgdn", KEYBOARD, VK_NEXT),        // Page Down
		KEY("pgup", KEYBOARD, VK_PRIOR),       // Page Up
		KEY("home", KEYBOARD, VK_HOME),        // Home
		KEY("end", KEYBOARD, VK_END),          // End
		KEY("pause", KEYBOARD, VK_PAUSE),      // Pause

		// Numpad 出荷時にあんまり使ってほしくないキー。使うならデバッグ目的にしよう
		KEY("kp_end", KEYBOARD, VK_NUMPAD1),        // Numpad End
		KEY("kp_downarrow", KEYBOARD, VK_NUMPAD2),  // Numpad Down
		KEY("kp_pgdn", KEYBOARD, VK_NUMPAD3),       // Numpad Page Down
		KEY("kp_leftarrow", KEYBOARD, VK_NUMPAD4),  // Numpad Left
		KEY("kp_5", KEYBOARD, VK_NUMPAD5),          // Numpad 5
		KEY("kp_rightarrow", KEYBOARD, VK_NUMPAD6), // Numpad Right
		KEY("kp_home", KEYBOARD, VK_NUMPAD7),       // Numpad Home
		KEY("kp_uparrow", KEYBOARD, VK_NUMPAD8),    // Numpad Up
		KEY("kp_pgup", KEYBOARD, VK_NUMPAD9),       // Numpad Page Up
		KEY("kp_enter", KEYBOARD, VK_RETURN),       // Numpad Enter
		KEY("kp_ins", KEYBOARD, VK_NUMPAD0),        // Numpad Insert
		KEY("kp_del", KEYBOARD, VK_DECIMAL),        // Numpad Delete
		KEY("kp_slash", KEYBOARD, VK_DIVIDE),       // Numpad Slash
		KEY("kp_multiply", KEYBOARD, VK_MULTIPLY),  // Numpad Multiply
		KEY("kp_minus", KEYBOARD, VK_SUBTRACT),     // Numpad Subtract
		KEY("kp_plus", KEYBOARD, VK_ADD),           // Numpad Add

		//---------------------------------------------------------------------
		// マウス
		//---------------------------------------------------------------------
		KEY("mouse1", MOUSE, VK_LBUTTON),  // 左ボタン
		KEY("mouse2", MOUSE, VK_RBUTTON),  // 右ボタン
		KEY("mouse3", MOUSE, VK_MBUTTON),  // 中央ボタン
		KEY("mouse4", MOUSE, VK_XBUTTON1), // サイドボタン1 手前
		KEY("mouse5", MOUSE, VK_XBUTTON2), // サイドボタン2 奥
	};

	const std::unordered_map<InputKey, std::string_view, KeyHash>
	KeyNameTable::sKeyToName = [] {
		std::unordered_map<InputKey, std::string_view, KeyHash> rev;
		for (const auto& [name, key] : sNameToKey)
			rev.emplace(key, name);
		return rev;
	}();
}
