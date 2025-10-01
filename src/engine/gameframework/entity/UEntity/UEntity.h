#pragma once
#include <engine/gameframework/entity/base/BaseEntity.h>

namespace Unnamed {
	class UEntity : public BaseEntity {
	public:
		using BaseEntity::BaseEntity;
		~UEntity() override;

		void OnRegister() override;
		void PostRegister() override;

		void PrePhysicsTick(float deltaTime) override;
		void Tick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		void OnPreRender() const override;
		void OnRender() const override;
		void OnPostRender() const override;

		void OnDestroy() override;

	private:
	};
}
