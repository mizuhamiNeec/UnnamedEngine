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
