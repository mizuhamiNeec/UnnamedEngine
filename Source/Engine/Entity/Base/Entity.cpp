#include "Entity.h"

#include <Debug/Debug.h>

#include <SubSystem/Console/ConVarManager.h>

#include "Engine.h"
#include "imgui_internal.h"
#include "Camera/CameraManager.h"
#include "Components/Camera/CameraComponent.h"
#include "Components/ColliderComponent/Base/ColliderComponent.h"
#include "ImGuiManager/ImGuiManager.h"
#include "Lib/DebugHud/DebugHud.h"
#include "Physics/PhysicsEngine.h"
#include "Window/WindowManager.h"

Entity::~Entity() {
	/*if (auto* collider = GetComponent<ColliderComponent>()) {
		if (collider) {
			if (collider->GetPhysicsEngine()) {
				collider->GetPhysicsEngine()->UnregisterEntity(this);
			}
		}
	}*/
}

void Entity::Update(const float deltaTime) {
	if (!bIsActive_) {
		return;
	}

	// 必須コンポーネントの更新
	if (!mTransform.get()) {
		Console::Print(
			std::format("Entity '{}' has no TransformComponent!", GetName()),
			Vec4(1, 0, 0, 1), Channel::General);
		return;
	}

	mTransform->Update(deltaTime);

	for (const auto& component : mComponents) {
		if (component->IsEditorOnly() /* && !bIsInEditor*/) {
			continue; // エディター専用のコンポーネントはゲーム中には更新しない
		}
		component->Update(deltaTime);
	}

	if (ConVarManager::GetConVar("ent_axis")->GetValueAsBool()) {
		Vec3 worldPos = GetTransform()->GetWorldPos();
		Vec2 screenSize = Engine::GetViewportSize();

		Debug::DrawAxis(worldPos,
			GetTransform()->GetWorldRot());

		Vec3 cameraPos = CameraManager::GetActiveCamera()->GetViewMat().
			Inverse().GetTranslate();

		// カメラとの距離を計算
		float distance = (worldPos - cameraPos).Length();

		// カメラとの距離が一定以下の場合は描画しない
		if (distance < Math::HtoM(4.0f)) {
			return;
		}

		bool  bIsOffscreen = false;
		float outAngle = 0.0f;

		Vec2 scrPosition = Math::WorldToScreen(
			worldPos,
			screenSize,
			false,
			0.0f,
			bIsOffscreen,
			outAngle
		);

		if (!bIsOffscreen) {
#ifdef _DEBUG
//auto   viewport  = ImGui::GetMainViewport();
			ImVec2 screenPos = {
				Engine::GetViewportLT().x, Engine::GetViewportLT().y
			};
			ImGui::SetNextWindowPos(screenPos);
			ImGui::SetNextWindowSize({ screenSize.x, screenSize.y });
			ImGui::SetNextWindowBgAlpha(0.0f); // 背景を透明にする
			ImGui::Begin("##EntityName", nullptr,
				ImGuiWindowFlags_NoBackground |
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_NoNav
			);

			ImVec2      textPos = { scrPosition.x, scrPosition.y };
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			float outlineSize = 1.0f;

			ImVec4 textColor = ToImVec4(Vec4::white);

			ImGuiManager::TextOutlined(
				drawList,
				textPos,
				mName.c_str(),
				textColor,
				ToImVec4(kDebugHudOutlineColor),
				outlineSize
			);

			ImGui::End();
#endif
		}
	}

	if (!GetChildren().empty()) {
		// 子のエンティティの更新
		for (const auto& child : mChildren) {
			if (child) {
				child->Update(deltaTime);
			}
		}
	}
}

void Entity::Render(ID3D12GraphicsCommandList* commandList) const {
	if (!bIsVisible_) {
		return;
	}

	for (const auto& component : mComponents) {
		if (component->IsEditorOnly() /* && !bIsInEditor*/) {
			continue; // エディター専用のコンポーネントはゲーム中には描画しない
		}
		component->Render(commandList);
	}
	// 子エンティティの描画
	for (const auto& child : mChildren) {
		child->Render(commandList);
	}
}

