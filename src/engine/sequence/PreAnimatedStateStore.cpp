#include "PreAnimatedStateStore.h"

namespace Unnamed {
	void PreAnimatedStateStore::SaveFloatIfMissing(
		const std::string& key,
		const float        value
	) {
		if (mSavedFloatValues.contains(key)) {
			return;
		}
		mSavedFloatValues.emplace(key, value);
	}

	void PreAnimatedStateStore::SaveBoolIfMissing(
		const std::string& key,
		const bool         value
	) {
		if (mSavedBoolValues.contains(key)) {
			return;
		}
		mSavedBoolValues.emplace(key, value);
	}

	void PreAnimatedStateStore::SaveVec3IfMissing(
		const std::string& key,
		const Vec3&        value
	) {
		if (mSavedVec3Values.contains(key)) {
			return;
		}
		mSavedVec3Values.emplace(key, value);
	}

	void PreAnimatedStateStore::SaveTransformIfMissing(
		const uint64_t                   entityGuid,
		const SequenceTransformSnapshot& snapshot
	) {
		if (mSavedTransforms.contains(entityGuid)) {
			return;
		}
		mSavedTransforms.emplace(entityGuid, snapshot);
	}

	void PreAnimatedStateStore::SaveEntityActiveIfMissing(
		const uint64_t entityGuid,
		const bool     active
	) {
		if (mSavedEntityActive.contains(entityGuid)) {
			return;
		}
		mSavedEntityActive.emplace(entityGuid, active);
	}

	void PreAnimatedStateStore::SaveEntityVisibleIfMissing(
		const uint64_t entityGuid,
		const bool     visible
	) {
		if (mSavedEntityVisible.contains(entityGuid)) {
			return;
		}
		mSavedEntityVisible.emplace(entityGuid, visible);
	}

	void PreAnimatedStateStore::SaveCameraIfMissing(
		const SequenceCameraSnapshot& snapshot
	) {
		if (mHasSavedCamera) {
			return;
		}
		mSavedCameraState = snapshot;
		mHasSavedCamera   = true;
	}

	bool PreAnimatedStateStore::TryGetFloat(
		const std::string& key,
		float&             outValue
	) const {
		const auto it = mSavedFloatValues.find(key);
		if (it == mSavedFloatValues.end()) {
			return false;
		}
		outValue = it->second;
		return true;
	}

	bool PreAnimatedStateStore::TryGetBool(
		const std::string& key,
		bool&              outValue
	) const {
		const auto it = mSavedBoolValues.find(key);
		if (it == mSavedBoolValues.end()) {
			return false;
		}
		outValue = it->second;
		return true;
	}

	bool PreAnimatedStateStore::TryGetVec3(
		const std::string& key,
		Vec3&              outValue
	) const {
		const auto it = mSavedVec3Values.find(key);
		if (it == mSavedVec3Values.end()) {
			return false;
		}
		outValue = it->second;
		return true;
	}

	bool PreAnimatedStateStore::TryGetTransform(
		const uint64_t             entityGuid,
		SequenceTransformSnapshot& outValue
	) const {
		const auto it = mSavedTransforms.find(entityGuid);
		if (it == mSavedTransforms.end()) {
			return false;
		}
		outValue = it->second;
		return true;
	}

	bool PreAnimatedStateStore::TryGetEntityActive(
		const uint64_t entityGuid,
		bool&          outValue
	) const {
		const auto it = mSavedEntityActive.find(entityGuid);
		if (it == mSavedEntityActive.end()) {
			return false;
		}
		outValue = it->second;
		return true;
	}

	bool PreAnimatedStateStore::TryGetEntityVisible(
		const uint64_t entityGuid,
		bool&          outValue
	) const {
		const auto it = mSavedEntityVisible.find(entityGuid);
		if (it == mSavedEntityVisible.end()) {
			return false;
		}
		outValue = it->second;
		return true;
	}

	bool PreAnimatedStateStore::TryGetCamera(
		SequenceCameraSnapshot& outValue
	) const {
		if (!mHasSavedCamera) {
			return false;
		}
		outValue = mSavedCameraState;
		return true;
	}

	void PreAnimatedStateStore::RemoveFloat(const std::string& key) {
		mSavedFloatValues.erase(key);
	}

	void PreAnimatedStateStore::RemoveBool(const std::string& key) {
		mSavedBoolValues.erase(key);
	}

	void PreAnimatedStateStore::RemoveVec3(const std::string& key) {
		mSavedVec3Values.erase(key);
	}

	void PreAnimatedStateStore::RemoveTransform(const uint64_t entityGuid) {
		mSavedTransforms.erase(entityGuid);
	}

	void PreAnimatedStateStore::RemoveEntityActive(const uint64_t entityGuid) {
		mSavedEntityActive.erase(entityGuid);
	}

	void PreAnimatedStateStore::RemoveEntityVisible(const uint64_t entityGuid) {
		mSavedEntityVisible.erase(entityGuid);
	}

	void PreAnimatedStateStore::RemoveCamera() {
		mHasSavedCamera   = false;
		mSavedCameraState = {};
	}

	void PreAnimatedStateStore::Clear() {
		mSavedFloatValues.clear();
		mSavedBoolValues.clear();
		mSavedVec3Values.clear();
		mSavedTransforms.clear();
		mSavedEntityActive.clear();
		mSavedEntityVisible.clear();
		mHasSavedCamera   = false;
		mSavedCameraState = {};
	}

	const std::unordered_map<std::string, float>&
	PreAnimatedStateStore::GetSavedFloats() const {
		return mSavedFloatValues;
	}

	const std::unordered_map<std::string, bool>&
	PreAnimatedStateStore::GetSavedBools() const {
		return mSavedBoolValues;
	}

	const std::unordered_map<std::string, Vec3>&
	PreAnimatedStateStore::GetSavedVec3() const {
		return mSavedVec3Values;
	}

	const std::unordered_map<uint64_t, SequenceTransformSnapshot>&
	PreAnimatedStateStore::GetSavedTransforms() const {
		return mSavedTransforms;
	}

	const std::unordered_map<uint64_t, bool>&
	PreAnimatedStateStore::GetSavedEntityActive() const {
		return mSavedEntityActive;
	}

	const std::unordered_map<uint64_t, bool>&
	PreAnimatedStateStore::GetSavedEntityVisible() const {
		return mSavedEntityVisible;
	}

	bool PreAnimatedStateStore::HasSavedCamera() const {
		return mHasSavedCamera;
	}
}
