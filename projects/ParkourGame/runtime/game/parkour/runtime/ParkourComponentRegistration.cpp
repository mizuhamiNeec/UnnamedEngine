#include "ParkourComponentRegistration.h"

#include "core/ComponentRegistry.h"

namespace Unnamed {
	void RegisterParkourGameComponents(ComponentRegistry& componentRegistry) {
		// Parkour 側は現在 static REGISTER_COMPONENT を主経路として運用しています。
		// 登録漏れ回避は App 側リンク設定（/WHOLEARCHIVE）で担保します。
		(void)componentRegistry;
	}
}
