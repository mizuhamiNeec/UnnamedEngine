#pragma once

#include <dxgiformat.h>

namespace Unnamed::DirectXTexConversions {
	/// @brief DirectXTexから得たDXGI_FORMATがsRGB形式か判定します。
	/// @param format 判定するDXGI_FORMAT。
	/// @return sRGB形式ならtrue。
	[[nodiscard]] bool IsSrgbFormat(DXGI_FORMAT format);
}
