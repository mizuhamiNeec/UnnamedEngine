#include "Logger.h"

void Logger::Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}
