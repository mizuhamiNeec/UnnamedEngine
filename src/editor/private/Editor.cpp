#include <pch.h>

#include <editor/public/Editor.h>

#include <engine/public/Engine.h>
#include <engine/public/Camera/CameraManager.h>
#include <engine/public/Components/Base/Component.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/ImGui/Icons.h>
#include <engine/public/Input/InputSystem.h>
#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/SceneManager/SceneManager.h>
#include <engine/public/Window/WindowManager.h>

#include <math/public/MathLib.h>

#include <engine/public/ImGui/ImGuiWidgets.h>

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後
#include <ImGuizmo.h>
#endif

Editor::Editor(SceneManager* sceneManager, GameTime* gameTime)
	: mSceneManager(sceneManager), mGameTime(gameTime) {
	mScene = mSceneManager->GetCurrentScene();
	Init();
}

void Editor::Init() {
	// カメラの作成
	mCameraEntity = std::make_unique<Entity>("editorCamera",
	                                         EntityType::EditorOnly);
	mCameraEntity->GetTransform()->SetLocalPos(
		Vec3::forward * -5.0f + Vec3::up * 2.0f);
	mCameraEntity->GetTransform()->SetLocalRot(
		Quaternion::Euler(Vec3::right * 15.0f * Math::deg2Rad));

	// 生ポインタを取得
	CameraComponent* rawCameraPtr = mCameraEntity->AddComponent<
		CameraComponent>();
	// 生ポインタを std::shared_ptr に変換
	auto camera = std::shared_ptr<CameraComponent>(
		rawCameraPtr, [](CameraComponent*) {
		}
	);

	// カメラを CameraManager に追加
	CameraManager::AddCamera(camera);
	// アクティブカメラに設定
	CameraManager::SetActiveCamera(camera);

	auto* camRaw = mCameraEntity.get();
	mSceneManager->GetCurrentScene()->AddEntity(std::move(mCameraEntity.get()));
	mCameraEntityRaw = camRaw;

#ifdef _DEBUG
	// ImGuizmoの設定
	{
		ImGuizmo::AllowAxisFlip(false);
		ImGuizmo::Style imGuizmoStyle;

		imGuizmoStyle.CenterCircleSize = 4.0f;

		imGuizmoStyle.HatchedAxisLineThickness = 2.0f;

		imGuizmoStyle.TranslationLineThickness = 2.0f;
		imGuizmoStyle.TranslationLineArrowSize = 8.0f;

		imGuizmoStyle.RotationLineThickness      = 4.0f;
		imGuizmoStyle.RotationOuterLineThickness = 2.0f;

		imGuizmoStyle.ScaleLineThickness  = 2.0f;
		imGuizmoStyle.ScaleLineCircleSize = 8.0f;

		imGuizmoStyle.Colors[ImGuizmo::DIRECTION_X] = ImVec4(
			0.78f, 0.12f, 0.12f, 1.0f);
		imGuizmoStyle.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(
			0.2f, 0.78f, 0.12f, 1.0f);
		imGuizmoStyle.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(
			0.12f, 0.43f, 0.78f, 1.0f);
		imGuizmoStyle.Colors[ImGuizmo::PLANE_X] = ImVec4(
			0.666f, 0.000f, 0.000f, 0.380f);
		imGuizmoStyle.Colors[ImGuizmo::PLANE_Y] = ImVec4(
			0.000f, 0.666f, 0.000f, 0.380f);
		imGuizmoStyle.Colors[ImGuizmo::PLANE_Z] = ImVec4(
			0.000f, 0.000f, 0.666f, 0.380f);
		imGuizmoStyle.Colors[ImGuizmo::SELECTION] = ImVec4(
			1.000f, 0.500f, 0.062f, 0.541f);
		imGuizmoStyle.Colors[ImGuizmo::INACTIVE] = ImVec4(
			0.600f, 0.600f, 0.600f, 0.600f);
		imGuizmoStyle.Colors[ImGuizmo::TRANSLATION_LINE] = ImVec4(
			0.666f, 0.666f, 0.666f, 0.666f);
		imGuizmoStyle.Colors[ImGuizmo::SCALE_LINE] = ImVec4(
			0.250f, 0.250f, 0.250f, 1.000f);
		imGuizmoStyle.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(
			1.000f, 0.500f, 0.062f, 1.000f);
		imGuizmoStyle.Colors[ImGuizmo::ROTATION_USING_FILL] = ImVec4(
			1.000f, 0.500f, 0.062f, 0.500f);
		imGuizmoStyle.Colors[ImGuizmo::HATCHED_AXIS_LINES] = ImVec4(
			0.000f, 0.000f, 0.000f, 0.500f);
		imGuizmoStyle.Colors[ImGuizmo::TEXT] = ImVec4(
			1.000f, 1.000f, 1.000f, 1.000f);
		imGuizmoStyle.Colors[ImGuizmo::TEXT] = ImVec4(
			1.000f, 1.000f, 1.000f, 1.000f);
		imGuizmoStyle.Colors[ImGuizmo::TEXT_SHADOW] = ImVec4(
			0.000f, 0.000f, 0.000f, 1.000f);

		ImGuizmo::GetStyle() = imGuizmoStyle;
	}
#endif
}

