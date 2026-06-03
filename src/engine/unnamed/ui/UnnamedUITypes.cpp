#include "UnnamedUITypes.h"

namespace Unnamed::UI {
	UIColor Lerp(const UIColor& a, const UIColor& b, const float t) {
		return UIColor{
			.r = a.r + (b.r - a.r) * t,
			.g = a.g + (b.g - a.g) * t,
			.b = a.b + (b.b - a.b) * t,
			.a = a.a + (b.a - a.a) * t
		};
	}
}
