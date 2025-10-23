#pragma once
#include <cstdint>


/// @brief メモリユーティリティクラス
/// @details メモリアライメントなどのメモリ操作用ヘルパー関数を提供します 
class MemUtil {
public:
	/// @brief 値を指定されたアライメントに切り上げる
	/// @param value 切り上げる値
	/// @param alignment アライメント境界
	/// @return アライメントに切り上げられた値
	static uint64_t AlignUp(uint64_t value, uint64_t alignment);
};
