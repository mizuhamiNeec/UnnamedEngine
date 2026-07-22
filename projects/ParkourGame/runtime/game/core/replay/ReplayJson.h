#pragma once

#include <json.hpp>

#include "core/math/Vec3.h"

namespace Unnamed::ReplayJson {
	/// @brief オブジェクト内の3要素配列をVec3として読み取ります。
	/// @param object 読み取り元JSONオブジェクト
	/// @param key 読み取るメンバ名
	/// @param outValue 読み取り成功時の出力先
	/// @return 対象が3要素配列の場合はtrue
	/// @note 要素型がfloatへ変換できない場合はnlohmann::jsonの例外を伝播します。
	[[nodiscard]] inline bool TryReadVec3(
		const nlohmann::json& object,
		const char*           key,
		Vec3&                 outValue
	) {
		if (!object.is_object()) {
			return false;
		}
		const auto it = object.find(key);
		if (it == object.end() || !it->is_array() || it->size() != 3) {
			return false;
		}
		outValue = Vec3(
			(*it)[0].get<float>(),
			(*it)[1].get<float>(),
			(*it)[2].get<float>()
		);
		return true;
	}
}
