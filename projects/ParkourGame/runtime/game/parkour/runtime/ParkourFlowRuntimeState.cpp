#include "ParkourFlowRuntimeState.h"

#include <atomic>

namespace Unnamed::ParkourFlowRuntimeState {
	namespace {
		std::atomic<bool> gPendingGameStartCutscene = false;
	}

	void MarkPendingGameStartCutscene() {
		gPendingGameStartCutscene.store(true, std::memory_order_release);
	}

	bool ConsumePendingGameStartCutscene() {
		return gPendingGameStartCutscene.exchange(false, std::memory_order_acq_rel);
	}
}
