#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Vec3.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class TransformComponent;

	class PatrolPointComponent final : public BaseComponent {
	public:
		void                     OnAttached() override;
		void                     OnTick(float deltaTime) override;
		[[nodiscard]] TICK_GROUP GetTickGroup() const override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const override;

	private:
		[[nodiscard]] TransformComponent* GetTransform() const;
		void                              ApplyStartPoint();

		Vec3  mPointA = Vec3::zero;
		Vec3  mPointB = Vec3(0.0f, 0.0f, 200.0f);
		float mSpeed  = 100.0f;

		bool mEnabled      = true;
		bool mStartFromA   = true;
		bool mMoveToPointB = true;
		bool mSnapOnAttach = true;
	};
}
