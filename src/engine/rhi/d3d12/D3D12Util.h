#pragma once
#include <dxgiformat.h>

#include "engine/rhi/RhiTypes.h"

namespace Unnamed::Rhi {
	void           Throw(HRESULT hr);
	DXGI_FORMAT    ToDxgiFormat(TEXTURE_FORMAT format);
	TEXTURE_FORMAT ToTextureFormat(DXGI_FORMAT format);
}
