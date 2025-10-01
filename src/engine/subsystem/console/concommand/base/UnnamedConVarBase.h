#pragma once

#include <engine/subsystem/console/concommand/base/UnnamedConCommandBase.h>

namespace Unnamed {
	class UnnamedConVarBase : public UnnamedConCommandBase {
	public:
		// 基底クラスのコンストラクタを呼び出す この書き方知らんかった...
		using UnnamedConCommandBase::UnnamedConCommandBase;

		~UnnamedConVarBase() override = default;

		[[nodiscard]] bool IsCommand() const override;
	};
}
