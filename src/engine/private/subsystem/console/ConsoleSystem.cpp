#include <pch.h>

#include <iostream>

#include <engine/public/subsystem/console/ConsoleSystem.h>

#include <engine/public/subsystem/interface/ServiceLocator.h>

ConsoleSystem::~ConsoleSystem() = default;

bool ConsoleSystem::Init() {
	ServiceLocator::Register<IConsole>(this);

	OutputDebugStringW(L"ConsoleSystem Initialized\n");
	return true;
}

void ConsoleSystem::Update(float deltaTime) {
	deltaTime;
}

void ConsoleSystem::Shutdown() {
}

const std::string_view ConsoleSystem::GetName() const {
	return "Console";
}

void ConsoleSystem::Msg(const std::string_view message) {
	OutputDebugStringA(message.data());
	OutputDebugStringA("\n");
}
