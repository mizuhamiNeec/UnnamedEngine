#pragma once
#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Math/Vector/Vec3.h"
#include "Base/Component.h"

namespace Components {
	class Transform : public Component {
	public:
		[[nodiscard]] const Vec3& GetPosition() const;
		void SetPosition(const Vec3& position);

		[[nodiscard]] const Quaternion& GetRotation() const;
		void SetRotation(const Quaternion& rotation);

		[[nodiscard]] const Vec3& GetScale() const;
		void SetScale(const Vec3& scale);

		[[nodiscard]] Mat4 GetTransformMatrix() const;

		//void DrawImGui() override;

	private:
		Vec3 position_;
		Quaternion rotation_;
		Vec3 scale_;
	};
} // namespace Components
