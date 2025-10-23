#include <engine/subsystem/console/concommand/base/UnnamedConVarBase.h>

namespace Unnamed {
	/// @brief このコンソール変数がコマンドかどうかを判定します
	bool UnnamedConVarBase::IsCommand() const {
		return false;
	}
}