void Editor::Update([[maybe_unused]] const float deltaTime) {
#ifdef _DEBUG
	// カメラの操作
	{
		static float moveSpd = 4.0f;

		static bool firstReset   = true; // 初回リセットフラグ
		static bool cursorHidden = false;

		static bool  bOpenPopup = false; // ポップアップ表示フラグ
		static float popupTimer = 0.0f;

		auto   lt           = Unnamed::Engine::GetViewportLT();
		auto   size         = Unnamed::Engine::GetViewportSize();
		ImVec2 viewportPos  = {lt.x, lt.y};
		ImVec2 viewportSize = {size.x, size.y};
		auto   mousePos     = ImGui::GetMousePos();

		bool bIsInsideViewport =
			mousePos.x >= viewportPos.x &&
			mousePos.x <= viewportPos.x + viewportSize.x &&
			mousePos.y >= viewportPos.y &&
			mousePos.y <= viewportPos.y + viewportSize.y;

		if (InputSystem::IsPressed("attack2") && bIsInsideViewport) {
			if (!cursorHidden) {
				ShowCursor(FALSE); // カーソルを非表示にする
				cursorHidden = true;
			}

			Vec2 delta = InputSystem::GetMouseDelta();

			if (!firstReset) {
				// 回転
				float sensitivity = ConVarManager::GetConVar("sensitivity")->
					GetValueAsFloat();
				float m_pitch = 0.022f;
				float m_yaw   = 0.022f;
				float min     = -89.0f;
				float max     = 89.0f;

				static Vec3 rot_ = mCameraEntity->GetTransform()->GetLocalRot().
				                                  ToEulerAngles();

				rot_.y += delta.y * sensitivity * m_pitch * Math::deg2Rad;
				rot_.x += delta.x * sensitivity * m_yaw * Math::deg2Rad;

				rot_.y = std::clamp(rot_.y, min * Math::deg2Rad,
				                    max * Math::deg2Rad);

				mCameraEntity->GetTransform()->SetWorldRot(
					Quaternion::Euler(
						Vec3::up * rot_.x + Vec3::right * rot_.y));

				Vec3 moveInput = {0.0f, 0.0f, 0.0f};

				if (InputSystem::IsPressed("forward")) {
					moveInput.z += 1.0f;
				}

				if (InputSystem::IsPressed("back")) {
					moveInput.z -= 1.0f;
				}

				if (InputSystem::IsPressed("moveright")) {
					moveInput.x += 1.0f;
				}

				if (InputSystem::IsPressed("moveleft")) {
					moveInput.x -= 1.0f;
				}

				if (InputSystem::IsPressed("moveup")) {
					moveInput.y += 1.0f;
				}

				if (InputSystem::IsPressed("movedown")) {
					moveInput.y -= 1.0f;
				}

				moveInput.Normalize();

				Quaternion camRot = mCameraEntity->GetTransform()->
				                                   GetWorldRot();
				Vec3 cameraForward = camRot * Vec3::forward;
				Vec3 cameraRight   = camRot * Vec3::right;
				Vec3 cameraUp      = camRot * Vec3::up;

				if (InputSystem::IsTriggered("invprev")) {
					moveSpd *= 2.0f;
					moveSpd = RoundToNearestPowerOfTwo(moveSpd);
				}

				if (InputSystem::IsTriggered("invnext")) {
					moveSpd *= 0.5f;
					moveSpd = RoundToNearestPowerOfTwo(moveSpd);
				}

				static float oldMoveSpd = 0.0f;
				if (moveSpd != oldMoveSpd) {
					bOpenPopup = true;
					popupTimer = 0.0f;
				}

				moveSpd = std::clamp(moveSpd, 0.125f, 65535.0f);

				oldMoveSpd = moveSpd;

				mCameraEntity->GetTransform()->SetWorldPos(
					mCameraEntity->GetTransform()->GetWorldPos() + (
						cameraForward *
						moveInput.z + cameraRight * moveInput.x + cameraUp *
						moveInput.y) *
					moveSpd * mGameTime->ScaledDeltaTime<float>()
				);
			}
			// カーソルをウィンドウの中央にリセット
			POINT centerCursorPos = {
				static_cast<LONG>(OldWindowManager::GetMainWindow()->
					GetClientWidth() /
					2),
				static_cast<LONG>(OldWindowManager::GetMainWindow()->
					GetClientHeight()
					/ 2)
			};
			ClientToScreen(OldWindowManager::GetMainWindow()->GetWindowHandle(),
			               &centerCursorPos); // クライアント座標をスクリーン座標に変換
			SetCursorPos(centerCursorPos.x, centerCursorPos.y);

			firstReset = false; // 初回リセット完了
		} else {
			if (cursorHidden) {
				ShowCursor(TRUE); // カーソルを表示する
				cursorHidden = false;
			}
			firstReset = true; // マウスボタンが離されたら初回リセットフラグをリセット
		}

		float iconScale = 0.75f;
		// 移動速度が変更されたらImGuiで現在の移動速度をポップアップで表示
		if (bOpenPopup) {
			auto windowSize = ImVec2(256.0f, 32.0f);

			// ウィンドウの中央下部位置を計算
			ImVec2 windowPos(
				viewportPos.x + (viewportSize.x) * 0.5f,
				viewportPos.y + (viewportSize.y) * iconScale
			);

			// ウィンドウの位置を調整
			windowPos.x -= windowSize.x * 0.5f;
			windowPos.y -= windowSize.y * 0.5f;

			// ウィンドウの位置を設定
			ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
			ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

			// ウィンドウを角丸に
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
			// タイトルバーを非表示

			ImGui::Begin(
				"##move speed",
				nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoScrollbar
			);

			ImGui::SetCursorPos(
				ImVec2(
					(windowSize.x - ImGui::CalcTextSize(
						(StrUtil::ConvertToUtf8(0xe9e4) + std::format(
							" {:.2f}", moveSpd)).c_str()).x) * 0.5f,
					(windowSize.y - ImGui::GetFontSize()) * 0.5f
				)
			);
			ImGui::Text((StrUtil::ConvertToUtf8(0xe9e4) + " %.2f").c_str(),
			            moveSpd);

			// 一定時間経過後にポップアップをフェードアウトして閉じる
			// ゲーム内ではないのでScaledDeltaTimeではなくDeltaTimeを使用
			popupTimer += deltaTime;
			if (popupTimer >= 3.0f) {
				ImGui::CloseCurrentPopup();
				bOpenPopup = false;
				popupTimer = 0.0f;
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}
	}

	ImGui::ShowDemoWindow();

	// アウトライナウィンドウの開始
	if (ImGui::Begin("Outliner")) {
		if (ImGui::Button("Add Entity")) {
			mScene->AddEntity(
				new Entity("New Entity"));
		}

		// テーブルの開始
		if (ImGui::BeginTable(
			"OutlinerTable", 3
			//ImGuiTableFlags_NoBordersInBody |
			//ImGuiTableFlags_SizingFixedFit 
			// ImGuiTableFlags_RowBg |
			// ImGuiTableFlags_BordersInnerH 
		)) {
			// カラムの設定
			ImGui::TableSetupColumn(
				"Name",
				ImGuiTableColumnFlags_NoHide |
				ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed,
			                        30.0f);
			ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed,
			                        38.0f);

			// 再帰的にエンティティを表示する関数
			std::function<void(Entity*)>
				drawEntityNode =
					[&](Entity* entity) {
					if (!entity) {
						return;
					}

					if (entity->GetName().empty()) {
						return;
					}

					ImGui::PushID(entity);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					// エンティティ名とツリー構造
					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
						ImGuiTreeNodeFlags_SpanAvailWidth |
						ImGuiTreeNodeFlags_AllowOverlap |
						ImGuiTreeNodeFlags_DefaultOpen;

					if (entity->GetChildren().empty()) {
						flags |= ImGuiTreeNodeFlags_Leaf;
					}
					if (entity == mSelectedEntity) {
						flags |= ImGuiTreeNodeFlags_Selected;
					}

					ImGui::AlignTextToFramePadding();
					bool nodeOpen = ImGui::TreeNodeEx(
						(StrUtil::ConvertToUtf8(kIconEntity) +
							" " +
							entity->GetName())
						.c_str(),
						flags
					);

					// 左クリックで選択
					if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
						mSelectedEntity = entity;
					}

					// 右クリックでコンテキストメニュー
					if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
						if (entity != mCameraEntity.get()) {
							// エディタカメラは削除不可
							if (ImGui::MenuItem("Delete")) {
								if (auto currentScene = mSceneManager->
									GetCurrentScene()) {
									// SceneクラスにRemoveEntityメソッドが実装されていると仮定
									currentScene->RemoveEntity(entity);
									if (mSelectedEntity == entity) {
										mSelectedEntity = nullptr; // 選択を解除
									}
									ImGui::EndPopup(); // ポップアップを閉じる

									// TreeNodeExが開かれていた場合、TreePopを呼び出してバランスを取る
									if (nodeOpen) {
										ImGui::TreePop();
									}
									ImGui::PopID();
									// ImGui::PushID(entity) でプッシュしたIDをポップ
									return; // 早期リターン
								}
							}
						} else {
							ImGui::TextDisabled("Cannot delete editor camera");
						}
						ImGui::EndPopup();
					}


					// Visible アイコン
					ImGui::TableNextColumn();
					bool visible = entity->IsVisible();

					// アイコンのサイズを一時的に変更
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
					                    ImVec2(0, 0));
					// // アイコン間のスペースを調整
					// float originalScale     = ImGui::GetFont()->Scale;
					// ImGui::GetFont()->Scale = 1.2f; // スケールを1.2倍に
					// ImGui::PushFont(ImGui::GetFont());

					ImGui::PushStyleColor(
						ImGuiCol_Text,
						visible
							? ImGui::GetStyleColorVec4(ImGuiCol_Text)
							: ImVec4(0.5f, 0.5f, 0.5f, 0.5f)
					);

					// アイコンを中央に配置
					float iconWidth = ImGui::CalcTextSize(
						StrUtil::ConvertToUtf8(kIconVisibility).c_str()).x;
					float columnWidth = ImGui::GetColumnWidth();
					ImGui::SetCursorPosX(
						ImGui::GetCursorPosX() + (columnWidth - iconWidth) *
						0.5f);

					if (ImGui::Selectable(
						StrUtil::ConvertToUtf8(
							visible ? kIconVisibility : kIconVisibilityOff
						).c_str(), false,
						ImGuiSelectableFlags_NoAutoClosePopups
					)) {
						entity->SetVisible(!visible);
					}

					// スタイルを元に戻す
					ImGui::PopStyleColor();
					ImGui::PopStyleVar();

					// Active チェックボックス
					ImGui::TableNextColumn();
					bool active = entity->IsActive();
					if (ImGui::Checkbox("##Active", &active)) {
						entity->SetActive(active);
					}

					if (nodeOpen) {
						// 子エンティティを処理する前に、現在のエンティティが削除されていないか確認
						// (上記の削除処理でreturnしているため、基本的にはここは通らないはずだが念のため)
						bool entityStillExists = false;
						if (auto currentScene = mSceneManager->
							GetCurrentScene()) {
							for (const auto& e : currentScene->GetEntities()) {
								if (e == entity) {
									entityStillExists = true;
									break;
								}
							}
						}

						if (entityStillExists) {
							// GetChildren() が返すコンテナのコピーに対してループする方が安全な場合がある
							// ここでは元の実装に従う
							auto children = entity->GetChildren();
							// コピーを取得する方が安全かもしれない
							for (auto& child : children) {
								// childが削除される可能性も考慮すると、さらに堅牢なイテレーションが必要
								drawEntityNode(child);
							}
						}
						ImGui::TreePop();
					}
					ImGui::PopID();
				};

			// ルートエンティティから開始
			// シーンからエンティティリストを取得する際、削除操作中にイテレータが無効になることを避けるため、
			// リストのコピーに対して操作を行うか、削除を遅延させるなどの対策が必要になる場合がある。
			// ここではGetCurrentScene()->GetEntities()が安全なコピーまたは参照を返すと仮定する。
			if (mScene) {
				// scene_が有効か確認
				auto entities = mScene->GetEntities();
				// 削除操作があるため、コピーを取得することを検討
				for (auto& entity : entities) {
					if (entity && entity->GetParent() == nullptr) {
						// entityがnullでないことも確認
						drawEntityNode(entity);
					}
				}
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
#endif

	// // タブの名前
	// static const char* tabNames[] = {
	// 	"Test",
	// 	"いい感じのヘッダー",
	// 	"Physics",
	// 	"Audio",
	// 	"Input"
	// };
	// static int selectedTab = 0; // 現在選択中のタブインデックス
	//
	// // 縦方向のレイアウトを作成
	// ImGui::Begin("Vertical Tabs Example");
	//
	// // タブ用の列を分割
	// ImGui::Columns(2, nullptr, false);
	//
	// // タブのリスト (左側の列)
	// ImGui::BeginChild("Tabs", ImVec2(512, 0), true);
	// for (int i = 0; i < IM_ARRAYSIZE(tabNames); i++) {
	// 	// ボタンまたは選択可能なアイテムとしてタブを表現
	// 	if (ImGui::Selectable(
	// 		(StrUtil::ConvertToUtf8(kIconTerminal) + tabNames[i]).c_str(),
	// 		selectedTab == i)) {
	// 		selectedTab = i; // タブを切り替える
	// 	}
	// }
	// ImGui::EndChild();
	//
	// // コンテンツ表示エリア (右側の列)
	// ImGui::NextColumn();
	// ImGui::BeginChild("Content", ImVec2(0, 0), false);
	// ImGui::Text("Content for tab: %s", tabNames[selectedTab]);
	// ImGui::EndChild();
	//
	// ImGui::End();

	// インスペクタ
#ifdef _DEBUG
	if (ImGui::Begin("Inspector")) {
		if (mSelectedEntity) {
			ImGui::Text("Name: %s", mSelectedEntity->GetName().c_str());

			mSelectedEntity->GetTransform()->DrawInspectorImGui();

			// コンポーネントの一覧表示と編集
			const auto& components = mSelectedEntity->GetComponents<
				Component>();
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
		ImGui::SliderFloat("##GridSize", &mGridSize, 0.125f, 64.0f, "%.3f",
		                   ImGuiSliderFlags_Logarithmic);
		ImGui::Text("Grid Range");
		ImGui::SliderFloat("##GridRange", &mGridRange, 128.0f, 16384.0f, "%.3f",
		                   ImGuiSliderFlags_Logarithmic);
	}
	ImGui::End();
#endif

#ifdef _DEBUG
	{
		ImGui::Begin("WidgetsTest");

		ImGui::BeginGroup();
		ImGui::Text("Hello there");
		ImGui::Button("Button Inside");
		static Vec3 vec3(0.0f, 0.0f, 0.0f);
		ImGuiWidgets::DragVec3("Vec3", vec3, Vec3::zero, 0.1f, "%.2f");
		ImGui::EndGroup();

		ImGui::End();
	}

	// TODO: アウトライナに使おう
	{
		ImGui::Begin("TreeTable");

		const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;

		static ImGuiTableFlags table_flags = ImGuiTableFlags_BordersV |
			ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
			ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

		static ImGuiTreeNodeFlags tree_node_flags_base =
			ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_DrawLinesFull;

		if (ImGui::BeginTable("3ways", 3, table_flags)) {
			// The first column will use the default _WidthStretch when ScrollX is Off and _WidthFixed when ScrollX is On
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed,
			                        TEXT_BASE_WIDTH * 12.0f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
			                        TEXT_BASE_WIDTH * 18.0f);
			ImGui::TableHeadersRow();

			// Simple storage to output a fake file-system.
			struct MyTreeNode {
				const char* name;
				const char* type;
				int         size;
				int         childIdx;
				int         childCount;

				static void DisplayNode(const MyTreeNode* node,
				                        const MyTreeNode* all_nodes) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const bool is_folder = (node->childCount > 0);

					ImGuiTreeNodeFlags node_flags = tree_node_flags_base;
					if (node != &all_nodes[0]) {
						node_flags &= ~ImGuiTreeNodeFlags_LabelSpanAllColumns;
						// Only demonstrate this on the root node.
					}

					if (is_folder) {
						bool open = ImGui::TreeNodeEx(node->name, node_flags);
						if ((node_flags &
							ImGuiTreeNodeFlags_LabelSpanAllColumns) == 0) {
							ImGui::TableNextColumn();
							ImGui::TextDisabled("--");
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(node->type);
						}
						if (open) {
							for (int child_n = 0; child_n < node->childCount;
							     child_n++) {
								DisplayNode(
									&all_nodes[node->childIdx + child_n],
									all_nodes);
							}
							ImGui::TreePop();
						}
					} else {
						ImGui::TreeNodeEx(node->name,
						                  node_flags | ImGuiTreeNodeFlags_Leaf |
						                  ImGuiTreeNodeFlags_Bullet |
						                  ImGuiTreeNodeFlags_NoTreePushOnOpen);
						ImGui::TableNextColumn();
						ImGui::Text("%d", node->size);
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(node->type);
					}
				}
			};
			static const MyTreeNode nodes[] =
			{
				{"Root with Long Name", "Folder", -1, 1, 3},            // 0
				{"Music", "Folder", -1, 4, 2},                          // 1
				{"Textures", "Folder", -1, 6, 3},                       // 2
				{"desktop.ini", "System file", 1024, -1, -1},           // 3
				{"File1_a.wav", "Audio file", 123000, -1, -1},          // 4
				{"File1_b.wav", "Audio file", 456000, -1, -1},          // 5
				{"Image001.png", "Image file", 203128, -1, -1},         // 6
				{"Copy of Image001.png", "Image file", 203256, -1, -1}, // 7
				{"Copy of Image001 (Final2).png", "Image file", 203512, -1, -1},
				// 8
			};

			MyTreeNode::DisplayNode(&nodes[0], nodes);

			ImGui::EndTable();
		}

		ImGui::End();
	}
