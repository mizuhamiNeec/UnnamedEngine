#include <engine/public/gameframework/entity/base/BaseEntity.h>

namespace Unnamed {
	BaseEntity::BaseEntity(std::string name): mName(std::move(name)) {
	}

	void BaseEntity::OnEditorTick(float) {
	}

	void BaseEntity::OnEditorRender() const {
	}

	std::string_view BaseEntity::GetName() const {
		return mName;
	}
}
