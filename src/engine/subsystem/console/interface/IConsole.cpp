#include <pch.h>

#include <engine/subsystem/console/interface/IConsole.h>

namespace Unnamed {
	const char* ToString(const LogLevel e) {
		switch (e) {
		case LogLevel::None: return "None";
		case LogLevel::Info: return "Info";
		case LogLevel::Dev: return "Dev";
		case LogLevel::Warning: return "Warning";
		case LogLevel::Error: return "Error";
		case LogLevel::Fatal: return "Fatal";
		case LogLevel::Execute: return "Execute";
		case LogLevel::Waiting: return "Waiting";
		case LogLevel::Success: return "Success";
		default: return "unknown";
		}
	}
}
