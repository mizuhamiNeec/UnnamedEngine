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
#include <engine/public/utils/StrUtil.h>
#include <engine/public/Window/WindowManager.h>

#include <math/public/MathLib.h>

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
}

void Editor::Update([[maybe_unused]] const float deltaTime) {
	if (auto currentScene = mSceneManager->GetCurrentScene()) {
		currentScene->Update(mGameTime->ScaledDeltaTime<float>());
	}

#ifdef _DEBUG
	// カメラの操作
	static float moveSpd = 4.0f;

	static bool firstReset   = true; // 初回リセットフラグ
	static bool cursorHidden = false;

	static bool  bOpenPopup = false; // ポップアップ表示フラグ
	static float popupTimer = 0.0f;

	if (InputSystem::IsPressed("attack2")) {
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
				Quaternion::Euler(Vec3::up * rot_.x + Vec3::right * rot_.y));

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

			Quaternion camRot = mCameraEntity->GetTransform()->GetWorldRot();
			Vec3       cameraForward = camRot * Vec3::forward;
			Vec3       cameraRight = camRot * Vec3::right;
			Vec3       cameraUp = camRot * Vec3::up;

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
				mCameraEntity->GetTransform()->GetWorldPos() + (cameraForward *
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

	// 移動速度が変更されたらImGuiで現在の移動速度をポップアップで表示
	if (bOpenPopup) {
		// ビューポートのサイズと位置を取得
		auto   lt           = Unnamed::Engine::GetViewportLT();
		auto   size         = Unnamed::Engine::GetViewportSize();
		ImVec2 viewportPos  = {lt.x, lt.y};
		ImVec2 viewportSize = {size.x, size.y};
		auto   windowSize   = ImVec2(256.0f, 32.0f);

		// ウィンドウの中央下部位置を計算
		ImVec2 windowPos(
			viewportPos.x + (viewportSize.x) * 0.5f,
			viewportPos.y + (viewportSize.y) * 0.75f
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

	ImGui::ShowDemoWindow();

	// アウトライナウィンドウの開始
	if (ImGui::Begin("Outliner")) {
		if (ImGui::Button("Add Entity")) {
			mScene->AddEntity(
				new Entity("New Entity"));
		}

		// テーブルの開始
		if (ImGui::BeginTable(
			"OutlinerTable", 3,
			ImGuiTableFlags_NoBordersInBody |
			ImGuiTableFlags_SizingFixedFit |
			ImGuiTableFlags_RowBg
		)) {
			// カラムの設定
			ImGui::TableSetupColumn(
				"Name",
				ImGuiTableColumnFlags_NoHide |
				ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed,
			                        30.0f);
			ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed,
			                        30.0f);

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
						ImGuiTreeNodeFlags_AllowItemOverlap |
						ImGuiTreeNodeFlags_DefaultOpen;

					if (entity->GetChildren().empty()) {
						flags |= ImGuiTreeNodeFlags_Leaf;
					}
					if (entity == mSelectedEntity) {
						flags |= ImGuiTreeNodeFlags_Selected;
					}

					ImGui::AlignTextToFramePadding();
					bool nodeOpen = ImGui::TreeNodeEx(
						(StrUtil::ConvertToUtf8(kIconObject) +
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
					// アイコン間のスペースを調整
					float originalScale     = ImGui::GetFont()->Scale;
					ImGui::GetFont()->Scale = 1.2f; // スケールを1.2倍に
					ImGui::PushFont(ImGui::GetFont());

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
						ImGuiSelectableFlags_DontClosePopups
					)) {
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

	// タブの名前
	static const char* tabNames[] = {
		"Test",
		"いい感じのヘッダー",
		"Physics",
		"Audio",
		"Input"
	};
	static int selectedTab = 0; // 現在選択中のタブインデックス

	// 縦方向のレイアウトを作成
	ImGui::Begin("Vertical Tabs Example");

	// タブ用の列を分割
	ImGui::Columns(2, nullptr, false);

	// タブのリスト (左側の列)
	ImGui::BeginChild("Tabs", ImVec2(512, 0), true);
	for (int i = 0; i < IM_ARRAYSIZE(tabNames); i++) {
		// ボタンまたは選択可能なアイテムとしてタブを表現
		if (ImGui::Selectable(
			(StrUtil::ConvertToUtf8(kIconTerminal) + tabNames[i]).c_str(),
			selectedTab == i)) {
			selectedTab = i; // タブを切り替える
		}
	}
	ImGui::EndChild();

	// コンテンツ表示エリア (右側の列)
	ImGui::NextColumn();
	ImGui::BeginChild("Content", ImVec2(0, 0), false);
	ImGui::Text("Content for tab: %s", tabNames[selectedTab]);
	ImGui::EndChild();

	ImGui::End();

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

#ifdef _DEBUG
	auto viewport = static_cast<ImGuiViewportP*>(static_cast<void*>(
		ImGui::GetMainViewport()));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 10.0f);

	if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down,
	                                38, window_flags)) {
		if (ImGui::BeginMenuBar()) {
			ImGui::PopStyleVar();

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
}

void Editor::Render() const {
	if (auto currentScene = mSceneManager->GetCurrentScene()) {
		currentScene->Render();
		mCameraEntity->Render(Unnamed::Engine::GetRenderer()->GetCommandList());
	}
}

void Editor::ShowDockSpace() {
#ifdef _DEBUG
	static bool* p_open;

	static bool               opt_fullscreen  = true;
	static bool               opt_padding     = false;
	static ImGuiDockNodeFlags dockspace_flags =
		ImGuiDockNodeFlags_PassthruCentralNode;

	// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
	// because it would be confusing to have two docking targets within each others.
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen) {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus;
	} else {
		dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
	}

	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;

	if (!opt_padding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", p_open, window_flags);
	if (!opt_padding)
		ImGui::PopStyleVar();

	if (opt_fullscreen)
		ImGui::PopStyleVar(2);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}

	if (ImGui::BeginMenuBar()) {
		// if (ImGui::BeginMenu("File")) {
		// 	ImGui::EndMenu();
		// }
		// if (ImGui::BeginMenu("Edit")) {
		// 	if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
		// 	if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
		// 	ImGui::Separator();
		// 	if (ImGui::MenuItem("Cut", "CTRL+X")) {}
		// 	if (ImGui::MenuItem("Copy", "CTRL+C")) {}
		// 	if (ImGui::MenuItem("Paste", "CTRL+V")) {}
		// 	ImGui::EndMenu();
		// }

		if (ImGui::BeginMenu("Options")) {
			// Disabling fullscreen would allow the window to be moved to the front of other windows,
			// which we can't undo at the moment without finer window depth/z control.
			ImGui::MenuItem("Fullscreen", nullptr, &opt_fullscreen);
			ImGui::MenuItem("Padding", nullptr, &opt_padding);
			ImGui::Separator();

			if (ImGui::MenuItem("Flag: NoDockingOverCentralNode", "",
			                    (dockspace_flags &
				                    ImGuiDockNodeFlags_NoDockingOverCentralNode)
			                    != 0)) {
				dockspace_flags ^= ImGuiDockNodeFlags_NoDockingOverCentralNode;
			}
			if (ImGui::MenuItem("Flag: NoDockingSplit", "",
			                    (dockspace_flags &
				                    ImGuiDockNodeFlags_NoDockingSplit) != 0)) {
				dockspace_flags ^= ImGuiDockNodeFlags_NoDockingSplit;
			}
			if (ImGui::MenuItem("Flag: NoUndocking", "",
			                    (dockspace_flags &
				                    ImGuiDockNodeFlags_NoUndocking) != 0)) {
				dockspace_flags ^= ImGuiDockNodeFlags_NoUndocking;
			}
			if (ImGui::MenuItem("Flag: NoResize", "",
			                    (dockspace_flags & ImGuiDockNodeFlags_NoResize)
			                    != 0)) {
				dockspace_flags ^= ImGuiDockNodeFlags_NoResize;
			}
			if (ImGui::MenuItem("Flag: AutoHideTabBar", "",
			                    (dockspace_flags &
				                    ImGuiDockNodeFlags_AutoHideTabBar) != 0)) {
				dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar;
			}
			if (ImGui::MenuItem("Flag: PassthruCentralNode", "",
			                    (dockspace_flags &
				                    ImGuiDockNodeFlags_PassthruCentralNode) !=
			                    0, opt_fullscreen)) {
				dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode;
			}
			ImGui::Separator();

			if (ImGui::MenuItem("Close", nullptr, false, p_open != nullptr)) {
				*p_open = false;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::End();
#endif
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
