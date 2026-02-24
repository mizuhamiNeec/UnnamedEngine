#include <pch.h>

#include <editor/Editor.h>

#include <engine/Engine.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Components/Base/Component.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Entity/EntityLoader.h>
#include <engine/ImGui/Icons.h>
#include <engine/ImGui/ImGuiUtil.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/Console.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/EngineServices.h>
#include <engine/SceneManager/SceneManager.h>

#include <runtime/core/Properties.h>
#include <runtime/core/math/Math.h>

#include "engine/unnamed/subsystem/console/concommand/UnnamedConVar.h"

#ifdef _DEBUG
#include <imgui_internal.h>
// ImGuizmoのインクルードはImGuiより後
#include <ImGuizmo.h>
#endif

/// @brief コンストラクタ
/// @param sceneManager シーンマネージャーへのポインタ
/// @param gameTime ゲームタイムへのポインタ
Editor::Editor(SceneManager* sceneManager, GameTime* gameTime)
	:
	mSceneManager(sceneManager),
	mGameTime(gameTime) {
	mScene = mSceneManager->GetCurrentScene();
	Init();
}

Editor::~Editor() {
	CameraManager::RemoveCamera(mCamera);
	mCamera.reset();
	mCameraEntityRaw = nullptr;
}

void Editor::ActivateEditorCamera() const {
	if (!mCamera) { return; }
	CameraManager::AddCamera(mCamera);
	CameraManager::SetActiveCamera(mCamera);
}

/// @brief エディターの初期化
void Editor::Init() {
	// カメラの作成
	mCameraEntity = std::make_unique<Entity>(
		"editorCamera",
		EntityType::EditorOnly
	);
	mCameraEntity->GetTransform()->SetLocalPos(
		Vec3::forward * -5.0f + Vec3::up * 2.0f
	);
	mCameraEntity->GetTransform()->SetLocalRot(
		Quaternion::Euler(Vec3::right * 15.0f * Math::deg2Rad)
	);

	// 生ポインタを取得
	CameraComponent* rawCameraPtr = mCameraEntity->AddComponent<
		CameraComponent>();
	// 生ポインタを std::shared_ptr に変換
	mCamera = std::shared_ptr<CameraComponent>(
		rawCameraPtr, [](CameraComponent*) {}
	);

	// カメラを CameraManager に追加
	ActivateEditorCamera();

	auto* camRaw = mCameraEntity.get();
	if (auto scene = mSceneManager->GetCurrentScene()) {
		scene->AddEntity(mCameraEntity.get());
	}
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
			0.78f, 0.12f, 0.12f, 1.0f
		);
		imGuizmoStyle.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(
			0.2f, 0.78f, 0.12f, 1.0f
		);
		imGuizmoStyle.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(
			0.12f, 0.43f, 0.78f, 1.0f
		);
		imGuizmoStyle.Colors[ImGuizmo::PLANE_X] = ImVec4(
			0.666f, 0.000f, 0.000f, 0.380f
		);
		imGuizmoStyle.Colors[ImGuizmo::PLANE_Y] = ImVec4(
			0.000f, 0.666f, 0.000f, 0.380f
		);
		imGuizmoStyle.Colors[ImGuizmo::PLANE_Z] = ImVec4(
			0.000f, 0.000f, 0.666f, 0.380f
		);
		imGuizmoStyle.Colors[ImGuizmo::SELECTION] = ImVec4(
			1.000f, 0.500f, 0.062f, 0.541f
		);
		imGuizmoStyle.Colors[ImGuizmo::INACTIVE] = ImVec4(
			0.600f, 0.600f, 0.600f, 0.600f
		);
		imGuizmoStyle.Colors[ImGuizmo::TRANSLATION_LINE] = ImVec4(
			0.666f, 0.666f, 0.666f, 0.666f
		);
		imGuizmoStyle.Colors[ImGuizmo::SCALE_LINE] = ImVec4(
			0.250f, 0.250f, 0.250f, 1.000f
		);
		imGuizmoStyle.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(
			1.000f, 0.500f, 0.062f, 1.000f
		);
		imGuizmoStyle.Colors[ImGuizmo::ROTATION_USING_FILL] = ImVec4(
			1.000f, 0.500f, 0.062f, 0.500f
		);
		imGuizmoStyle.Colors[ImGuizmo::HATCHED_AXIS_LINES] = ImVec4(
			0.000f, 0.000f, 0.000f, 0.500f
		);
		imGuizmoStyle.Colors[ImGuizmo::TEXT] = ImVec4(
			1.000f, 1.000f, 1.000f, 1.000f
		);
		imGuizmoStyle.Colors[ImGuizmo::TEXT] = ImVec4(
			1.000f, 1.000f, 1.000f, 1.000f
		);
		imGuizmoStyle.Colors[ImGuizmo::TEXT_SHADOW] = ImVec4(
			0.000f, 0.000f, 0.000f, 1.000f
		);

		ImGuizmo::GetStyle() = imGuizmoStyle;
	}
