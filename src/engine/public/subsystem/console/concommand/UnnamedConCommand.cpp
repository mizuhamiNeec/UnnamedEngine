#include "UnnamedConCommand.h"

#include <utility>

#include <engine/public/subsystem/console/ConsoleSystem.h>
#include <engine/public/subsystem/console/Log.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	UnnamedConCommand::UnnamedConCommand(
		const std::string_view& name,
		OnExecute         callback,
		const std::string_view& description,
		const FCVAR&      flags,
		OnComplete        onComplete
	) : UnnamedConVarBase(name, description, flags),
	    onExecute(std::move(callback)),
	    onComplete(std::move(onComplete)) {
		RegisterSelf();
	}

	void UnnamedConCommand::RegisterSelf() {
		const auto console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			Error(
				"ConCommand",
				"登録に失敗しました: {}",
				GetName()
			);
			return;
		}
		console->RegisterConCommand(this);

		DevMsg(
			"ConCommand",
			"Name: {}\n"
			"Description: {}\n",
			GetName(),
			GetDescription()
		);
	}
}