#endif

#ifdef _DEBUG
	ImVec2 windowPadding = ImGui::GetStyle().WindowPadding;
	ImVec2 framePadding  = ImGui::GetStyle().FramePadding;
	ImVec2 itemSpacing   = ImGui::GetStyle().ItemSpacing;

	// 上側のツールバー
	{
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus;

		// 横長にする
		constexpr auto toolbarIconSize = ImVec2(
			96.0f,
			32.0f
		);


		//		ImGui::SetNextWindowBgAlpha(1.0f);
		if (
			ImGui::BeginViewportSideBar(
				"##TopToolbar",
				ImGui::GetMainViewport(),
				ImGuiDir_Up,
				toolbarIconSize.y + windowPadding.y * 2.0f,
				flags
			)
		) {
			// 選択
			{
				constexpr float iconScale = 0.75f;

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconVertex).c_str(), "Vertices",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				);

				ImGui::SameLine();

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconEdge).c_str(), "Edges",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				);

				ImGui::SameLine();

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconFace).c_str(), "Faces",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				);

				ImGui::SameLine();

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconMesh).c_str(), "Meshes",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				);

				ImGui::SameLine();

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconObject).c_str(), "Objects",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				);

				ImGui::SameLine();

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconGroup).c_str(), "Groups",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				);
			}

			ImGui::End();
		}
	}

	// 左側のツールバー
	{
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBringToFrontOnFocus;

		constexpr float iconSize  = 40; // アイコンのサイズを設定
		constexpr float iconScale = 0.75f;

		const ImVec2 toolbarIconSize = ImVec2(iconSize, iconSize);

		ImGui::SetNextWindowBgAlpha(1.0f);
		if (
			ImGui::BeginViewportSideBar(
				"##LeftToolbar",
				ImGui::GetMainViewport(),
				ImGuiDir_Left,
				toolbarIconSize.x + windowPadding.x * 2.0f,
				flags
			)
		) {
			// ツールバー
			{
				ImGui::BeginDisabled();

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconSelect).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconMove).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconRotate).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconScale).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconPivot).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGui::Separator();

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconObject).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconNANKABOX).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconTexture).c_str(),
					"",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGui::EndDisabled();
			}
			ImGui::End();
		}
	}

	if (
		ImGui::BeginViewportSideBar(
			"##MainStatusBar",
			ImGui::GetMainViewport(),
			ImGuiDir_Down,
			0.0f,
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_MenuBar
		)
	) {
		if (ImGui::BeginMenuBar()) {
			//ImGui::PopStyleVar();

			// ステータスバーの幅を取得
			float statusBarWidth = ImGui::GetWindowWidth();

			// アングルスナップ
			{
				const float windowHeight = ImGui::GetWindowSize().y;
				const char* items[]      = {
					"0.25°", "0.5°", "1°", "5°", "5.625°", "11.25°", "15°",
					"22.5°", "30°", "45°", "90°"
				};
				static int  itemCurrentIndex = 6;
				const char* comboLabel       = items[itemCurrentIndex];

				ImGui::Text("Angle: ");

				// 垂直中央に配置
				float comboHeight = ImGui::GetFrameHeight();
				float offsetY     = (windowHeight - comboHeight) * 0.5f;
				ImGui::SetCursorPosY(offsetY);

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				if (ImGui::BeginCombo("##angle", comboLabel)) {
					for (int n = 0; n < IM_ARRAYSIZE(items); ++n) {
						const bool isSelected = (itemCurrentIndex == n);
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
							mAngleSnap       = std::stof(
								items[itemCurrentIndex]);
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
				const char* items[]      = {
					"0.125", "0.25", "0.5", "1", "2", "4", "8", "16", "32",
					"64", "128", "256", "512"
				};
				static int  itemCurrentIndex = 9;
				const char* comboLabel       = items[itemCurrentIndex];
				ImGui::Text("Grid: ");
				// 垂直中央に配置
				float comboHeight = ImGui::GetFrameHeight();
				float offsetY     = (windowHeight - comboHeight) * 0.5f;
				ImGui::SetCursorPosY(offsetY);

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				ImGui::PushID("GridCombo"); // IDの衝突を避けるためにプッシュ

				if (ImGui::BeginCombo("##grid", comboLabel)) {
					for (int n = 0; n < IM_ARRAYSIZE(items); ++n) {
						const bool isSelected = (itemCurrentIndex == n);
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
							// 選択された文字列を浮動小数点数に変換してgridSize_に設定
							mGridSize = std::stof(items[itemCurrentIndex]);
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				// コンボボックスにマウスオーバーしている時にホイールで操作
				if (ImGui::IsItemHovered()) {
					float wheel = ImGui::GetIO().MouseWheel;
					if (wheel != 0.0f) {
						itemCurrentIndex -= static_cast<int>(wheel);
						itemCurrentIndex = std::clamp(
							itemCurrentIndex, 0, IM_ARRAYSIZE(items) - 1);
						// 選択された文字列を浮動小数点数に変換してgridSize_に設定
						mGridSize = std::stof(items[itemCurrentIndex]);
					}
				}

				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::EndMenuBar();
		}
		ImGui::End();
	}
#endif

	// グリッドの表示
	DrawGrid(
		mGridSize,
		mGridRange,
		{0.28f, 0.28f, 0.28f, 1.0f},
		{0.39f, 0.2f, 0.02f, 1.0f},
		{0.0f, 0.39f, 0.39f, 1.0f},
		{0.39f, 0.39f, 0.39f, 1.0f},
		CameraManager::GetActiveCamera()->GetViewMat().Inverse().GetTranslate(),
		mGridSize * 32.0f
	);

	if (auto currentScene = mSceneManager->GetCurrentScene()) {
		currentScene->Update(mGameTime->ScaledDeltaTime<float>());
	}

#ifdef _DEBUG
	// ギズモの操作はエンティティの更新が終わった後に行う
	Vec2 vLT   = Unnamed::Engine::GetViewportLT();
	Vec2 vSize = Unnamed::Engine::GetViewportSize();
	ImGuizmo::SetRect(
		vLT.x, vLT.y,
		vSize.x, vSize.y
	);

	auto camera = CameraManager::GetActiveCamera();
	Mat4 view   = camera->GetViewMat();
	Mat4 proj   = camera->GetProjMat();

	if (mSelectedEntity) {
		Mat4 worldMat = mSelectedEntity->GetTransform()->GetLocalMat();

		static ImGuizmo::MODE      mode      = ImGuizmo::MODE::LOCAL;
		static ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;

		static bool bIsWorldMode = true;
		if (InputSystem::IsTriggered("toggleGizmo")) {
			bIsWorldMode = !bIsWorldMode;
		}

		if (bIsWorldMode) {
			mode = ImGuizmo::MODE::WORLD;
		} else {
			mode = ImGuizmo::MODE::LOCAL;
		}

		auto snapValue = Vec3(mGridSize, mGridSize, mGridSize);

		if (InputSystem::IsTriggered("bounds")) {
			operation = ImGuizmo::OPERATION::BOUNDS;
		}

		if (InputSystem::IsTriggered("translate")) {
			operation = ImGuizmo::OPERATION::TRANSLATE;
			snapValue = Math::HtoM(Vec3(mGridSize, mGridSize, mGridSize));
		}
		if (InputSystem::IsTriggered("rotate")) {
			operation = ImGuizmo::OPERATION::ROTATE;
			snapValue = {
				mAngleSnap * Math::deg2Rad,
				mAngleSnap * Math::deg2Rad,
				mAngleSnap * Math::deg2Rad
			}; // ラジアンに変換
		}
		if (InputSystem::IsTriggered("scale")) {
			operation = ImGuizmo::OPERATION::SCALE;
			snapValue = Math::HtoM(Vec3(mGridSize, mGridSize, mGridSize));
		}

		if (operation == ImGuizmo::OPERATION::ROTATE) {
			snapValue = Vec3(
				mAngleSnap,
				mAngleSnap,
				mAngleSnap
			); // ラジアンに変換
		}

		mIsManipulating = ImGuizmo::Manipulate(
			*view.m,
			*proj.m,
			operation,
			mode,
			*worldMat.m,
			nullptr,
			&snapValue.x
		);

		if (mIsManipulating) {
			mSelectedEntity->GetTransform()->SetLocalPos(
				worldMat.GetTranslate()
			);
			mSelectedEntity->GetTransform()->SetLocalRot(
				Quaternion::Euler(worldMat.GetRotate()).Inverse()
			);
			mSelectedEntity->GetTransform()->SetLocalScale(
				worldMat.GetScale()
			);
		}
	}
#endif
}

void Editor::Render() const {
	if (auto currentScene = mSceneManager->GetCurrentScene()) {
		currentScene->Render();
		mCameraEntity->Render(Unnamed::Engine::GetRenderer()->GetCommandList());
	}
}

void Editor::DrawGrid(
	const float gridSize, const float range, const Vec4& color,
	const Vec4& majorColor,
	const Vec4& axisColor, const Vec4& minorColor, const Vec3& cameraPosition,
	const float drawRadius
) {
	const float minorInterval = gridSize * 8.0f;

	// 描画範囲の二乗を事前計算
	const float drawRadiusSq = drawRadius * drawRadius;

	// カメラ位置のXとZを事前に取得
	const float cameraPosX = cameraPosition.x;
	const float cameraPosZ = cameraPosition.z;

	// 範囲内のグリッドラインを計算
	const int   numLines = static_cast<int>((range * 2) / gridSize) + 1;
	const float startX   = -range;
	const float startZ   = -range;

	for (int i = 0; i < numLines; ++i) {
		constexpr float majorInterval = 1024.0f;
		float           x             = startX + i * gridSize;
		float           z             = startZ + i * gridSize;

		// 垂直線（X軸に沿った線）の描画
		{
			Vec4 lineColor = color;

			if (std::fmod(x, majorInterval) == 0.0f) {
				lineColor = majorColor; // 主要なグリッド線
			} else if (std::fmod(x, minorInterval) == 0.0f) {
				lineColor = minorColor; // 細かいグリッド線
			}

			if (x == 0.0f) {
				lineColor = axisColor; // 軸線
			}

			if (std::fmod(x, majorInterval) == 0.0f || x == 0.0f) {
				// 主要線・軸線は常に最大範囲で描画
				Debug::DrawLine(Vec3(x, 0, -range), Vec3(x, 0, range),
				                lineColor);
			} else {
				// 細かいグリッド線は円形範囲内のみ描画
				float distToLineSq = (cameraPosX - x) * (cameraPosX - x);
				if (distToLineSq <= drawRadiusSq) {
					float maxZ = std::sqrt(drawRadiusSq - distToLineSq);
					Debug::DrawLine(
						Vec3(x, 0, cameraPosZ - maxZ),
						Vec3(x, 0, cameraPosZ + maxZ),
						lineColor
					);
				}
			}
		}

		// 水平線（Z軸に沿った線）の描画
		{
			Vec4 lineColor = color;

			if (std::fmod(z, majorInterval) == 0.0f) {
				lineColor = majorColor; // 主要なグリッド線
			} else if (std::fmod(z, minorInterval) == 0.0f) {
				lineColor = minorColor; // 細かいグリッド線
			}

			if (z == 0.0f) {
				lineColor = axisColor; // 軸線
			}

			if (std::fmod(z, majorInterval) == 0.0f || z == 0.0f) {
				// 主要線・軸線は常に最大範囲で描画
				Debug::DrawLine(Vec3(-range, 0, z), Vec3(range, 0, z),
				                lineColor);
			} else {
				// 細かいグリッド線は円形範囲内のみ描画
				float distToLineSq = (cameraPosZ - z) * (cameraPosZ - z);
				if (distToLineSq <= drawRadiusSq) {
					float maxX = std::sqrt(drawRadiusSq - distToLineSq);
					Debug::DrawLine(
						Vec3(cameraPosX - maxX, 0, z),
						Vec3(cameraPosX + maxX, 0, z),
						lineColor
					);
				}
			}
		}
	}
}

float Editor::RoundToNearestPowerOfTwo(const float value) {
	float lowerPowerOfTwo = std::pow(2.0f, std::floor(std::log2(value)));
	float upperPowerOfTwo = std::pow(2.0f, std::ceil(std::log2(value)));

	if (value - lowerPowerOfTwo < upperPowerOfTwo - value) {
		return lowerPowerOfTwo;
	}
	return upperPowerOfTwo;
}

bool Editor::mIsManipulating = false;
