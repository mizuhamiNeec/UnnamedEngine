#include "TweenEase.h"

#include <algorithm>

#include "core/math/Math.h"

namespace Unnamed {
	//-------------------------------------------------------------------------
	// ソース: https://easings.net/ja 
	//-------------------------------------------------------------------------
	float TweenEase::Evaluate(const EASE_TYPE easeType, float x) {
		x = std::clamp(x, 0.0f, 1.0f);

		constexpr auto c1 = 1.70158f;
		constexpr auto c2 = c1 * 1.525f;
		constexpr auto c3 = c1 + 1.0f;
		constexpr auto c4 = 2.0f * Math::pi / 3.0f;
		constexpr auto c5 = 2.0f * Math::pi / 4.5f;

		switch (easeType) {
			case EASE_TYPE::LINEAR: return x;
			case EASE_TYPE::IN_SINE: {
				return 1.0f - cos(x * Math::pi * 0.5f);
			}
			case EASE_TYPE::OUT_SINE: {
				return sin(x * Math::pi * 0.5f);
			}
			case EASE_TYPE::IN_OUT_SINE: {
				return -(cos(Math::pi * x) - 1.0f) * 0.5f;
			}
			case EASE_TYPE::IN_QUAD: {
				return x * x;
			}
			case EASE_TYPE::OUT_QUAD: {
				return 1.0f - (1.0f - x) * (1.0f - x);
			}
			case EASE_TYPE::IN_OUT_QUAD: {
				return x < 0.5f ?
					       2.0f * x * x :
					       1.0f - pow(-2.0f * x + 2.0f, 2.0f) * 0.5f;
			}
			case EASE_TYPE::IN_CUBIC: {
				return x * x * x;
			}
			case EASE_TYPE::OUT_CUBIC: {
				return 1.0f - pow(1.0f - x, 3.0f);
			}
			case EASE_TYPE::IN_OUT_CUBIC: {
				return x < 0.5f ?
					       4.0f * x * x * x :
					       1.0f - pow(-2.0f * x + 2.0f, 3.0f) * 0.5f;
			}
			case EASE_TYPE::IN_QUART: {
				return x * x * x * x;
			}
			case EASE_TYPE::OUT_QUART: {
				return 1.0f - pow(1.0f - x, 4.0f);
			}
			case EASE_TYPE::IN_OUT_QUART: {
				return x < 0.5f ?
					       8 * x * x * x * x :
					       1.0f - pow(-2.0f * x + 2.0f, 4.0f) * 0.5f;
			}
			case EASE_TYPE::IN_QUINT: {
				return x * x * x * x * x;
			}
			case EASE_TYPE::OUT_QUINT: {
				return 1.0f - pow(1.0f - x, 5.0f);
			}
			case EASE_TYPE::IN_OUT_QUINT: {
				return x < 0.5f ?
					       16.0f * x * x * x * x * x :
					       1.0f - pow(-2.0f * x + 2.0f, 5.0f) * 0.5f;
			}
			case EASE_TYPE::IN_EXPO: {
				return x == 0.0f ? 0.0f : pow(2.0f, 10.0f * x - 10.0f);
			}
			case EASE_TYPE::OUT_EXPO: {
				return x == 1.0f ? 1.0f : 1.0f - pow(2.0f, -10.0f * x);
			}
			case EASE_TYPE::IN_OUT_EXPO: {
				return x == 0.0f ?
					       0.0f :
					       x == 1.0f ?
					       1.0f :
					       x < 0.5f ?
					       pow(2.0f, 20.0f * x - 10.0f) * 0.5f :
					       (2.0f - pow(2.0f, -20.0f * x + 10.0f)) * 0.5f;
			}
			case EASE_TYPE::IN_CIRC: {
				return 1.0f - sqrt(1.0f - pow(x, 2.0f));
			}
			case EASE_TYPE::OUT_CIRC: {
				return sqrt(1.0f - pow(x - 1.0f, 2.0f));
			}
			case EASE_TYPE::IN_OUT_CIRC: {
				return x < 0.5f ?
					       (1.0f - sqrt(1.0f - pow(2.0f * x, 2.0f))) * 0.5f :
					       (sqrt(
						        1.0f - pow(-2.0f * x + 2.0f, 2.0f)
					        ) + 1.0f) * 0.5f;
			}
			case EASE_TYPE::IN_BACK: {
				return c3 * x * x * x - c1 * x * x;
			}
			case EASE_TYPE::OUT_BACK: {
				return 1.0f + c3 * pow(x - 1.0f, 3.0f) + c1 * pow(
					       x - 1.0f, 2.0f
				       );
			}
			case EASE_TYPE::IN_OUT_BACK: {
				return x < 0.5f ?
					       pow(2.0f * x, 2.0f) * (
						       (c2 + 1.0f) * 2.0f * x - c2) * 0.5f :
					       (pow(2.0f * x - 2.0f, 2.0f) * (
						        (c2 + 1.0f) * (x * 2.0f - 2.0f) + c2) +
					        2.0f) * 0.5f;
			}
			case EASE_TYPE::IN_ELASTIC: {
				return x == 0.0f ?
					       0.0f :
					       x == 1.0f ?
					       1.0f :
					       -pow(2.0f, 10.0f * x - 10.0f) * sin(
						       (x * 10.0f - 10.75f) * c4
					       );
			}
			case EASE_TYPE::OUT_ELASTIC: {
				return x == 0.0f ?
					       0.0f :
					       x == 1.0f ?
					       1.0f :
					       pow(2.0f, -10.0f * x) * sin(
						       (x * 10.0f - 0.75f) * c4
					       ) + 1.0f;
			}
			case EASE_TYPE::IN_OUT_ELASTIC: {
				return x == 0.0f ?
					       0.0f :
					       x == 1.0f ?
					       1.0f :
					       x < 0.5f ?
					       -(pow(2.0f, 20.0f * x - 10.0f) * sin(
						         (20.0f * x - 11.125f) * c5
					         )) * 0.5f :
					       pow(2.0f, -20.0f * x + 10.0f) * sin(
						       (20.0f * x - 11.125f) * c5
					       ) * 0.5f + 1.0f;
			}
			case EASE_TYPE::IN_BOUNCE: {
				return 1.0f - Evaluate(EASE_TYPE::OUT_BOUNCE, 1.0f - x);
			}
			case EASE_TYPE::OUT_BOUNCE: {
				constexpr auto n1 = 7.5625f;
				constexpr auto d1 = 2.75f;

				if (x < 1.0f / d1) {
					return n1 * x * x;
				}
				if (
					x < 2.0f / d1) {
					return n1 * (x - 1.0f / d1) * (x - 1.0f / d1) +
					       0.75f;
				}
				if (x < 2.5f / d1) {
					return n1 * (x - 2.0f / d1) * (x - 2.0f / d1) +
					       0.9375f;
				}
				return n1 * (x - 2.625f / d1) * (x - 2.625f / d1) +
				       0.984375f;
			}
			case EASE_TYPE::IN_OUT_BOUNCE: {
				return x < 0.5f ?
					       (1.0f - Evaluate(
						        EASE_TYPE::OUT_BOUNCE, 1.0f - 2.0f * x
					        )) * 0.5f :
					       Evaluate(
						       EASE_TYPE::OUT_BOUNCE, 2.0f * x - 1.0f
					       ) * 0.5f + 0.5f;
			}
			default: {
				return x;
			}
		}
	}
}
