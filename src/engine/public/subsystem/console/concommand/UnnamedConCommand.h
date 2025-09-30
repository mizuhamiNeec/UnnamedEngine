#pragma once
#include <functional>

#include <engine/public/subsystem/console/concommand/base/UnnamedConVarBase.h>

namespace Unnamed {
	class UnnamedConCommand : public UnnamedConVarBase {
	public:
		// コールバック[引数: トークン化されたコマンド]
		using OnExecute  = std::function<bool(std::vector<std::string>)>;
		using OnComplete = std::function<void()>;

		UnnamedConCommand(
			const std::string_view& name,
			OnExecute               callback,
			const std::string_view& description,
			const FCVAR&            flags      = FCVAR::NONE,
			OnComplete              onComplete = {}
		);

		OnExecute  onExecute;  // コマンドを実行した際に呼び出されるコールバック
		OnComplete onComplete; // コマンドの実行が完了した際に呼び出されるコールバック

	private:
		void RegisterSelf();
	};
}
