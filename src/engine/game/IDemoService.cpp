#include "IDemoService.h"

#include <algorithm>

#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		constexpr uint32_t kDefaultDemoTickRate = 66u;
	}

	uint32_t IDemoService::ResolveConfiguredTickRate() {
		ConsoleSystem* console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			return kDefaultDemoTickRate;
		}

		const int rawTickRate = console->GetConVarValueOr(
			"sv_tickrate",
			static_cast<int>(kDefaultDemoTickRate)
		);
		return std::clamp(static_cast<uint32_t>(std::max(1, rawTickRate)), 1u,
		                  1000u);
	}

	float IDemoService::TickStepSecondsFromRate(const uint32_t tickRate) {
		const uint32_t sanitized = std::clamp(tickRate, 1u, 1000u);
		return 1.0f / static_cast<float>(sanitized);
	}
}
