#include "ConVar.h"

std::string ToString(const ConVarFlags& e) {
	switch (e) {
	case ConVarFlags::ConVarFlags_None: return "ConVarFlags_None";
	case ConVarFlags::ConVarFlags_Archive: return "ConVarFlags_Archive";
	case ConVarFlags::ConVarFlags_Hidden: return "ConVarFlags_Hidden";
	case ConVarFlags::ConVarFlags_Notify: return "ConVarFlags_Notify";
	default: return "unknown";
	}
}

bool HasFlags(const ConVarFlags& flags, const ConVarFlags& flag) {
	return static_cast<int>(flags) & static_cast<int>(flag);
}

template <typename T>
ConVarFlags ConVar<T>::GetFlags() const { return flags_; }

template <typename T>
T ConVar<T>::GetValue() const { return value_; }

template <typename T>
void ConVar<T>::SetValue(const T& newValue) {
	value_ = newValue;

	if (HasFlags(flags_, ConVarFlags::ConVarFlags_Notify)) {
		// TODO : プレイヤーに通知する
		Console::Print(
			std::format(
				"{}のCVAR 値 '{}'を {} に変更しました\n",
				"クライアント",
				name_,
				value_
			)
		);
	}
}