#endif
}

#ifdef _DEBUG
void Editor::DrawMenuBars() {
	DrawMainMenuBar();
	DrawTopBar();
	DrawStatusBar();
	DrawSideBar();
}
#endif

/// @brief エディターの更新
void Editor::Update([[maybe_unused]] const float deltaTime) {
	if (!mSceneManager) { return; }

	const auto currentScene = mSceneManager->GetCurrentScene();
	if (mScene != currentScene) {
		mScene          = currentScene;
		mSelectedEntity = nullptr;
	}

	if (mScene && mCameraEntity) {
		bool hasEditorCamera = false;
		for (auto* entity : mScene->GetEntities()) {
			if (entity == mCameraEntity.get()) {
				hasEditorCamera = true;
				break;
			}
		}

		if (!hasEditorCamera) { mScene->AddEntity(mCameraEntity.get()); }
	}

	// シーン遷移直後などでアクティブカメラが無い場合のみ Editor カメラを復帰させる
	if (!CameraManager::GetActiveCamera()) { ActivateEditorCamera(); }

	// シーンのインポート処理
	if (mLoadFilePath) {
		if (currentScene && mEntityLoader) {
			auto* engine          = Unnamed::EngineServices::Get();
			auto* resourceManager = engine ?
				                        engine->GetResourceManagerInstance() :
				                        nullptr;
			if (resourceManager) {
				mEntityLoader->LoadScene(
					mLoadFilePath.value(), currentScene.get(),
					resourceManager
				);
				Msg(
					"Editor",
					"Scene imported from: {}",
					mLoadFilePath.value()
				);
			}
		}
		mLoadFilePath.reset(); // ロード後はリセット
	}

#ifdef _DEBUG

	// インスペクタ
	DrawInspector();

#endif

	auto con = ServiceLocator::Get<Unnamed::ConsoleSystem>();

	auto ed_gridcolor_r = dynamic_cast<Unnamed::UnnamedConVar<float>*>(con->
		GetConVar("ed_gridcolor_r"));
	auto ed_gridcolor_g = dynamic_cast<Unnamed::UnnamedConVar<float>*>(con->
		GetConVar("ed_gridcolor_g"));
	auto ed_gridcolor_b = dynamic_cast<Unnamed::UnnamedConVar<float>*>(con->
		GetConVar("ed_gridcolor_b"));

	auto ed_gridmajorcolor_r = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridmajorcolor_r"));
	auto ed_gridmajorcolor_g = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridmajorcolor_g"));
	auto ed_gridmajorcolor_b = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridmajorcolor_b"));

	auto ed_gridaxiscolor_r = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridaxiscolor_r"));
	auto ed_gridaxiscolor_g = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridaxiscolor_g"));
	auto ed_gridaxiscolor_b = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridaxiscolor_b"));

	auto ed_gridminorcolor_r = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridminorcolor_r"));
	auto ed_gridminorcolor_g = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridminorcolor_g"));
	auto ed_gridminorcolor_b = dynamic_cast<Unnamed::UnnamedConVar<float>*>(
		con->GetConVar("ed_gridminorcolor_b"));

	auto gridColor = Vec4(
		ed_gridcolor_r->GetValue(),
		ed_gridcolor_g->GetValue(),
		ed_gridcolor_b->GetValue()
	);
	auto majorLineColor = Vec4(
		ed_gridmajorcolor_r->GetValue(),
		ed_gridmajorcolor_g->GetValue(),
		ed_gridmajorcolor_b->GetValue()
	);
	auto axisLineColor = Vec4(
		ed_gridaxiscolor_r->GetValue(),
		ed_gridaxiscolor_g->GetValue(),
		ed_gridaxiscolor_b->GetValue()
	);
	auto minorLineColor = Vec4(
		ed_gridminorcolor_r->GetValue(),
		ed_gridminorcolor_g->GetValue(),
		ed_gridminorcolor_b->GetValue()
	);

	auto activeCamera = CameraManager::GetActiveCamera();
	if (!activeCamera) { return; }

	// グリッドの表示
	DrawGrid(
		mGridSize,
		mGridRange,
		gridColor,
		majorLineColor,
		axisLineColor,
		minorLineColor,
		activeCamera->GetViewMat().Inverse().GetTranslate(),
		mGridSize * 128.0f
	);

#ifdef _DEBUG
	// ギズモの操作はエンティティの更新前に行う
	auto* engine = Unnamed::EngineServices::Get();
	Vec2  vLT    = engine ? engine->GetViewportLTInstance() : Vec2{};
	Vec2  vSize  = engine ? engine->GetViewportSizeInstance() : Vec2{};
	ImGuizmo::SetRect(
		vLT.x, vLT.y,
		vSize.x, vSize.y
	);

	Mat4 view = activeCamera->GetViewMat();
	Mat4 proj = activeCamera->GetProjMat();

	if (mSelectedEntity) {
		Mat4 worldMat = mSelectedEntity->GetTransform()->GetLocalMat();

		static ImGuizmo::MODE      mode      = ImGuizmo::MODE::LOCAL;
		static ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;

		static bool bIsWorldMode = true;
		if (InputSystem::IsTriggered("togglegizmo")) {
			bIsWorldMode = !bIsWorldMode;
		}

		if (bIsWorldMode) { mode = ImGuizmo::MODE::WORLD; } else {
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

	if (mSceneManager->GetCurrentScene()) {
		currentScene->Update(mGameTime->ScaledDeltaTime<float>());
	}

#ifdef _DEBUG
	// カメラの操作
	{
		static float moveSpd = 4.0f;

		static bool firstReset   = true; // 初回リセットフラグ
		static bool cursorHidden = false;

		static bool  bOpenPopup = false; // ポップアップ表示フラグ
		static float popupTimer = 0.0f;

		auto*  engine2 = Unnamed::EngineServices::Get();
		auto   lt = engine2 ? engine2->GetViewportLTInstance() : Vec2{};
		auto   size = engine2 ? engine2->GetViewportSizeInstance() : Vec2{};
		ImVec2 viewportPos = {lt.x, lt.y};
		ImVec2 viewportSize = {size.x, size.y};
		auto   mousePos = ImGui::GetMousePos();

		bool bIsInsideViewport =
			mousePos.x >= viewportPos.x &&
			mousePos.x <= viewportPos.x + viewportSize.x &&
			mousePos.y >= viewportPos.y &&
			mousePos.y <= viewportPos.y + viewportSize.y;

		if (InputSystem::IsPressed("attack2") && bIsInsideViewport) {
			ActivateEditorCamera();

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

				rot_.y = std::clamp(
					rot_.y, min * Math::deg2Rad,
					max * Math::deg2Rad
				);

				mCameraEntity->GetTransform()->SetWorldRot(
					Quaternion::Euler(
						Vec3::up * rot_.x + Vec3::right * rot_.y
					)
				);

				Vec3 moveInput = {0.0f, 0.0f, 0.0f};

				if (InputSystem::IsPressed("forward")) { moveInput.z += 1.0f; }

				if (InputSystem::IsPressed("back")) { moveInput.z -= 1.0f; }

				if (InputSystem::IsPressed("moveright")) {
					moveInput.x += 1.0f;
				}

				if (InputSystem::IsPressed("moveleft")) { moveInput.x -= 1.0f; }

				if (InputSystem::IsPressed("moveup")) { moveInput.y += 1.0f; }

				if (InputSystem::IsPressed("movedown")) { moveInput.y -= 1.0f; }

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
			ClientToScreen(
				OldWindowManager::GetMainWindow()->GetWindowHandle(),
				&centerCursorPos
			); // クライアント座標をスクリーン座標に変換
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
						 (Unnamed::StrUtil::ConvertToUtf8(0xe9e4) + std::format(
							  " {:.2f}", moveSpd
						  )).c_str()
					 ).x) * 0.5f,
					(windowSize.y - ImGui::GetFontSize()) * 0.5f
				)
			);
			ImGui::Text(
				(Unnamed::StrUtil::ConvertToUtf8(0xe9e4) + " %.2f").c_str(),
				moveSpd
			);

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

	DrawOutliner();

#endif
}

