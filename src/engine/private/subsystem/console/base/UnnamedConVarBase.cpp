#include <engine/public/subsystem/console/base/UnnamedConVarBase.h>

namespace Unnamed {
	bool UnnamedConVarBase::IsCommand() const {
		return false; // 変数はコマンドじゃあないッ!
	}
}
