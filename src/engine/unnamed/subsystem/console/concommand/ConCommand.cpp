#include "ConCommand.h"

#include <utility>

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

	namespace Unnamed {
	/// @brief デストラクタ
	ConCommand::~ConCommand() {
		UnregisterFromConsoleSystem();
	}

	/// @brief コンストラクタ
	/// @param name コマンド名
	/// @param callback 実行時コールバック関数
	/// @param description コマンド説明
	/// @param flags フラグ
	/// @param onComplete 補完時コールバック関数
	ConCommand::ConCommand(
		const std::string_view& name,
		OnExecute               callback,
		const std::string_view& description,
		const FCVAR&            flags,
		OnComplete              onComplete
	) : ConCommandBase(name, description, flags),
	    onExecute(std::move(callback)),
	    onComplete(std::move(onComplete)) {
		RegisterSelf();
	}

	bool ConCommand::IsCommand() const {
		return true;
	}

	/// @brief 自身をコンソールシステムに登録します
	void ConCommand::RegisterSelf() {
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
	}
}