/// @brief エディタのレンダリング処理
void Editor::Render() const {
	if (!mSceneManager) { return; }

	if (auto currentScene = mSceneManager->GetCurrentScene()) {
		currentScene->Render();
		if (auto* engine3 = Unnamed::EngineServices::Get()) {
			if (auto* r = engine3->GetRendererInstance(); r && mCameraEntity) {
				mCameraEntity->Render(r->GetCommandList());
			}
		}
	}
}

void Editor::SetEntityLoader(EntityLoader* entityLoader) {
	mEntityLoader = entityLoader;
}

/// @brief ギズモ操作中かどうかを取得する
/// @return ギズモ操作中であればtrue、そうでなければfalse
bool Editor::IsManipulating() { return mIsManipulating; }

#ifdef _DEBUG
/// @brief インスペクタウィンドウを描画します
void Editor::DrawInspector() const {
	if (ImGui::Begin("Inspector")) {
		if (mSelectedEntity) {
			ImGui::Text("Name: %s", mSelectedEntity->GetName().c_str());

			mSelectedEntity->GetTransform()->DrawInspectorImGui();

			// コンポーネントの一覧表示と編集
			const auto& components = mSelectedEntity->GetComponents<
				Component>();
			for (const auto& component : components) {
				if (component) { component->DrawInspectorImGui(); }
			}
		}
	}
	ImGui::End();
}

