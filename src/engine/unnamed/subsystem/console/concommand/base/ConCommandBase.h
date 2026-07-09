#pragma once
#include <cstdint>
#include <string>

#include <engine/unnamed/subsystem/console/ConsoleFlags.h>

namespace Unnamed {
	class ConsoleSystem;

	enum class ConsoleRegistrationKind : uint8_t {
		None,
		Command,
		ConVar,
	};

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

		void AttachToConsoleSystem(
			ConsoleSystem&          consoleSystem,
			ConsoleRegistrationKind registrationKind
		) noexcept;
		void DetachFromConsoleSystem(
			const ConsoleSystem& consoleSystem
		) noexcept;

	protected:
		void UnregisterFromConsoleSystem() noexcept;

	protected:
		std::string mName;
		std::string mDescription;
		FCVAR       mFlags;

	private:
		ConsoleSystem*          mRegisteredConsoleSystem = nullptr;
		ConsoleRegistrationKind mRegistrationKind =
			ConsoleRegistrationKind::None;
	};
}
