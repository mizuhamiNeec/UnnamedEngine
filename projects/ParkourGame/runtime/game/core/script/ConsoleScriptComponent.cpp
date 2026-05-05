#include "ConsoleScriptComponent.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "ConsoleScriptComponent";

		constexpr const char* kAttachCommandsKey = "attachCommands";
		constexpr const char* kDetachCommandsKey = "detachCommands";

#ifdef _DEBUG
		template <size_t N>
		bool EditCommandInput(const char* label, std::string& value) {
			std::array<char, N> buffer = {};
			const size_t        copyLen = std::min(value.size(), buffer.size() - 1);
			if (copyLen > 0) {
				std::memcpy(buffer.data(), value.data(), copyLen);
			}
			if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
				return false;
			}
			value = buffer.data();
			return true;
		}

		void DrawCommandListInspector(
			const char*               label,
			std::vector<std::string>& commands
		) {
			ImGui::PushID(label);
			ImGui::SeparatorText(label);

			if (ImGui::Button("Add Command")) {
				commands.emplace_back();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Commands")) {
				commands.clear();
			}

			for (size_t i = 0; i < commands.size(); ++i) {
				ImGui::PushID(static_cast<int>(i));
				std::string& command = commands[i];
				EditCommandInput<512>("##Command", command);
				ImGui::SameLine();
				if (ImGui::Button("Remove")) {
					commands.erase(commands.begin() + static_cast<ptrdiff_t>(i));
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
			ImGui::PopID();
		}
#endif

		void ReadCommandArray(
			const JsonReader&          node,
			std::vector<std::string>& outCommands
		) {
			outCommands.clear();
			if (!node.Valid() || !node.IsArray()) {
				return;
			}

			const JsonReader commandArray = node.GetArray();
			for (size_t i = 0; i < commandArray.Size(); ++i) {
				const JsonReader commandNode = commandArray[i];
				if (!commandNode.Valid()) {
					continue;
				}
				std::string command = StrUtil::TrimSpaces(commandNode.GetString(""));
				if (command.empty()) {
					continue;
				}
				outCommands.emplace_back(std::move(command));
			}
		}
	}

	void ConsoleScriptComponent::OnAttached() {
		BaseComponent::OnAttached();
		ExecutePhaseCommands(mOnAttachCommands, "Attach");
	}

	void ConsoleScriptComponent::OnDetached() {
		BaseComponent::OnDetached();
		ExecutePhaseCommands(mOnDetachCommands, "Detach");
	}

	std::string_view ConsoleScriptComponent::GetStableName() const {
		return "engine.ScriptComponent";
	}

	std::string_view ConsoleScriptComponent::GetComponentName() const {
		return "ScriptComponent";
	}

	uint32_t ConsoleScriptComponent::GetIcon() const {
		return kIconArticle;
	}

#ifdef _DEBUG
	void ConsoleScriptComponent::DrawInspectorImGui() {
		DrawCommandListInspector("On Attach", mOnAttachCommands);
		DrawCommandListInspector("On Detach", mOnDetachCommands);
	}
#endif

	void ConsoleScriptComponent::Deserialize(const JsonReader& reader) {
		ReadCommandArray(reader[kAttachCommandsKey], mOnAttachCommands);
		ReadCommandArray(reader[kDetachCommandsKey], mOnDetachCommands);
	}

	void ConsoleScriptComponent::Serialize(JsonWriter& writer) const {
		writer.Key(kAttachCommandsKey);
		writer.BeginArray();
		for (const std::string& command : mOnAttachCommands) {
			const std::string trimmed = StrUtil::TrimSpaces(command);
			if (trimmed.empty()) {
				continue;
			}
			writer.Write(trimmed);
		}
		writer.EndArray();

		writer.Key(kDetachCommandsKey);
		writer.BeginArray();
		for (const std::string& command : mOnDetachCommands) {
			const std::string trimmed = StrUtil::TrimSpaces(command);
			if (trimmed.empty()) {
				continue;
			}
			writer.Write(trimmed);
		}
		writer.EndArray();
	}

	void ConsoleScriptComponent::ExecutePhaseCommands(
		const std::vector<std::string>& commands,
		const char*                     phaseName
	) const {
		ConsoleSystem* console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			Warning(
				kChannel,
				"{} command execution skipped: ConsoleSystem is not available.",
				phaseName
			);
			return;
		}

		for (const std::string& rawCommand : commands) {
			const std::string command = StrUtil::TrimSpaces(rawCommand);
			if (command.empty()) {
				continue;
			}

			// 実行順を維持し、フェーズごとのトレースを残す。
			DevMsg(kChannel, "{} execute: {}", phaseName, command);
			console->ExecuteCommand(command, EXEC_FLAG::FROM_ENGINE);
		}
	}

	REGISTER_COMPONENT(ConsoleScriptComponent);
}
