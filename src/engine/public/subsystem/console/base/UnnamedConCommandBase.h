#pragma once
#include <string>

#include <engine/public/subsystem/console/ConsoleFlags.h>

namespace Unnamed {
	//-----------------------------------------------------------------------------
	// Purpose: コンソールコマンドの基本クラス
	//-----------------------------------------------------------------------------
	class UnnamedConCommandBase {
	public:
		UnnamedConCommandBase(
			const std::string_view& name, const std::string_view& description,
			const FCVAR             flags = FCVAR::NONE
		) : mName(name), mDescription(description), mFlags(flags) {
		}

		virtual ~UnnamedConCommandBase() = default;

		[[nodiscard]] virtual bool IsCommand() const = 0;

		[[nodiscard]] bool HasFlags(const FCVAR& flags) const;
		void               AddFlag(const FCVAR& flag);
		void               RemoveFlag(const FCVAR& flag);

		[[nodiscard]] std::string_view GetName() const;
		[[nodiscard]] std::string_view GetDescription() const;

	protected:
		std::string mName;
		std::string mDescription;
		FCVAR       mFlags;
	};
}
