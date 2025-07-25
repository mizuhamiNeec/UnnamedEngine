#pragma once
#include <cstdint>

namespace Unnamed {
	enum class FCVAR : uint32_t {
		//--------Shared-------------------------------------------------------
		NONE = 0, // フラグなし

		UNREGISTERED = 1 << 0, // これが設定されている場合、リンクリストなどに追加しないでください。

		// 開発用。通常のリリース版では無効ですが、developer 0/1 で切り替えできます。
		DEVELOPMENTONLY = 1 << 1,
		HIDDEN          = 1 << 4, // コンソールのサジェストに表示されない

		//--------ConVar Only--------------------------------------------------
		PROTECTED = 1 << 5,  // サーバーCVARですが、パスワードなどであるため、データは送信しません。
		ARCHIVE   = 1 << 7,  // コンフィグファイルに保存するようになります。
		NOTIFY    = 1 << 8,  // プレイヤーに変更が通知される。
		USERINFO  = 1 << 9,  // クライアントの情報文字列を変更します。
		CHEAT     = 1 << 14, // シングルプレイ/デバッグ/マルチプレイ & sv_cheats 1 のときのみ有効。

		PRINTABLEONLY = 1 << 10, // ユーザー/プレイヤーに見せても問題ない文字列のみを許可します。

		// TODO: ネットワークに対応しよう

		// このフラグがREPLICATEDで設定されている場合、値の変更をログファイルやコンソールに記録しません。
		UNLOGGED = 1 << 11,

		REPLICATED = 1 << 13, // サーバーの値をクライアントに反映させる設定。

		NOT_CONNECTED = 1 << 22, // クライアントがサーバーに接続している場合、このcvarは変更できません。
	};

	const char* ToString(FCVAR e);

	FCVAR& operator|=(FCVAR& lhs, const FCVAR& rhs);
	bool   operator&(const FCVAR& lhs, const FCVAR& rhs);
	bool   operator!=(const FCVAR& lhs, const FCVAR& rhs);
}
