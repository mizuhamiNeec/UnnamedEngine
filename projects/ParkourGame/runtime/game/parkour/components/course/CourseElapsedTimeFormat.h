#pragma once

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

namespace Unnamed {
	/// @brief CourseElapsedTimePartsは、パルクールゲームプレイで表示・計算しやすい単位へ分解した値を保持します
	struct CourseElapsedTimeParts {
		int minutes  = 0;
		int seconds  = 0;
		int fraction = 0;
	};

	/// @brief 経過秒を `分`,`秒`,`百分秒` に分解します。
	[[nodiscard]] inline CourseElapsedTimeParts SplitCourseElapsedTime(
		const float elapsedSeconds
	) {
		const int totalCentiseconds = static_cast<int>(
			std::lround(std::max(0.0f, elapsedSeconds) * 100.0f)
		);
		const int fraction = totalCentiseconds % 100;
		const int totalSeconds = totalCentiseconds / 100;
		const int seconds = totalSeconds % 60;
		const int minutes = totalSeconds / 60;
		return CourseElapsedTimeParts{
			.minutes = minutes,
			.seconds = seconds,
			.fraction = fraction,
		};
	}

	/// @brief 経過秒を `分,秒.百分秒` 形式の表示文字列へ変換します。
	[[nodiscard]] inline std::string FormatCourseElapsedTime(
		const float elapsedSeconds
	) {
		const CourseElapsedTimeParts time = SplitCourseElapsedTime(elapsedSeconds);
		return std::format(
			"{:02},{:02}.{:02}",
			time.minutes,
			time.seconds,
			time.fraction
		);
	}
}
