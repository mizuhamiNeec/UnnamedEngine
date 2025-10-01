#include <engine/gameframework/entity/base/BaseEntity.h>

namespace Unnamed {
	BaseEntity::BaseEntity(std::string name) : mName(std::move(name)) {
	}

	BaseEntity::~BaseEntity() = default;

	void BaseEntity::OnEditorTick(float) {
	}

	void BaseEntity::OnEditorRender() const {
	}

	std::string_view BaseEntity::GetName() const {
		return mName;
	}

	void BaseEntity::SetName(std::string& name) {
		mName = std::move(name);
	}

	bool BaseEntity::IsEditorOnly() const noexcept {
		return mIsEditorOnly;
	}

	uint64_t BaseEntity::GetId() const noexcept {
		return mId;
	}
}
