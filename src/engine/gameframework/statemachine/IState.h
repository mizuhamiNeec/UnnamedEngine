#pragma once
#include <functional>
#include <string_view>
#include <engine/gameframework/statemachine/Event.h>

namespace Unnamed {
	template <class TContext>
	class IState {
	public:
		virtual                                ~IState() = default;
		[[nodiscard]] virtual std::string_view Name() const = 0;

		virtual void OnEnter(TContext& ctx, const Event* cause) {
			(void)ctx;
			(void)cause;
		}

		virtual void OnExit(TContext& ctx, const Event* cause) {
			(void)ctx;
			(void)cause;
		}

		virtual void Update(TContext& ctx, const float dt) {
			(void)ctx;
			(void)dt;
		}
	};

	template <class TContext>
	struct Transition {
		StateId                                            from;
		StateId                                            to;
		std::function<bool(const TContext&, const Event*)> guard{};
	};
}
