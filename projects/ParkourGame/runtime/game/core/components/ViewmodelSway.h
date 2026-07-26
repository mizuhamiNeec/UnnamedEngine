#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Quaternion.h"
#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	class TransformComponent;

	/// @brief ViewmodelSwayは、視点移動と回転から一人称モデルの位置・姿勢オフセットを計算します
	class ViewmodelSway : public BaseComponent {
	public:
		void OnAttached() override;
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const override;

		[[nodiscard]] TICK_GROUP GetTickGroup() const override;

	private:
		/// @brief 親のTransformComponentを取得します。無効な場合はnullptrを返します。
		/// @return 親のTransformComponentへのポインタ、無効な場合はnullptr
		[[nodiscard]] TransformComponent* GetParentTransform() const;

		TransformComponent* mTransform  = nullptr;
		TransformComponent* mLookSource = nullptr;

		Vec3       mBaseLocalPosition = Vec3::zero;
		Quaternion mBaseLocalRotation = Quaternion::identity;
		Vec2       mPrevLookDeg       = Vec2::zero;
		float      mPitch             = 0.0f;
		float      mYaw               = 0.0f;

		float mSwayAmount  = 0.0f;
		float mAttenuation = 16.0f;

		float mLocationAmount = 0.0f;

		bool mInitialized = false;
	};
}

