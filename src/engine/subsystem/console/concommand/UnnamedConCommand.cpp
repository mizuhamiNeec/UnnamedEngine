#include "UnnamedConCommand.h"

#include <utility>

#include <engine/subsystem/console/ConsoleSystem.h>
#include <engine/subsystem/console/Log.h>
#include <engine/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	/// @brief コンストラクタ
	/// @param name コマンド名
	/// @param callback 実行時コールバック関数
	/// @param description コマンド説明
	/// @param flags フラグ
	/// @param onComplete 補完時コールバック関数
	UnnamedConCommand::UnnamedConCommand(
		const std::string_view& name,
		OnExecute               callback,
		const std::string_view& description,
		const FCVAR&            flags,
		OnComplete              onComplete
	) : UnnamedConVarBase(name, description, flags),
	    onExecute(std::move(callback)),
	    onComplete(std::move(onComplete)) {
		RegisterSelf();
	}

	/// @brief 自身をコンソールシステムに登録します
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
