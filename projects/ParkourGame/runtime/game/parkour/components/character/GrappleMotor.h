#pragma once

#include "core/math/Math.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	struct GrappleState {
		Vec3 anchorPoint = Vec3::zero;

		float ropeLength    = 0.0f;
		float minRopeLength = Math::HtoM(32.0f);
		float maxRopeLength = Math::HtoM(0xFFFFFF);

		bool isActive  = false;
		bool isLatched = false;

		void Reset() {
			isActive    = false;
			isLatched   = false;
			anchorPoint = Vec3::zero;
			ropeLength  = 0.0f;
		}
	};
}
