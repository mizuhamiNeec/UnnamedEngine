#include "BaseKinematicCollisionResolver.h"

#include <cmath>

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	BaseKinematicCollisionResolver::BaseKinematicCollisionResolver(
		Physics::Engine* engine
	) : mEngine(engine) {}

	BaseKinematicCollisionResolver::~BaseKinematicCollisionResolver() {}

	Vec3 BaseKinematicCollisionResolver::ClipVelocity(
		const Vec3& in, const Vec3& normal, const float overbounce
	) {
		const float backoff = in.Dot(normal) * overbounce;
		Vec3        out     = in - normal * backoff;

		constexpr float stopEps = 1e-6f;
		if (std::fabs(out.x) < stopEps) {
			out.x = 0.0f;
		}
		if (std::fabs(out.y) < stopEps) {
			out.y = 0.0f;
		}
		if (std::fabs(out.z) < stopEps) {
			out.z = 0.0f;
		}
		return out;
	}

	Physics::Engine* BaseKinematicCollisionResolver::GetPhysics() const {
		return mEngine;
	}
}
