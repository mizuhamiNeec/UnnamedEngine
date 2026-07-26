#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	/// @brief パフォーマンスプロファイラー。フレームごとのサンプルを記録し、履歴を保持します。
	class Profiler {
	public:
		/// @brief SampleViewは、profiler sampleの名称、経過時間、nest深度を表示用に参照します
		struct SampleView {
			std::string_view          name;
			const std::vector<float>* history           = nullptr;
			uint32_t                  historyWriteIndex = 0;
			float                     currentMs         = 0.0f;
			float                     averageMs         = 0.0f;
			float                     maxMs             = 0.0f;
			uint32_t                  colorIndex        = 0;
		};

		/// @brief ScopeTimerは、構築から破棄までの経過時間を計測し、対応するProfilerへRAIIで記録します
		class ScopeTimer {
		public:
			ScopeTimer(Profiler* profiler, std::string_view name);
			~ScopeTimer();

			// コピー禁止
			ScopeTimer(const ScopeTimer&)            = delete;
			ScopeTimer& operator=(const ScopeTimer&) = delete;

		private:
			using Clock = std::chrono::steady_clock;

			Profiler*         mProfiler = nullptr;
			std::string_view  mName;
			Clock::time_point mStart = {};
		};

		/// @brief フレーム開始
		void BeginFrame();

		/// @brief フレーム終了。サンプルの履歴を更新します。
		void EndFrame();

		/// @brief サンプルを追加します。サンプルはフレームごとに記録され、履歴に保存されます。
		/// @param name サンプルの名前。プロファイラー内で識別されます。
		/// @param milliseconds サンプルの値。通常は処理にかかった時間をミリ秒単位で表します。
		void AddSample(std::string_view name, float milliseconds);

		/// @brief 登録されたサンプルの一覧を取得します。各サンプルには履歴データへのポインタが含まれます。
		/// @return 登録されたサンプルの一覧。各サンプルには履歴データへのポインタが含まれます。
		[[nodiscard]] const std::vector<SampleView>& GetSamples() const;

		/// @brief サンプルの履歴サイズを取得します。プロファイラーが保持する履歴のフレーム数を表します。
		/// @return サンプルの履歴サイズ。プロファイラーが保持する履歴のフレーム数を表します。
		[[nodiscard]] uint32_t GetHistorySize() const;

		/// @brief プロファイラーが記録したフレーム数を取得します。プロファイラーが開始されてからのフレーム数を表します。
		/// @return プロファイラーが記録したフレーム数。プロファイラーが開始されてからのフレーム数を表します。
		[[nodiscard]] uint64_t GetFrameCount() const;

	private:
		/// @brief SampleDataは、named profiling scopeのframe内時間、履歴、平均・最大時間を保持します
		struct SampleData {
			std::string        name;
			std::vector<float> history;
			float              frameAccumulatedMs = 0.0f;
			float              currentMs          = 0.0f;
			float              averageMs          = 0.0f;
			float              maxMs              = 0.0f;
			uint32_t           colorIndex         = 0;
		};

		/// @brief 名前に対応するサンプルデータを取得します。存在しない場合は新しいサンプルデータを作成して返します。
		/// @param name サンプルの名前。プロファイラー内で識別されます。
		/// @return 名前に対応するサンプルデータ。存在しない場合は新しいサンプルデータを作成して返します。
		SampleData& GetOrCreateSample(std::string_view name);

		static constexpr uint32_t kHistorySize = 180;

		std::vector<SampleData>                 mSamples;
		std::unordered_map<std::string, size_t> mSampleIndices;
		mutable std::vector<SampleView>         mSampleViews;
		uint64_t                                mFrameCount        = 0;
		uint32_t                                mHistoryWriteIndex = 0;
	};
}
