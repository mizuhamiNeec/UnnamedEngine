#include "Editor.h"

#include <Debug/Debug.h>

#include <Lib/Timer/EngineTimer.h>

#ifdef _DEBUG
#include <imgui_internal.h>
#endif

Editor::Editor(std::shared_ptr<Scene> scene) : scene_(std::move(scene)) {
	Init();
}

void Editor::Init() {}

void Editor::Update([[maybe_unused]] const float deltaTime) {
	// グリッドの表示
	DrawGrid(
		1.0f,
		64,
		{ .x = 0.28f, .y = 0.28f, .z = 0.28f, .w = 1.0f },
		{ .x = 0.39f, .y = 0.2f, .z = 0.02f, .w = 1.0f },
		{ .x = 0.0f, .y = 0.39f, .z = 0.39f, .w = 1.0f },
		{ .x = 0.39f, .y = 0.39f, .z = 0.39f, .w = 1.0f }
	);

#ifdef _DEBUG
	auto viewport = static_cast<ImGuiViewportP*>(static_cast<void*>(ImGui::GetMainViewport()));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.0f);

	if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, 38, window_flags)) {
		if (ImGui::BeginMenuBar()) {
			ImGui::PopStyleVar();

			ImGui::Text("ハリボテ");

			ImGui::BeginDisabled(true);

			// アングルスナップ
			{
				const float windowHeight = ImGui::GetWindowSize().y;
				const char* items[] = {
					"0.25°", "0.5°", "1°", "5°", "5.625°", "11.25°", "15°", "22.5°", "30°", "45°", "90°"
				};
				static int itemCurrentIndex = 6;
				const char* comboLabel = items[itemCurrentIndex];

				ImGui::Text("Angle: ");

				// 垂直中央に配置
				float comboHeight = ImGui::GetFrameHeight();
				float offsetY = (windowHeight - comboHeight) * 0.5f;
				ImGui::SetCursorPosY(offsetY);

				if (ImGui::BeginCombo("##angle", comboLabel)) {
					for (int n = 0; n < IM_ARRAYSIZE(items); ++n) {
						const bool isSelected = (itemCurrentIndex == n);
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}

			ImGui::EndDisabled();

			ImGui::EndMenuBar();
		}
		ImGui::End();
	}
#endif

	if (scene_) {
		scene_->Update(EngineTimer::GetScaledDeltaTime());
	}
}

void Editor::Render() {
	if (scene_) {
		scene_->Render();
	}
}

void Editor::DrawGrid(
	const float gridSize, const float range, const Vec4& color, const Vec4& majorColor,
	const Vec4& axisColor, const Vec4& minorColor
) {
	// const float range = 16384.0f;
	constexpr float majorInterval = 1024.0f;
	const float minorInterval = gridSize * 8.0f;

	for (float x = -range; x <= range; x += gridSize) {
		Vec4 lineColor = color;
		if (fmod(x, majorInterval) == 0) {
			lineColor = majorColor;
		} else if (fmod(x, minorInterval) == 0) {
			lineColor = minorColor;
		}
		if (x == 0) {
			lineColor = axisColor;
		}
		Debug::DrawLine(Vec3(x, 0, -range), Vec3(x, 0, range), lineColor);
	}

	for (float z = -range; z <= range; z += gridSize) {
		Vec4 lineColor = color;
		if (fmod(z, majorInterval) == 0) {
			lineColor = majorColor;
		} else if (fmod(z, minorInterval) == 0) {
			lineColor = minorColor;
		}
		if (z == 0) {
			lineColor = axisColor;
		}
		Debug::DrawLine(Vec3(-range, 0, z), Vec3(range, 0, z), lineColor);
	}
}