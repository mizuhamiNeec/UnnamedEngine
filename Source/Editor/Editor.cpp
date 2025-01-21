#include "Editor.h"

#include <Debug/Debug.h>

#include <Lib/Timer/EngineTimer.h>

#include "Camera/CameraManager.h"
#include "ImGuiManager/Icons.h"
#include "Input/InputSystem.h"
#include "Lib/Console/ConVarManager.h"
#include "Lib/Utils/StrUtils.h"

#ifdef _DEBUG
#include <imgui_internal.h>
#endif

Editor::Editor(std::shared_ptr<Scene> scene) : scene_(std::move(scene)) {
	Init();
}

void Editor::Init() {
	// カメラの作成
	cameraEntity_ = std::make_unique<Entity>("editorcamera");
	cameraEntity_->GetTransform()->SetLocalPos(Vec3::forward * -5.0f + Vec3::up * 2.0f);
	cameraEntity_->GetTransform()->SetLocalRot(Quaternion::Euler(Vec3::right * 15.0f * Math::deg2Rad));

	// 生ポインタを取得
	CameraComponent* rawCameraPtr = cameraEntity_->AddComponent<CameraComponent>();
	// 生ポインタを std::shared_ptr に変換
	std::shared_ptr<CameraComponent> camera = std::shared_ptr<CameraComponent>(
		rawCameraPtr, [](CameraComponent*) {
		}
	);

	// カメラを CameraManager に追加
	CameraManager::AddCamera(camera);
	// アクティブカメラに設定
	CameraManager::SetActiveCamera(camera);
}

