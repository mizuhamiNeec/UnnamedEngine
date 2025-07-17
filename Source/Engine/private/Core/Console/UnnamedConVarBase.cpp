#include <public/Core/Console/UnnamedConVarBase.h>

namespace Core {
	bool UnnamedConVarBase::IsCommand() const {
		return false; // 変数はコマンドじゃあないッ!
	}
}