/// @brief アウトライナウィンドウを描画します
void Editor::DrawOutliner() {
	if (ImGui::Begin("Outliner")) {
		if (ImGui::Button("Add Entity") && mScene) {
			mScene->AddEntity(NEW Entity("New Entity"));
		}

		static std::string buttonLabel = Unnamed::StrUtil::ConvertToUtf8(
			                                 kIconFilter
		                                 ) +
		                                 " " + Unnamed::StrUtil::ConvertToUtf8(
			                                 kIconDropDown
		                                 );

		if (ImGui::Button(buttonLabel.c_str())) {
			ImGui::OpenPopup("filterPopup");
		}

		if (ImGui::BeginPopup("filterPopup")) {
			ImGui::Button(
				Unnamed::StrUtil::ConvertToUtf8(kIconVisibility).c_str()
			);
			ImGui::SameLine();
			ImGui::Button(
				Unnamed::StrUtil::ConvertToUtf8(kIconPower).c_str()
			);
			ImGui::SameLine();
			ImGui::Button(
				Unnamed::StrUtil::ConvertToUtf8(kIconArrowBack).c_str()
			);
			ImGui::EndPopup();
		}

		constexpr ImGuiTableFlags tableFlags =
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_BordersOuterH |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_NoBordersInBodyUntilResize;

		// テーブルで表示
		if (ImGui::BeginTable("OutlinerTable", 3, tableFlags)) {
			// カラムの設定
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
			ImGui::TableSetupColumn(
				"Visible", ImGuiTableColumnFlags_WidthFixed
			);
			ImGui::TableSetupColumn(
				"Active", ImGuiTableColumnFlags_WidthFixed
			);

			// 再帰的にエンティティを表示する関数
			std::function<void(Entity*)>
				drawEntityNode =
					[&](Entity* entity) {
					ImGui::PushID(entity);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					// エンティティ名とツリー構造
					ImGuiTreeNodeFlags flags =
						ImGuiTreeNodeFlags_SpanAvailWidth |
						ImGuiTreeNodeFlags_DefaultOpen |
						ImGuiTreeNodeFlags_OpenOnArrow |
						ImGuiTreeNodeFlags_DrawLinesFull;

					if (entity->GetChildren().empty()) {
						flags |= ImGuiTreeNodeFlags_Leaf;
					}
					if (entity == mSelectedEntity) {
						flags |= ImGuiTreeNodeFlags_Selected;
					}

					ImGui::AlignTextToFramePadding();
					bool nodeOpen = ImGui::TreeNodeEx(
						(Unnamed::StrUtil::ConvertToUtf8(kIconEntity) +
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
									if (nodeOpen) { ImGui::TreePop(); }
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
					{
						ImGui::TableNextColumn();
						bool visible = entity->IsVisible();

						if (ImGuiWidgets::IconButton(
							Unnamed::StrUtil::ConvertToUtf8(
								visible ?
									kIconVisibility :
									kIconVisibilityOff
							).c_str(),
							nullptr,
							ImVec2(22.0f, 22.0f)
						)) { entity->SetVisible(!visible); }
					}

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
}

void Editor::DrawMainMenuBar() {
	// メニューバーを少し高くする
	ImGui::PushStyleVar(
		ImGuiStyleVar_FramePadding,
		ImVec2(
			0.0f, kTitleBarH * 0.5f -
			      ImGui::GetFontSize() * 0.5f
		)
	);
	ImGui::PushStyleVar(
		ImGuiStyleVar_ItemSpacing,
		ImVec2(0.0f, kTitleBarH)
	);
	if (ImGui::BeginMainMenuBar()) {
		ImGui::PopStyleVar(2); // メニューバーのスタイルを元に戻す
		// アイコンメニュー
		ImGui::PushStyleColor(
			ImGuiCol_Text,
			ImVec4(0.13f, 0.5f, 1.0f, 1.0f)
		);

		if (ImGuiWidgets::BeginMainMenu(
			Unnamed::StrUtil::ConvertToUtf8(kIconArrowBack).c_str()
		)) {
			ImGui::PopStyleColor();
			if (ImGui::MenuItemEx(
				"About Unnamed",
				nullptr
			)) {}
			ImGui::EndMenu();
		} else { ImGui::PopStyleColor(); }

		if (ImGuiWidgets::BeginMainMenu("File")) {
			ImGui::BeginDisabled();
			if (ImGui::MenuItemEx(
				"Save",
				Unnamed::StrUtil::ConvertToUtf8(kIconSave).c_str()
			)) {}

			if (ImGui::MenuItemEx(
				"Save As",
				Unnamed::StrUtil::ConvertToUtf8(
					kIconSaveAs
				).
				c_str()
			)) {}
			ImGui::EndDisabled();

			ImGui::Separator();

			if (ImGui::MenuItemEx(
				"Import",
				Unnamed::StrUtil::ConvertToUtf8(
					kIconDownload
				)
				.
				c_str()
			)) {
				BaseScene* currentScene = mSceneManager->
				                          GetCurrentScene().
				                          get();
				if (currentScene) {
					char szFile[MAX_PATH] = ""; // 初期ファイル名は空

					OPENFILENAMEA ofn;
					ZeroMemory(&ofn, sizeof(ofn)); // 構造体をゼロ初期化
					ofn.lStructSize = sizeof(OPENFILENAMEA);

					HWND hwndOwner = nullptr;
					if (OldWindowManager::GetMainWindow()) {
						hwndOwner =
							OldWindowManager::GetMainWindow()->
							GetWindowHandle();
					}
					ofn.hwndOwner   = hwndOwner;
					ofn.lpstrFilter =
						"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile  = szFile;
					ofn.nMaxFile   = MAX_PATH;
					ofn.lpstrTitle = "Import Scene From";
					ofn.Flags      = OFN_PATHMUSTEXIST |
					                 OFN_FILEMUSTEXIST |
					                 OFN_NOCHANGEDIR;
					// ファイル/パス存在確認、カレントディレクトリ変更なし
					ofn.lpstrDefExt = "scene";

					if (GetOpenFileNameA(&ofn)) {
						mLoadFilePath = ofn.lpstrFile;
					}
				} else {
					Error(
						"ImportScene",
						"Import failed: No active scene found."
					);
				}
			}

			if (ImGui::MenuItemEx(
				"Export",
				Unnamed::StrUtil::ConvertToUtf8(
					kIconUpload
				).
				c_str()
			)) {
				BaseScene* currentScene = mSceneManager->
				                          GetCurrentScene().
				                          get();
				if (currentScene) {
					char szFile[MAX_PATH] = "scene.json";
					// デフォルトのファイル名

					OPENFILENAMEA ofn;
					ZeroMemory(&ofn, sizeof(ofn)); // 構造体をゼロ初期化
					ofn.lStructSize = sizeof(OPENFILENAMEA);

					HWND hwndOwner = nullptr;
					if (OldWindowManager::GetMainWindow()) {
						hwndOwner =
							OldWindowManager::GetMainWindow()->
							GetWindowHandle();
					}
					ofn.hwndOwner   = hwndOwner;
					ofn.lpstrFilter =
						"Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile  = szFile;
					ofn.nMaxFile   = MAX_PATH;
					ofn.lpstrTitle = "Export Scene As";
					ofn.Flags      = OFN_OVERWRITEPROMPT |
					                 OFN_NOCHANGEDIR;
					// 上書き確認、カレントディレクトリ変更なし
					ofn.lpstrDefExt = "scene";

					if (GetSaveFileNameA(&ofn)) {
						std::string filePath = ofn.lpstrFile;
						if (mEntityLoader) {
							mEntityLoader->
								SaveScene(filePath, currentScene);
							SpecialMsg(
								Unnamed::LogLevel::Success,
								"SceneExport",
								"Scene exported to: {}",
								filePath
							);
						}
					}
				} else {
					Error(
						"SceneExport",
						"Export failed: No active scene found."
					);
				}
			}

			ImGui::Separator();

			ImGui::BeginDisabled();
			if (ImGui::MenuItemEx(
					"Exit",
					Unnamed::StrUtil::ConvertToUtf8(kIconPower).c_str()
				)
			) { Console::SubmitCommand("quit"); }
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}

		if (ImGuiWidgets::BeginMainMenu("Edit")) {
			ImGui::Separator();
			if (ImGuiWidgets::MenuItemWithIcon(
				Unnamed::StrUtil::ConvertToUtf8(kIconSettings).c_str(),
				"Settings"
			)) {}

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void Editor::DrawTopBar() {
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
	constexpr auto toolbarIconSize = ImVec2(96.0f, 32.0f);

	const auto windowPadding = ImGui::GetStyle().WindowPadding;

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
			constexpr float iconScale = 1.0f;

			ImGui::BeginDisabled();

			ImGuiWidgets::IconButton(
				Unnamed::StrUtil::ConvertToUtf8(kIconVertex).c_str(),
				"Vertices",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				Unnamed::StrUtil::ConvertToUtf8(kIconEdge).c_str(), "Edges",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				Unnamed::StrUtil::ConvertToUtf8(kIconFace).c_str(), "Faces",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				Unnamed::StrUtil::ConvertToUtf8(kIconMesh).c_str(), "Meshes",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				Unnamed::StrUtil::ConvertToUtf8(kIconObject).c_str(), "Objects",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();

			ImGuiWidgets::IconButton(
				Unnamed::StrUtil::ConvertToUtf8(kIconGroup).c_str(), "Groups",
				toolbarIconSize,
				iconScale,
				ImGuiDir_Right
			);

			ImGui::SameLine();
			if (ImGui::Button("EditorCam")) { ActivateEditorCamera(); }
		}

		ImGui::EndDisabled();

		ImGui::End();
	}
}

void Editor::DrawSideBar() {
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

		constexpr auto toolbarIconSize = ImVec2(iconSize, iconSize);

		const auto windowPadding = ImGui::GetStyle().WindowPadding;

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
					Unnamed::StrUtil::ConvertToUtf8(kIconSelect).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					Unnamed::StrUtil::ConvertToUtf8(kIconMove).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					Unnamed::StrUtil::ConvertToUtf8(kIconRotate).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					Unnamed::StrUtil::ConvertToUtf8(kIconScale).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					Unnamed::StrUtil::ConvertToUtf8(kIconPivot).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGui::Separator();

				ImGuiWidgets::IconButton(
					Unnamed::StrUtil::ConvertToUtf8(kIconObject).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);
				ImGuiWidgets::IconButton(
					Unnamed::StrUtil::ConvertToUtf8(kIconNANKABOX).c_str(), "",
					toolbarIconSize,
					iconScale,
					ImGuiDir_None
				);

				ImGuiWidgets::IconButton(
					Unnamed::StrUtil::ConvertToUtf8(kIconTexture).c_str(),
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
}

void Editor::DrawStatusBar() {
	if (
		ImGui::BeginViewportSideBar(
			"##MainStatusBar",
			ImGui::GetMainViewport(),
			ImGuiDir_Down,
			32.0f,
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoSavedSettings
		)
	) {
		// ステータスバーの幅を取得
		float statusBarWidth = ImGui::GetWindowWidth();

		// アングルスナップ
		{
			//const float windowHeight = ImGui::GetWindowSize().y;
			const char* items[] = {
				"0.25°", "0.5°", "1°", "5°", "5.625°", "11.25°", "15°",
				"22.5°", "30°", "45°", "90°"
			};
			static int  itemCurrentIndex = 6;
			const char* comboLabel       = items[itemCurrentIndex];

			ImGui::Text("Angle: ");

			ImGui::SameLine();

			// // 垂直中央に配置
			// float comboHeight = ImGui::GetFrameHeight();
			// float offsetY     = (windowHeight - comboHeight) * 0.5f;
			// ImGui::SetCursorPosY(offsetY);

			// コンボボックスの幅をステータスバーの幅に合わせて調整
			ImGui::PushItemWidth(statusBarWidth * 0.2f);
			if (ImGui::BeginCombo("##angle", comboLabel)) {
				for (int n = 0; n < IM_ARRAYSIZE(items); ++n) {
					const bool isSelected = (itemCurrentIndex == n);
					if (ImGui::Selectable(items[n], isSelected)) {
						itemCurrentIndex = n;
						mAngleSnap       = std::stof(
							items[itemCurrentIndex]
						);
					}
					if (isSelected) { ImGui::SetItemDefaultFocus(); }
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();
		}

		ImGui::SameLine();

		// グリッドスナップ
		{
			const char* items[] = {
				"0.125", "0.25", "0.5",
				"1", "2", "4", "8",
				"16", "32", "64", "128",
				"256", "512", "1024"
			};
			static int  itemCurrentIndex = 9;
			const char* comboLabel       = items[itemCurrentIndex];
			ImGui::Text("Grid: ");

			ImGui::SameLine();

			// コンボボックスの幅をステータスバーの幅に合わせて調整
			ImGui::PushItemWidth(statusBarWidth * 0.2f);
			ImGui::PushID("GridCombo"); // IDの衝突を避けるためにプッシュ

			if (ImGui::BeginCombo("##grid", comboLabel)) {
				for (int n = 0; n < IM_ARRAYSIZE(items); ++n) {
					const bool isSelected = (itemCurrentIndex == n);
					if (ImGui::Selectable(items[n], isSelected)) {
						itemCurrentIndex = n;
						// 選択された文字列を浮動小数点数に変換してgridSizeに設定
						mGridSize = std::stof(items[itemCurrentIndex]);
					}
					if (isSelected) { ImGui::SetItemDefaultFocus(); }
				}
				ImGui::EndCombo();
			}

			// コンボボックスにマウスオーバーしている時にホイールで操作
			if (ImGui::IsItemHovered()) {
				float wheel = ImGui::GetIO().MouseWheel;
				if (wheel != 0.0f) {
					itemCurrentIndex -= static_cast<int>(wheel);
					itemCurrentIndex = std::clamp(
						itemCurrentIndex, 0, IM_ARRAYSIZE(items) - 1
					);
					// 選択された文字列を浮動小数点数に変換してgridSize_に設定
					mGridSize = std::stof(items[itemCurrentIndex]);
				}
			}

			ImGui::PopID();
			ImGui::PopItemWidth();
		}

		ImGui::End();
	}
}
#endif

/// @brief グリッドを描画する
/// @param gridSize グリッドの間隔
/// @param range グリッドの描画範囲（中心からの距離）
/// @param color グリッドの基本色
/// @param majorColor 主要なグリッド線の色
/// @param axisColor 軸線の色
/// @param minorColor 細かいグリッド線の色
/// @param cameraPosition カメラの位置
/// @param drawRadius 描画する円形範囲の半径
void Editor::DrawGrid(
	const float gridSize, const float range,
	const Vec4& color,
	const Vec4& majorColor, const Vec4& axisColor, const Vec4& minorColor,
	const Vec3& cameraPosition,
	const float drawRadius
) {
	constexpr float majorInterval = 1024.0f;
	const float     minorInterval = gridSize * 8.0f;

	// 描画範囲の二乗を事前計算
	const float drawRadiusSq = drawRadius * drawRadius;

	// カメラ位置のXとZを事前に取得
	const float cameraPosX = cameraPosition.x;
	const float cameraPosZ = cameraPosition.z;

	// グリッドサイズの逆数を事前計算（除算を乗算に変換）
	const float     invGridSize      = 1.0f / gridSize;
	constexpr float invMajorInterval = 1.0f / majorInterval;

	// 主要線と軸線を先に描画（range全体から探す）
	// X軸方向の主要線
	{
		// 軸線（X=0）の描画
		if (std::abs(0.0f) <= range) {
			DebugDraw::DrawLine(
				Vec3(0, 0, -range), Vec3(0, 0, range), axisColor
			);
		}

		// 1024間隔の主要線を描画
		const int majorStartIdx = static_cast<int>(std::ceil(
			-range * invMajorInterval
		));
		const int majorEndIdx = static_cast<int>(std::floor(
			range * invMajorInterval
		));

		for (int i = majorStartIdx; i <= majorEndIdx; ++i) {
			if (i == 0) continue; // 軸線は既に描画済み
			const float x = i * majorInterval;
			if (std::abs(x) <= range) {
				DebugDraw::DrawLine(
					Vec3(x, 0, -range), Vec3(x, 0, range),
					majorColor
				);
			}
		}
	}

	// Z軸方向の主要線
	{
		// 軸線（Z=0）の描画
		if (std::abs(0.0f) <= range) {
			DebugDraw::DrawLine(
				Vec3(-range, 0, 0), Vec3(range, 0, 0), axisColor
			);
		}

		// 1024間隔の主要線を描画
		const int majorStartIdx = static_cast<int>(std::ceil(
			-range * invMajorInterval
		));
		const int majorEndIdx = static_cast<int>(std::floor(
			range * invMajorInterval
		));

		for (int i = majorStartIdx; i <= majorEndIdx; ++i) {
			if (i == 0) continue; // 軸線は既に描画済み
			const float z = i * majorInterval;
			if (std::abs(z) <= range) {
				DebugDraw::DrawLine(
					Vec3(-range, 0, z), Vec3(range, 0, z),
					majorColor
				);
			}
		}
	}

	// 通常のグリッド線（カメラ周辺のみ）
	const float cullingRadius = drawRadius;
	const float minX          = std::max(-range, cameraPosX - cullingRadius);
	const float maxX          = std::min(range, cameraPosX + cullingRadius);
	const float minZ          = std::max(-range, cameraPosZ - cullingRadius);
	const float maxZ          = std::min(range, cameraPosZ + cullingRadius);

	// X軸方向の通常グリッド線
	{
		const int startIdx = static_cast<int>(std::floor(minX * invGridSize));
		const int endIdx   = static_cast<int>(std::ceil(maxX * invGridSize));

		for (int i = startIdx; i <= endIdx; ++i) {
			const float x = i * gridSize;

			// range外の線はスキップ
			if (x < -range || x > range) continue;

			// 主要線と軸線は既に描画済みなのでスキップ
			const bool isAxis  = (i == 0);
			const bool isMajor = !isAxis && (std::abs(
				                                 std::fmod(x, majorInterval)
			                                 ) < 0.01f);
			if (isAxis || isMajor) continue;

			// 細線判定
			const bool isMinor = (std::abs(std::fmod(x, minorInterval)) <
			                      0.01f);

			// 通常のグリッド線は近距離のみ描画判定
			const float dx     = cameraPosX - x;
			const float distSq = dx * dx;
			if (distSq > drawRadiusSq) continue;

			// 色の決定
			Vec4 lineColor = isMinor ? minorColor : color;

			// 円形範囲内で描画（range内にクリップ）
			const float halfLen = std::sqrt(drawRadiusSq - distSq);
			const float startZ  = std::max(-range, cameraPosZ - halfLen);
			const float endZ    = std::min(range, cameraPosZ + halfLen);

			// 有効な範囲の場合のみ描画
			if (startZ < endZ && startZ <= range && endZ >= -range) {
				DebugDraw::DrawLine(
					Vec3(x, 0, startZ),
					Vec3(x, 0, endZ),
					lineColor
				);
			}
		}
	}

	// Z軸方向の通常グリッド線
	{
		const int startIdx = static_cast<int>(std::floor(minZ * invGridSize));
		const int endIdx   = static_cast<int>(std::ceil(maxZ * invGridSize));

		for (int i = startIdx; i <= endIdx; ++i) {
			const float z = i * gridSize;

			// range外の線はスキップ
			if (z < -range || z > range) continue;

			// 主要線と軸線は既に描画済みなのでスキップ
			const bool isAxis  = (i == 0);
			const bool isMajor = !isAxis && (std::abs(
				                                 std::fmod(z, majorInterval)
			                                 ) < 0.01f);
			if (isAxis || isMajor) continue;

			// 細線判定
			const bool isMinor = (std::abs(std::fmod(z, minorInterval)) <
			                      0.01f);

			// 通常のグリッド線は近距離のみ描画判定
			const float dz     = cameraPosZ - z;
			const float distSq = dz * dz;
			if (distSq > drawRadiusSq) continue;

			// 色の決定
			Vec4 lineColor = isMinor ? minorColor : color;

			// 円形範囲内で描画（range内にクリップ）
			const float halfLen = std::sqrt(drawRadiusSq - distSq);
			const float startX  = std::max(-range, cameraPosX - halfLen);
			const float endX    = std::min(range, cameraPosX + halfLen);

			// 有効な範囲の場合のみ描画
			if (startX < endX && startX <= range && endX >= -range) {
				DebugDraw::DrawLine(
					Vec3(startX, 0, z),
					Vec3(endX, 0, z),
					lineColor
				);
			}
		}
	}
}

/// @brief 指定した値に最も近い2のべき乗を返す
/// @param value 入力値
/// @return 最も近い2のべき乗
float Editor::RoundToNearestPowerOfTwo(const float value) {
	const float lowerPowerOfTwo = std::pow(2.0f, std::floor(std::log2(value)));
	const float upperPowerOfTwo = std::pow(2.0f, std::ceil(std::log2(value)));

	if (value - lowerPowerOfTwo < upperPowerOfTwo - value) {
		return lowerPowerOfTwo;
	}
	return upperPowerOfTwo;
}

bool Editor::mIsManipulating = false;
