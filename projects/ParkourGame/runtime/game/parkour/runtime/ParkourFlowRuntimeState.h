#pragma once

namespace Unnamed::ParkourFlowRuntimeState {
	/// @brief 次回の game シーン読み込みで開始カットシーンを再生する要求をセットします。
	void MarkPendingGameStartCutscene();

	/// @brief 開始カットシーン再生要求を1回だけ取り出して消費します。
	/// @return 要求が存在した場合は true
	[[nodiscard]] bool ConsumePendingGameStartCutscene();
}
