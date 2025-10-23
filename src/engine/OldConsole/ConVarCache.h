#pragma once
#include <string>
#include <unordered_map>

#include "IConVar.h"

/// @brief コンソール変数キャッシュクラス
class ConVarCache {
public:
	/// @brief シングルトンインスタンスを取得します
	static ConVarCache& GetInstance() {
		static ConVarCache instance;
		return instance;
	}

	/// @brief コンソール変数をキャッシュに保存します
	/// @param name コンソール変数の名前
	/// @param conVar コンソール変数のポインタ
	void CacheConVar(const std::string& name, IConVar* conVar) {
		cachedConVars_[name] = conVar;
	}

	/// @brief キャッシュからコンソール変数を取得します
	/// @param name コンソール変数の名前
	/// @return コンソール変数のポインタ、存在しない場合はnullptr
	IConVar* GetCachedConVar(const std::string& name) {
		auto it = cachedConVars_.find(name);
		return it != cachedConVars_.end() ? it->second : nullptr;
	}

private:
	std::unordered_map<std::string, IConVar*> cachedConVars_;
};
