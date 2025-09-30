#include <pch.h>

#include <engine/public/subsystem/console/concommand/base/UnnamedConCommandBase.h>
#include <engine/public/subsystem/console/ConsoleFlags.h>

namespace Unnamed {
	const char* ToString(FCVAR e) {
		switch (e) {
		case FCVAR::NONE: return "NONE";
		case FCVAR::UNREGISTERED: return "UNREGISTERED";
		case FCVAR::DEVELOPMENTONLY: return "DEVELOPMENTONLY";
		case FCVAR::HIDDEN: return "HIDDEN";
		case FCVAR::PROTECTED: return "PROTECTED";
		case FCVAR::ARCHIVE: return "ARCHIVE";
		case FCVAR::NOTIFY: return "NOTIFY";
		case FCVAR::USERINFO: return "USERINFO";
		case FCVAR::CHEAT: return "CHEAT";
		case FCVAR::PRINTABLEONLY: return "PRINTABLEONLY";
		case FCVAR::UNLOGGED: return "UNLOGGED";
		case FCVAR::REPLICATED: return "REPLICATED";
		case FCVAR::NOT_CONNECTED: return "NOT_CONNECTED";
		default: return "unknown";
		}
	}

	FCVAR& operator|=(FCVAR& lhs, const FCVAR& rhs) {
		lhs = static_cast<FCVAR>(static_cast<int>(lhs) | static_cast<int>(rhs));
		return lhs;
	}

	bool operator&(const FCVAR& lhs, const FCVAR& rhs) {
		return (static_cast<int>(lhs) & static_cast<int>(rhs)) != 0;
	}

	bool operator!=(const FCVAR& lhs, const FCVAR& rhs) {
		return static_cast<int>(lhs) != static_cast<int>(rhs);
	}

	/// @brief フラグを持っているかどうかをチェックします。
	/// @param flags チェックするフラグ
	/// @return そのフラグが設定されているかどうか
	bool UnnamedConCommandBase::HasFlags(const FCVAR& flags) const {
		return (mFlags & flags);
	}

	/// @brief フラグを追加します。
	/// @param flag 追加するフラグ
	void UnnamedConCommandBase::AddFlag(const FCVAR& flag) {
		mFlags |= flag;
	}

	/// @brief フラグを削除します。
	/// @param flag 削除するフラグ 
	void UnnamedConCommandBase::RemoveFlag(const FCVAR& flag) {
		if (HasFlags(flag)) {
			mFlags = static_cast<FCVAR>(static_cast<int>(mFlags) & ~static_cast<
				int>(flag));
		} else {
			// Log::Warning(
			// 	"ConCommand",
			// 	"存在しないフラグを削除しようとしました: {}",
			// 	ToString(flag)
			// );
		}
	}

	/// @brief 名前を取得します。
	/// @return 名前
	std::string_view UnnamedConCommandBase::GetName() const {
		return mName;
	}

	/// @brief 説明を取得します。
	/// @return 説明
	std::string_view UnnamedConCommandBase::GetDescription() const {
		return mDescription;
	}
}
