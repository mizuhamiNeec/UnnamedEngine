#pragma once
#include <vector>

#include <engine/gameframework/component/base/BaseComponent.h>

#include <runtime/core/math/Math.h>

namespace Unnamed {
	/// @class TransformComponent
	/// @brief エンティティに空間をもたせるコンポーネントです。
	class TransformComponent : public BaseComponent {
	public:
		//---------------------------------------------------------------------
		// TransformComponent
		//---------------------------------------------------------------------
		[[nodiscard]] const Vec3&         Position() const noexcept;
		[[nodiscard]] const Quaternion&   Rotation() const noexcept;
		[[nodiscard]] const Vec3&         Scale() const noexcept;
		[[nodiscard]] TransformComponent* Parent() const;
		[[nodiscard]] const Mat4&         WorldMat() const noexcept;

		void SetPosition(const Vec3& newPosition);
		void SetRotation(const Quaternion& newRotation);
		void SetScale(const Vec3& newScale);
		void SetParent(TransformComponent* newParent);

		//---------------------------------------------------------------------
		// BaseComponent
		//---------------------------------------------------------------------
		void OnAttached() override;
		void OnDetached() override;

		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		void OnPreRender() const override;
		void OnRender() const override;
		void OnPostRender() const override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		[[nodiscard]] std::string_view GetComponentName() const override;

	private:
		void MarkDirty(); // 変化が合った場合のみ行列を更新する

		Vec3       mLocalPos = Vec3::zero;
		Quaternion mLocalRot = Quaternion::identity;
		Vec3       mLocalScale    = Vec3::one;

		Mat4 mWorldMat = Mat4::identity;

		TransformComponent*              mParent = nullptr;
		std::vector<TransformComponent*> mChildren;

		bool mIsDirty = false; // 変更があったか?
	};
}
