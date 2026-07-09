#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	/// @brief Transformの退避値です。
	struct SequenceTransformSnapshot final {
		Vec3       position = Vec3::zero;
		Quaternion rotation = Quaternion::identity;
		Vec3       scale    = Vec3::one;
	};

	/// @brief カメラ選択状態の退避値です。
	struct SequenceCameraSnapshot final {
		bool     hasCurrent        = false;
		uint64_t currentCameraGuid = 0;
	};

	/// @brief Sequence適用前の値を保持するストアです。
	class PreAnimatedStateStore final {
	public:
		/// @brief Floatを初回のみ保存します。
		void SaveFloatIfMissing(const std::string& key, float value);

		/// @brief Boolを初回のみ保存します。
		void SaveBoolIfMissing(const std::string& key, bool value);

		/// @brief Vec3を初回のみ保存します。
		void SaveVec3IfMissing(const std::string& key, const Vec3& value);

		/// @brief Transformを初回のみ保存します。
		void SaveTransformIfMissing(
			uint64_t entityGuid, const SequenceTransformSnapshot& snapshot
		);

		/// @brief Entityのactive状態を初回のみ保存します。
		void SaveEntityActiveIfMissing(uint64_t entityGuid, bool active);

		/// @brief Entityのvisible状態を初回のみ保存します。
		void SaveEntityVisibleIfMissing(uint64_t entityGuid, bool visible);

		/// @brief Camera選択状態を初回のみ保存します。
		void SaveCameraIfMissing(const SequenceCameraSnapshot& snapshot);

		/// @brief 保存済みFloatを取得します。
		[[nodiscard]] bool TryGetFloat(
			const std::string& key, float& outValue
		) const;

		/// @brief 保存済みBoolを取得します。
		[[nodiscard]] bool TryGetBool(
			const std::string& key, bool& outValue
		) const;

		/// @brief 保存済みVec3を取得します。
		[[nodiscard]] bool TryGetVec3(
			const std::string& key, Vec3& outValue
		) const;

		/// @brief 保存済みTransformを取得します。
		[[nodiscard]] bool TryGetTransform(
			uint64_t entityGuid, SequenceTransformSnapshot& outValue
		) const;

		/// @brief 保存済みEntity activeを取得します。
		[[nodiscard]] bool TryGetEntityActive(
			uint64_t entityGuid, bool& outValue
		) const;

		/// @brief 保存済みEntity visibleを取得します。
		[[nodiscard]] bool TryGetEntityVisible(
			uint64_t entityGuid, bool& outValue
		) const;

		/// @brief 保存済みCamera状態を取得します。
		[[nodiscard]] bool TryGetCamera(SequenceCameraSnapshot& outValue) const;

		/// @brief 保存済みFloatを削除します。
		void RemoveFloat(const std::string& key);

		/// @brief 保存済みBoolを削除します。
		void RemoveBool(const std::string& key);

		/// @brief 保存済みVec3を削除します。
		void RemoveVec3(const std::string& key);

		/// @brief 保存済みTransformを削除します。
		void RemoveTransform(uint64_t entityGuid);

		/// @brief 保存済みEntity activeを削除します。
		void RemoveEntityActive(uint64_t entityGuid);

		/// @brief 保存済みEntity visibleを削除します。
		void RemoveEntityVisible(uint64_t entityGuid);

		/// @brief 保存済みCamera状態を削除します。
		void RemoveCamera();

		/// @brief すべての保存値をクリアします。
		void Clear();

		/// @brief 退避済みFloatキーを取得します。
		[[nodiscard]] const std::unordered_map<std::string, float>&
		GetSavedFloats() const;

		/// @brief 退避済みBoolキーを取得します。
		[[nodiscard]] const std::unordered_map<std::string, bool>&
		GetSavedBools() const;

		/// @brief 退避済みVec3キーを取得します。
		[[nodiscard]] const std::unordered_map<std::string, Vec3>&
		GetSavedVec3() const;

		/// @brief 退避済みTransformキーを取得します。
		[[nodiscard]] const std::unordered_map<
			uint64_t, SequenceTransformSnapshot>& GetSavedTransforms() const;

		/// @brief 退避済みEntity activeキーを取得します。
		[[nodiscard]] const std::unordered_map<uint64_t, bool>&
		GetSavedEntityActive() const;

		/// @brief 退避済みEntity visibleキーを取得します。
		[[nodiscard]] const std::unordered_map<uint64_t, bool>&
		GetSavedEntityVisible() const;

		/// @brief Camera退避値が存在するかを返します。
		[[nodiscard]] bool HasSavedCamera() const;

	private:
		std::unordered_map<std::string, float> mSavedFloatValues = {};
		std::unordered_map<std::string, bool> mSavedBoolValues = {};
		std::unordered_map<std::string, Vec3> mSavedVec3Values = {};
		std::unordered_map<uint64_t, SequenceTransformSnapshot> mSavedTransforms
			= {};
		std::unordered_map<uint64_t, bool> mSavedEntityActive  = {};
		std::unordered_map<uint64_t, bool> mSavedEntityVisible = {};
		bool                               mHasSavedCamera     = false;
		SequenceCameraSnapshot             mSavedCameraState   = {};
	};
}