EntityType Entity::GetType() const {
	return mEntityType;
}

void Entity::SetType(const EntityType& type) {
	mEntityType = type;
}

TransformComponent* Entity::GetTransform() const {
	return mTransform.get();
}

bool Entity::IsActive() const {
	return bIsActive_;
}

void Entity::SetActive(const bool active) {
	bIsActive_ = active;
}

bool Entity::IsVisible() const {
	return bIsVisible_;
}

void Entity::SetVisible(const bool visible) {
	bIsVisible_ = visible;
}

void Entity::SetParent(Entity* newParent) {
	// 循環参照チェック
	const Entity* check = newParent;
	while (check) {
		if (check == this) {
			Console::Print(
				std::format("Entity '{}': Circular parenting detected!", mName),
				Vec4(1, 0, 0, 1), Channel::General);
			return;
		}
		check = check->mParent;
	}

	if (mParent == newParent) {
		return; // 変更なし
	}

	// 現在のワールド変換を保存
	Vec3       currentWorldPos;
	Quaternion currentWorldRot;
	Vec3       currentWorldScale;

	if (mTransform) {
		currentWorldPos = mTransform->GetWorldPos();
		currentWorldRot = mTransform->GetWorldRot();
		currentWorldScale = mTransform->GetWorldScale();
	}

	// 既存の親から削除
	if (mParent) {
		auto it = std::ranges::find(mParent->mChildren, this);
		if (it != mParent->mChildren.end()) {
			mParent->mChildren.erase(it);
		}
	}

	// 新しい親を設定
	mParent = newParent;

	// トランスフォームの更新
	if (mTransform) {
		if (mParent && mParent->GetTransform()) {
			// 親のワールド変換を取得
			Vec3 parentWorldPos = mParent->GetTransform()->GetWorldPos();
			Quaternion parentWorldRot = mParent->GetTransform()->GetWorldRot();
			Vec3 parentWorldScale = mParent->GetTransform()->GetWorldScale();

			// 位置の計算
			Vec3 localPos = currentWorldPos - parentWorldPos;
			// 親の回転の逆回転を適用
			localPos = parentWorldRot.Inverse().RotateVector(localPos);
			// スケールの補正
			localPos = localPos / parentWorldScale;

			// 回転の計算
			Quaternion localRot = parentWorldRot.Inverse() * currentWorldRot;

			// スケールの計算
			Vec3 localScale = currentWorldScale / parentWorldScale;

			mTransform->SetLocalPos(localPos);
			mTransform->SetLocalRot(localRot);
			mTransform->SetLocalScale(localScale);
		} else {
			mTransform->SetWorldPos(currentWorldPos);
			mTransform->SetWorldRot(currentWorldRot);
			mTransform->SetWorldScale(currentWorldScale);
		}

		mTransform->MarkDirty();

		// 子のトランスフォーム更新
		for (auto* child : mChildren) {
			if (child->mTransform) {
				child->mTransform->MarkDirty();
			}
		}
	}

	// 新しい親の子リストに追加
	if (mParent) {
		mParent->mChildren.emplace_back(this);
	}
}

Entity* Entity::GetParent() const {
	return mParent;
}

const std::vector<Entity*>& Entity::GetChildren() const {
	return mChildren;
}

void Entity::AddChild(Entity* child) {
	if (std::ranges::find(mChildren, child) == mChildren.end()) {
		mChildren.emplace_back(child);
		child->SetParent(this);
	}
}

void Entity::RemoveChild(Entity* child) {
	auto it = std::ranges::remove(mChildren, child).begin();
	if (it != mChildren.end()) {
		mChildren.erase(it);
		child->SetParent(nullptr);
	}
}

std::string& Entity::GetName() {
	return mName;
}

void Entity::SetName(const std::string& name) {
	mName = name;
}

void Entity::RemoveAllComponents() {
	mComponents.clear();
}
