#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	/// @brief GameplayCue の型付き named payload コンテナです。
	struct GameplayCuePayloadBag final {
		/// @brief bool payload を設定します。
		void SetBool(std::string_view name, bool value);

		/// @brief float payload を設定します。
		void SetFloat(std::string_view name, float value);

		/// @brief Vec2 payload を設定します。
		void SetVec2(std::string_view name, const Vec2& value);

		/// @brief Vec3 payload を設定します。
		void SetVec3(std::string_view name, const Vec3& value);

		/// @brief EntityId(uint64_t) payload を設定します。
		void SetEntityId(std::string_view name, uint64_t value);

		/// @brief bool payload を取得します。
		[[nodiscard]] bool TryGetBool(
			std::string_view name, bool& outValue
		) const;

		/// @brief float payload を取得します。
		[[nodiscard]] bool TryGetFloat(
			std::string_view name,
			float&           outValue
		) const;

		/// @brief Vec2 payload を取得します。
		[[nodiscard]] bool TryGetVec2(
			std::string_view name,
			Vec2&            outValue
		) const;

		/// @brief Vec3 payload を取得します。
		[[nodiscard]] bool TryGetVec3(
			std::string_view name,
			Vec3&            outValue
		) const;

		/// @brief EntityId(uint64_t) payload を取得します。
		[[nodiscard]] bool TryGetEntityId(
			std::string_view name,
			uint64_t&        outValue
		) const;

		/// @brief payload をクリアします。
		void Clear();

	private:
		template <class TValue>
		bool TryGetValue(
			const std::unordered_map<std::string, TValue>& map,
			const std::string_view name, TValue& outValue
		) const {
			const std::string key = NormalizeKey(name);
			if (key.empty()) {
				return false;
			}
			const auto found = map.find(key);
			if (found == map.end()) {
				return false;
			}
			outValue = found->second;
			return true;
		}

		[[nodiscard]] static std::string NormalizeKey(
			std::string_view name
		);

		std::unordered_map<std::string, bool>     mBoolValues;
		std::unordered_map<std::string, float>    mFloatValues;
		std::unordered_map<std::string, Vec2>     mVec2Values;
		std::unordered_map<std::string, Vec3>     mVec3Values;
		std::unordered_map<std::string, uint64_t> mEntityIdValues;
	};

	/// @brief GameplayCueは、cue tag、送信元、payloadを1件のgameplay通知として保持します
	struct GameplayCue {
		std::string           id;
		uint64_t              sourceEntityGuid = 0;
		float                 value            = 0.0f;
		float                 value2           = 0.0f;
		GameplayCuePayloadBag payload          = {};

		/// @brief bool payload を設定します。
		void SetBool(std::string_view name, bool payloadValue);

		/// @brief float payload を設定します。
		void SetFloat(std::string_view name, float payloadValue);

		/// @brief Vec2 payload を設定します。
		void SetVec2(std::string_view name, const Vec2& payloadValue);

		/// @brief Vec3 payload を設定します。
		void SetVec3(std::string_view name, const Vec3& payloadValue);

		/// @brief EntityId(uint64_t) payload を設定します。
		void SetEntityId(
			std::string_view name, uint64_t payloadValue
		);

		/// @brief bool payload を取得します。
		[[nodiscard]] bool TryGetBool(
			std::string_view name,
			bool&            outValue
		) const;

		/// @brief float payload を取得します。
		[[nodiscard]] bool TryGetFloat(
			std::string_view name,
			float&           outValue
		) const;

		/// @brief Vec2 payload を取得します。
		[[nodiscard]] bool TryGetVec2(
			std::string_view name,
			Vec2&            outValue
		) const;

		/// @brief Vec3 payload を取得します。
		[[nodiscard]] bool TryGetVec3(
			std::string_view name,
			Vec3&            outValue
		) const;

		/// @brief EntityId(uint64_t) payload を取得します。
		[[nodiscard]] bool TryGetEntityId(
			std::string_view name,
			uint64_t&        outValue
		) const;
	};

	/// @brief GameplayCueFilterは、ワールドイベントまたは要素を通過させる条件を保持します
	struct GameplayCueFilter {
		std::string cueId;
		uint64_t    sourceEntityGuid = 0;
	};

	/// @brief GameplayCueBusは、GameplayCueをフィルター条件に一致するlistenerへ同期配送します
	class GameplayCueBus final {
	public:
		using Handle   = uint64_t;
		using Callback = std::function<void(const GameplayCue&)>;

		/// @brief 指定したフィルタ条件で GameplayCue を購読します。
		/// @param filter   購読する GameplayCue の条件
		/// @param callback 条件に合致する GameplayCue が発行されたときに呼び出されるコールバック関数
		/// @return 購読のハンドル。購読解除の際に使用します。無効な購読ルールやコールバックが指定された場合は0を返します。
		[[nodiscard]] Handle Subscribe(
			const GameplayCueFilter& filter, Callback callback
		);

		/// @brief 指定したハンドルの購読を解除します。
		/// @param handle 購読のハンドル
		/// @return 購読が正常に解除された場合はtrue。無効なハンドルが指定された場合や、すでに解除されている場合はfalse。
		bool Unsubscribe(Handle handle);

		/// @brief 条件に合致する GameplayCue を発行します。
		/// @param cue 発行する GameplayCue。id と sourceEntityGuid は必須で、空のidや0のsourceEntityGuidを持つcueは無視されます。
		void Publish(const GameplayCue& cue);

		/// @brief 登録された購読ルールとコールバックをすべてクリアします。
		void Clear();

	private:
		/// @brief Listenerは、GameplayCue listener ID、filter、callbackとowner lifetime tokenを保持します
		struct Listener {
			Handle            handle   = 0;
			GameplayCueFilter filter   = {};
			Callback          callback = nullptr;
			bool              active   = false;
		};

		/// @brief 指定したフィルタ条件が GameplayCue と合致するかを判定します。
		/// @param filter 購読ルールのフィルタ条件
		/// @param cue    発行された GameplayCue
		/// @return 条件が合致する場合はtrue。cueId と sourceEntityGuid の両方がフィルタ条件と完全一致する必要があります。
		[[nodiscard]] static bool Matches(
			const GameplayCueFilter& filter, const GameplayCue& cue
		);

		/// @brief 無効な購読をリストから削除します。
		void PruneInactiveListeners();

		std::vector<Listener> mListeners;
		Handle                mNextHandle   = 1;
		bool                  mIsPublishing = false;
	};
}
