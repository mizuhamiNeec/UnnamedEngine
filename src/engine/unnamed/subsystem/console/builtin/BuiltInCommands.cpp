#include "BuiltInCommands.h"

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/ConCommand.h>
#include <engine/unnamed/subsystem/console/concommand/ConVar.h>
#include <engine/game/IDemoService.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

#include "core/filesystem/Path.h"

#include "engine/unnamed/subsystem/console/ConsoleScriptParser.h"

namespace Unnamed {
	void RegisterBuiltInCommands() {
		// 数値のトグル処理（値リストから現在値を探し、次の値へ）
		auto ToggleSequence = [](
			auto*                           var,
			const std::vector<std::string>& values,
			auto                            parse
		) {
			if (!var || values.empty()) {
				return;
			}
			using t         = decltype(var->GetValue());
			const t current = static_cast<t>(*var);

			for (size_t i = 0; i < values.size(); ++i) {
				if (current == parse(values[i])) {
					const size_t nextIndex = (i + 1) % values.size();
					var->SetValue(parse(values[nextIndex]));
					Msg(kChannelNone, "Toggle: {}", var->GetValue());
					return;
				}
			}

			// 見つからなければ最初の値へ
			var->SetValue(parse(values[0]));
			Msg(kChannelNone, "Toggle: {}", var->GetValue());
		};

		static ConCommand toggle(
			"toggle",
			[ToggleSequence](std::vector<std::string> args) {
				// 引数がなければ何もせず終了。
				if (args.empty()) {
					return false;
				}

				auto* console = ServiceLocator::Get<ConsoleSystem>();
				// 最初の引数がCVar名、その後が切り替えたい値のリスト
				const std::vector argValues(
					args.begin() + 1, args.end()
				);
				const auto argSize = argValues.size();            // 値の数
				const auto var     = console->GetConVar(args[0]); // CVarを取得

				switch (GetConVarType(var)) {
					case CVAR_TYPE::NONE: break;

					case CVAR_TYPE::BOOL: {
						const auto bVar = dynamic_cast<ConVar<bool>*>(
							var);
						const bool bValue = static_cast<bool>(*bVar);
						bVar->SetValue(!bValue);
						Msg(kChannelNone, "Toggle: {}", bVar->GetValue());
					}
					break;

					case CVAR_TYPE::INT: {
						if (argSize == 0) {
							return false;
						}
						auto* iVar = dynamic_cast<ConVar<int>*>(var);
						ToggleSequence(
							iVar, argValues, [](const std::string& s) {
								return std::stoi(s);
							}
						);
					}
					break;

					case CVAR_TYPE::FLOAT: {
						if (argSize == 0) {
							return false;
						}
						auto* fVar = dynamic_cast<ConVar<float>*>(var);
						ToggleSequence(
							fVar, argValues, [](const std::string& s) {
								return std::stof(s);
							}
						);
					}
					break;

					case CVAR_TYPE::DOUBLE: {
						if (argSize == 0) {
							return false;
						}
						auto* dVar = dynamic_cast<ConVar<double>*>(var);
						ToggleSequence(
							dVar, argValues, [](const std::string& s) {
								return std::stod(s);
							}
						);
					}
					break;

					case CVAR_TYPE::STRING: {
						if (argSize == 0) {
							return false;
						}
						auto* sVar = dynamic_cast<ConVar<std::string>*>(
							var);
						ToggleSequence(
							sVar, argValues, [](const std::string& s) {
								return s;
							}
						);
					}
					break;
					case CVAR_TYPE::VEC3:
						// 使う...か?
						Error(
							"Toggle",
							"Vec3 type is not supported for toggle command."
						);
						break;
				}
				return true;
			},
			"Usage: toggle <cvar> <value 1> <value 2> <value 3> ..."
		);

		static ConCommand exec(
			"exec",
			[](const std::vector<std::string>& args) {
				if (args.empty()) {
					Error(kChannelNone, "Usage: exec <script file path>");
					return false;
				}

				std::string scriptPath = args[0];
				for (size_t i = 1; i < args.size(); ++i) {
					scriptPath += " ";
					scriptPath += args[i];
				}

				// スクリプトファイルを実行する
				ConsoleScriptParser parser;
				parser.ParseAndExecute(Path(scriptPath));

				return true;
			},
			"Execute a console script file."
		);

		static ConCommand demoRecord(
			"demo_record",
			[](const std::vector<std::string>& args) {
				auto* demoService = ServiceLocator::Get<IDemoService>();
				if (!demoService) {
					Error(kChannelNone, "DemoService is not available.");
					return false;
				}
				const std::string path = args.empty() ? std::string() : args[0];
				return demoService->StartRecording(Path(path));
			},
			"Start demo recording. Usage: demo_record [path]"
		);

		static ConCommand demoPlay(
			"demo_play",
			[](const std::vector<std::string>& args) {
				auto* demoService = ServiceLocator::Get<IDemoService>();
				if (!demoService) {
					Error(kChannelNone, "DemoService is not available.");
					return false;
				}
				if (args.empty()) {
					Error(kChannelNone, "Usage: demo_play <path>");
					return false;
				}
				return demoService->StartPlayback(Path(args[0]));
			},
			"Start demo playback. Usage: demo_play <path>"
		);

		static ConCommand demoStop(
			"demo_stop",
			[](const std::vector<std::string>&) {
				auto* demoService = ServiceLocator::Get<IDemoService>();
				if (!demoService) {
					Error(kChannelNone, "DemoService is not available.");
					return false;
				}
				return demoService->Stop();
			},
			"Stop demo recording/playback."
		);

		static ConCommand demoStatus(
			"demo_status",
			[](const std::vector<std::string>&) {
				const auto* demoService = ServiceLocator::Get<IDemoService>();
				if (!demoService) {
					Error(kChannelNone, "DemoService is not available.");
					return false;
				}
				Msg(kChannelNone, "{}", demoService->BuildStatusString());
				return true;
			},
			"Show demo manager status."
		);

		ServerCommand();
	}

	void ServerCommand() {
	}
}