void Editor::Update([[maybe_unused]] const float deltaTime) {
	// グリッドの表示
	DrawGrid(
		gridSize_,
		gridRange_,
		{ 0.28f, 0.28f, 0.28f, 1.0f },
		{ 0.39f, 0.2f, 0.02f, 1.0f },
		{ 0.0f, 0.39f, 0.39f, 1.0f },
		{ 0.39f, 0.39f, 0.39f, 1.0f }
	);

//#ifdef _DEBUG
//	// カメラの操作
//	static float moveSpd = 4.0f;
//
//	static bool firstReset = true; // 初回リセットフラグ
//	static bool cursorHidden = false;
//
//	static bool bOpenPopup = false; // ポップアップ表示フラグ
//	static float popupTimer = 0.0f;
//
//	if (InputSystem::IsPressed("attack2")) {
//		if (!cursorHidden) {
//			ShowCursor(FALSE); // カーソルを非表示にする
//			cursorHidden = true;
//		}
//
//		Vec2 delta = InputSystem::GetMouseDelta();
//
//		if (!firstReset) {
//			// 回転
//			float sensitivity = ConVarManager::GetConVar("sensitivity")->GetValueAsFloat();
//			float m_pitch = 0.022f;
//			float m_yaw = 0.022f;
//			float min = -89.0f;
//			float max = 89.0f;
//
//			static Vec3 rot_ = cameraEntity_->GetTransform()->GetLocalRot().ToEulerAngles();
//
//			rot_.y += delta.y * sensitivity * m_pitch * Math::deg2Rad;
//			rot_.x += delta.x * sensitivity * m_yaw * Math::deg2Rad;
//
//			rot_.y = std::clamp(rot_.y, min * Math::deg2Rad, max * Math::deg2Rad);
//
//			cameraEntity_->GetTransform()->SetWorldRot(Quaternion::Euler(Vec3::up * rot_.x + Vec3::right * rot_.y));
//
//			Vec3 moveInput = { 0.0f, 0.0f, 0.0f };
//
//			if (InputSystem::IsPressed("forward")) {
//				moveInput.z += 1.0f;
//			}
//
//			if (InputSystem::IsPressed("back")) {
//				moveInput.z -= 1.0f;
//			}
//
//			if (InputSystem::IsPressed("moveright")) {
//				moveInput.x += 1.0f;
//			}
//
//			if (InputSystem::IsPressed("moveleft")) {
//				moveInput.x -= 1.0f;
//			}
//
//			if (InputSystem::IsPressed("moveup")) {
//				moveInput.y += 1.0f;
//			}
//
//			if (InputSystem::IsPressed("movedown")) {
//				moveInput.y -= 1.0f;
//			}
//
//			moveInput.Normalize();
//
//			Quaternion camRot = cameraEntity_->GetTransform()->GetWorldRot();
//			Vec3 cameraForward = camRot * Vec3::forward;
//			Vec3 cameraRight = camRot * Vec3::right;
//			Vec3 cameraUp = camRot * Vec3::up;
//
//			if (InputSystem::IsTriggered("invprev")) {
//				moveSpd += 1.0f;
//			}
//
//			if (InputSystem::IsTriggered("invnext")) {
//				moveSpd -= 1.0f;
//			}
//
//			static float oldMoveSpd = 0.0f;
//			if (moveSpd != oldMoveSpd) {
//				bOpenPopup = true;
//				popupTimer = 0.0f;
//			}
//
//			moveSpd = std::clamp(moveSpd, 0.125f, 128.0f);
//
//			oldMoveSpd = moveSpd;
//
//			cameraEntity_->GetTransform()->SetWorldPos(
//				cameraEntity_->GetTransform()->GetWorldPos() + (cameraForward * moveInput.z + cameraRight * moveInput.x + cameraUp * moveInput.y) *
//				moveSpd * EngineTimer::GetScaledDeltaTime()
//			);
//		}
//		// カーソルをウィンドウの中央にリセット
//		POINT centerCursorPos = {
//			static_cast<LONG>(Window::GetClientWidth() / 2), static_cast<LONG>(Window::GetClientHeight() / 2)
//		};
//		ClientToScreen(Window::GetWindowHandle(), &centerCursorPos); // クライアント座標をスクリーン座標に変換
//		SetCursorPos(centerCursorPos.x, centerCursorPos.y);
//
//		firstReset = false; // 初回リセット完了
//	} else {
//		if (cursorHidden) {
//			ShowCursor(TRUE); // カーソルを表示する
//			cursorHidden = false;
//		}
//		firstReset = true; // マウスボタンが離されたら初回リセットフラグをリセット
//	}
//
//	// 移動速度が変更されたらImGuiで現在の移動速度をポップアップで表示
//	if (bOpenPopup) {
//		// ビューポートのサイズと位置を取得
//		ImGuiViewport* viewport = ImGui::GetMainViewport();
//		ImVec2 viewportPos = viewport->Pos;
//		ImVec2 viewportSize = viewport->Size;
//		auto windowSize = ImVec2(256.0f, 32.0f);
//
//		// ウィンドウの中央下部位置を計算
//		ImVec2 windowPos(
//			viewportPos.x + (viewportSize.x) * 0.5f,
//			viewportPos.y + (viewportSize.y) * 0.75f
//		);
//
//		// ウィンドウの位置を調整
//		windowPos.x -= windowSize.x * 0.5f;
//		windowPos.y -= windowSize.y * 0.5f;
//
//		// ウィンドウの位置を設定
//		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
//		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
//
//		// ウィンドウを角丸に
//		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
//		// タイトルバーを非表示
//
//		ImGui::Begin(
//			"##move speed",
//			nullptr,
//			ImGuiWindowFlags_NoTitleBar |
//			ImGuiWindowFlags_NoResize |
//			ImGuiWindowFlags_NoMove |
//			ImGuiWindowFlags_NoSavedSettings |
//			ImGuiWindowFlags_NoBringToFrontOnFocus |
//			ImGuiWindowFlags_NoFocusOnAppearing |
//			ImGuiWindowFlags_NoScrollbar
//		);
//
//		ImGui::SetCursorPos(
//			ImVec2(
//				(windowSize.x - ImGui::CalcTextSize((StrUtils::ConvertToUtf8(0xe9e4) + std::format(" {:.2f}", moveSpd)).c_str()).x) * 0.5f,
//				(windowSize.y - ImGui::GetFontSize()) * 0.5f
//			)
//		);
//		ImGui::Text((StrUtils::ConvertToUtf8(0xe9e4) + " %.2f").c_str(), moveSpd);
//
//		// 一定時間経過後にポップアップをフェードアウトして閉じる
//		popupTimer += EngineTimer::GetDeltaTime(); // ゲーム内ではないのでScaledDeltaTimeではなくDeltaTimeを使用
//		if (popupTimer >= 3.0f) {
//			ImGui::CloseCurrentPopup();
//			bOpenPopup = false;
//			popupTimer = 0.0f;
//		}
//
//		ImGui::End();
//		ImGui::PopStyleVar();
//	}
//
//	cameraEntity_->Update(deltaTime);
//
//#endif

#ifdef _DEBUG
	ImGui::ShowDemoWindow();

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "CTRL+X")) {}
			if (ImGui::MenuItem("Copy", "CTRL+C")) {}
			if (ImGui::MenuItem("Paste", "CTRL+V")) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// アウトライナウィンドウの開始
	if (ImGui::Begin("Outliner")) {
		// テーブルの開始
		if (ImGui::BeginTable("OutlinerTable", 3,
			ImGuiTableFlags_NoBordersInBody |
			ImGuiTableFlags_SizingFixedFit |
			ImGuiTableFlags_RowBg)
			) {

			// カラムの設定
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 30.0f);

			// 再帰的にエンティティを表示する関数
			std::function<void(Entity*)> drawEntityNode = [&](Entity* entity) {
				ImGui::PushID(entity);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				// エンティティ名とツリー構造
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
					ImGuiTreeNodeFlags_SpanAvailWidth |
					ImGuiTreeNodeFlags_AllowItemOverlap |
					ImGuiTreeNodeFlags_DefaultOpen;

				if (entity->GetChildren().empty()) {
					flags |= ImGuiTreeNodeFlags_Leaf;
				}
				if (entity == selectedEntity_) {
					flags |= ImGuiTreeNodeFlags_Selected;
				}

				ImGui::AlignTextToFramePadding();
				bool nodeOpen = ImGui::TreeNodeEx(entity->GetName().c_str(), flags);

				if (ImGui::IsItemClicked()) {
					selectedEntity_ = entity;
				}

				// Visible アイコン
				ImGui::TableNextColumn();
				bool visible = entity->IsVisible();

				// アイコンのサイズを一時的に変更
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));  // アイコン間のスペースを調整
				float originalScale = ImGui::GetFont()->Scale;
				ImGui::GetFont()->Scale = 1.2f;  // スケールを1.2倍に
				ImGui::PushFont(ImGui::GetFont());

				ImGui::PushStyleColor(ImGuiCol_Text, visible ?
					ImGui::GetStyleColorVec4(ImGuiCol_Text) :
					ImVec4(0.5f, 0.5f, 0.5f, 0.5f));

				// アイコンを中央に配置
				float iconWidth = ImGui::CalcTextSize(StrUtils::ConvertToUtf8(kIconVisibility).c_str()).x;
				float columnWidth = ImGui::GetColumnWidth();
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (columnWidth - iconWidth) * 0.5f);

				if (ImGui::Selectable(StrUtils::ConvertToUtf8(visible ?
					kIconVisibility : kIconVisibilityOff).c_str(), false,
					ImGuiSelectableFlags_DontClosePopups)) {
					entity->SetVisible(!visible);
				}

				// スタイルを元に戻す
				ImGui::PopStyleColor();
				ImGui::GetFont()->Scale = originalScale;
				ImGui::PopFont();
				ImGui::PopStyleVar();

				// Active チェックボックス
				ImGui::TableNextColumn();
				bool active = entity->IsActive();
				if (ImGui::Checkbox("##Active", &active)) {
					entity->SetActive(active);
				}

				if (nodeOpen) {
					for (auto& child : entity->GetChildren()) {
						drawEntityNode(child);
					}
					ImGui::TreePop();
				}

				ImGui::PopID();
				};

			// ルートエンティティから開始
			auto entities = scene_->GetEntities();
			for (auto& entity : entities) {
				if (entity->GetParent() == nullptr) {
					drawEntityNode(entity);
				}
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
#endif

	//// タブの名前
	//static const char* tabNames[] = {
	//	"Test",
	//	("いい感じのヘッダー" + StrUtils::ConvertToUtf8(kIconTerminal)).c_str(),
	//	"Physics",
	//	"Audio",
	//	"Input"
	//};
	//static int selectedTab = 0; // 現在選択中のタブインデックス

	//// 縦方向のレイアウトを作成
	//ImGui::Begin("Vertical Tabs Example");

	//// タブ用の列を分割
	//ImGui::Columns(2, nullptr, false);

	//// タブのリスト (左側の列)
	//ImGui::BeginChild("Tabs", ImVec2(32, 0), true);
	//for (int i = 0; i < IM_ARRAYSIZE(tabNames); i++) {
	//	// ボタンまたは選択可能なアイテムとしてタブを表現
	//	if (ImGui::Selectable((StrUtils::ConvertToUtf8(kIconTerminal)).c_str(), selectedTab == i)) {
	//		selectedTab = i; // タブを切り替える
	//	}
	//}
	//ImGui::EndChild();

	//// コンテンツ表示エリア (右側の列)
	//ImGui::NextColumn();
	//ImGui::BeginChild("Content", ImVec2(0, 0), false);
	//ImGui::Text("Content for tab: %s", tabNames[selectedTab]);
	//ImGui::EndChild();

	//ImGui::End();

	// インスペクタ
#ifdef _DEBUG
	if (ImGui::Begin("Inspector")) {
		if (selectedEntity_) {
			ImGui::Text("Name: %s", selectedEntity_->GetName().c_str());

			selectedEntity_->GetTransform()->DrawInspectorImGui();

			// コンポーネントの一覧表示と編集
			const auto& components = selectedEntity_->GetComponents<Component>();
			for (const auto& component : components) {
				if (component) {
					component->DrawInspectorImGui();
				}
			}
		}
	}
	ImGui::End();


	if (ImGui::Begin("World Settings")) {
		ImGui::Text("Grid Size");
		ImGui::SliderFloat("##GridSize", &gridSize_, 0.125f, 64.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::Text("Grid Range");
		ImGui::SliderFloat("##GridRange", &gridRange_, 128.0f, 16384.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
	}
	ImGui::End();
#endif

#ifdef _DEBUG
	auto viewport = static_cast<ImGuiViewportP*>(static_cast<void*>(ImGui::GetMainViewport()));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.0f);

	if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, 38, window_flags)) {
		if (ImGui::BeginMenuBar()) {
			ImGui::PopStyleVar();

			// ステータスバーの幅を取得
			float statusBarWidth = ImGui::GetWindowWidth();

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

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
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
				ImGui::PopItemWidth();
			}

			// グリッドスナップ
			{
				const float windowHeight = ImGui::GetWindowSize().y;
				const char* items[] = { "0.125", "0.25", "0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512" };
				static int itemCurrentIndex = 2;
				const char* comboLabel = items[itemCurrentIndex];
				ImGui::Text("Grid: ");
				// 垂直中央に配置
				float comboHeight = ImGui::GetFrameHeight();
				float offsetY = (windowHeight - comboHeight) * 0.5f;
				ImGui::SetCursorPosY(offsetY);

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				if (ImGui::BeginCombo("##grid", comboLabel)) {
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
				ImGui::PopItemWidth();
			}

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