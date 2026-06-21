#pragma once
#include <string>

#include <engine/unnamed/subsystem/console/ConsoleFlags.h>

namespace Unnamed {
	/// @brief コンソールコマンド/変数基底クラス
	class ConCommandBase {
	public:
		ConCommandBase(
			const std::string_view& name, const std::string_view& description,
			const FCVAR             flags = FCVAR::NONE
		) : mName(name),
		    mDescription(description),
		    mFlags(flags) {
		}

		virtual ~ConCommandBase() = default;

		[[nodiscard]] virtual bool IsCommand() const {
			return false;
		};

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
