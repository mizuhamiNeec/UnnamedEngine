#ifdef _DEBUG
#include "LevelEditorTool.h"

#include <algorithm>
#include <functional>
#include <imgui.h>
#include <limits>
#include <string_view>

#include "engine/profiler/Profiler.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] std::string_view ExtractProfilerCategory(
			const std::string_view sampleName
		) {
			const size_t dotPos = sampleName.find_last_of('.');
			if (dotPos == std::string_view::npos || dotPos == 0) {
				return sampleName;
			}
			return sampleName.substr(0, dotPos);
		}

		[[nodiscard]] ImVec4 BuildCategoryColor(
			const std::string_view sampleName
		) {
			const std::string_view category = ExtractProfilerCategory(
				sampleName);
			const size_t hashValue = std::hash<std::string_view>{}(
				category
			);
			const float hue = static_cast<float>(hashValue % 1024u) / 1024.0f;
			return ImColor::HSV(hue, 0.75f, 0.95f);
		}

		[[nodiscard]] float DistanceToLineSegmentSquared(
			const ImVec2& point,
			const ImVec2& segmentStart,
			const ImVec2& segmentEnd
		) {
			const ImVec2 segment(
				segmentEnd.x - segmentStart.x,
				segmentEnd.y - segmentStart.y
			);
			const float segmentLengthSquared =
				segment.x * segment.x + segment.y * segment.y;
			if (segmentLengthSquared <= 0.000001f) {
				const float dx = point.x - segmentStart.x;
				const float dy = point.y - segmentStart.y;
				return dx * dx + dy * dy;
			}

			const float t = std::clamp(
				((point.x - segmentStart.x) * segment.x +
				 (point.y - segmentStart.y) * segment.y) /
				segmentLengthSquared,
				0.0f,
				1.0f
			);
			const ImVec2 nearestPoint(
				segmentStart.x + segment.x * t,
				segmentStart.y + segment.y * t
			);
			const float dx = point.x - nearestPoint.x;
			const float dy = point.y - nearestPoint.y;
			return dx * dx + dy * dy;
		}
	}

	void LevelEditorTool::DrawProfilerWindow() {
		if (!mShowProfilerWindow) {
			return;
		}

		const bool bOpen = ImGui::Begin("Profiler", &mShowProfilerWindow);

		if (bOpen) {
			const Profiler* profiler = ServiceLocator::Get<Profiler>();
			if (!profiler) {
				ImGui::TextUnformatted("Profiler is unavailable.");
				ImGui::End();
				return;
			}

			ImGui::Text(
				"Frames: %llu  History: %u",
				static_cast<unsigned long long>(profiler->GetFrameCount()),
				profiler->GetHistorySize()
			);
			ImGui::Separator();

			const auto& samples   = profiler->GetSamples();
			float       globalMax = 1.0f;
			for (const auto& sample : samples) {
				if (!sample.history || sample.history->empty()) {
					continue;
				}
				globalMax = std::max(globalMax, sample.maxMs);
			}

			const ImVec2 canvasSize(
				ImGui::GetContentRegionAvail().x,
				180.0f
			);
			ImGui::InvisibleButton("##ProfilerCanvas", canvasSize);
			const ImVec2 canvasMin     = ImGui::GetItemRectMin();
			const ImVec2 canvasMax     = ImGui::GetItemRectMax();
			ImDrawList*  drawList      = ImGui::GetWindowDrawList();
			const bool   canvasHovered = ImGui::IsItemHovered();
			const ImVec2 mousePos      = ImGui::GetMousePos();

			drawList->AddRectFilled(
				canvasMin,
				canvasMax,
				ImGui::GetColorU32(ImGuiCol_FrameBg),
				4.0f
			);
			drawList->AddRect(
				canvasMin,
				canvasMax,
				ImGui::GetColorU32(ImGuiCol_Border),
				4.0f
			);

			const float paddedMinX  = canvasMin.x + 8.0f;
			const float paddedMaxX  = canvasMax.x - 8.0f;
			const float paddedMinY  = canvasMin.y + 8.0f;
			const float paddedMaxY  = canvasMax.y - 8.0f;
			const float graphWidth  = std::max(1.0f, paddedMaxX - paddedMinX);
			const float graphHeight = std::max(1.0f, paddedMaxY - paddedMinY);

			for (int grid = 1; grid < 4; ++grid) {
				const float t = static_cast<float>(grid) / 4.0f;
				const float y = paddedMaxY - graphHeight * t;
				drawList->AddLine(
					ImVec2(paddedMinX, y),
					ImVec2(paddedMaxX, y),
					ImGui::GetColorU32(ImVec4(1, 1, 1, 0.08f))
				);
			}

			const Profiler::SampleView* hoveredSample = nullptr;
			float hoveredDistanceSquared = std::numeric_limits<float>::max();
			constexpr float kHoverDistanceThresholdSquared = 16.0f;

			for (const auto& sample : samples) {
				if (!sample.history || sample.history->size() < 2) {
					continue;
				}

				const ImVec4 colorVec = BuildCategoryColor(sample.name);
				const ImU32  color    = ImGui::GetColorU32(colorVec);

				ImVec2 prev    = {};
				bool   hasPrev = false;
				for (size_t i = 0; i < sample.history->size(); ++i) {
					const size_t historyIndex =
						(sample.historyWriteIndex + i) % sample.history->size();
					const float value = (*sample.history)[historyIndex];
					const float x     = paddedMinX +
					                    static_cast<float>(i) /
					                    static_cast<float>(
						                    sample.history->size() -
						                    1) *
					                    graphWidth;
					const float y = paddedMaxY -
					                value / (globalMax * 1.1f) * graphHeight;
					const ImVec2 point(
						x, std::clamp(y, paddedMinY, paddedMaxY)
					);
					if (hasPrev) {
						drawList->AddLine(prev, point, color, 1.5f);
						if (canvasHovered) {
							const float distanceSquared =
								DistanceToLineSegmentSquared(
									mousePos, prev, point);
							if (distanceSquared < hoveredDistanceSquared) {
								hoveredDistanceSquared = distanceSquared;
								hoveredSample          = &sample;
							}
						}
					}
					prev    = point;
					hasPrev = true;
				}
			}

			if (
				canvasHovered &&
				hoveredSample != nullptr &&
				hoveredDistanceSquared <= kHoverDistanceThresholdSquared
			) {
				ImGui::BeginTooltip();
				ImGui::Text(
					"%.*s",
					static_cast<int>(hoveredSample->name.size()),
					hoveredSample->name.data()
				);
				ImGui::EndTooltip();
			}

			for (const auto& sample : samples) {
				if (!sample.history || sample.history->empty()) {
					continue;
				}

				const ImVec4 color = BuildCategoryColor(sample.name);

				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::BulletText(
					"%.*s  %.3f ms  avg %.3f ms  max %.3f ms",
					static_cast<int>(sample.name.size()),
					sample.name.data(),
					sample.currentMs,
					sample.averageMs,
					sample.maxMs
				);
				ImGui::PopStyleColor();
			}
		}
		ImGui::End();
	}
}

#endif
