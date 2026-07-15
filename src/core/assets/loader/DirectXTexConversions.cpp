#include "DirectXTexConversions.h"

namespace Unnamed::DirectXTexConversions {
	bool IsSrgbFormat(const DXGI_FORMAT format) {
		switch (format) {
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			case DXGI_FORMAT_BC7_UNORM_SRGB: return true;
			default: return false;
		}
	}
}
