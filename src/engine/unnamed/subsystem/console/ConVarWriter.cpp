#include "ConVarWriter.h"

#include <fstream>

#include <core/path/PathUtil.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/ConVar.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	ConVarWriter::ConVarWriter(const std::string_view path) {
		std::ofstream ofs(Path::FromUtf8(path), std::ios::binary);

		if (!ofs) {
			Warning(
				"ConVarWriter",
				"Failed to open file for writing: {}",
				path
			);
			return;
		}

		const auto console = ServiceLocator::Get<ConsoleSystem>();
		if (!console) {
			Warning(
				"ConVarWriter",
				"ConsoleSystem is unavailable while writing archived convars."
			);
			return;
		}

		const auto vars    = console->GetConVars();

		for (const auto& var : vars) {
			if (!var.second) {
				continue;
			}

			if (var.second->HasFlags(FCVAR::ARCHIVE)) {
				switch (GetConVarType(var.second)) {
					case CVAR_TYPE::NONE: break;
					case CVAR_TYPE::BOOL: {
						const auto* bVar = dynamic_cast<ConVar<bool>*>(
							var.
							second);
						if (!bVar) {
							break;
						}
						std::string valueStr = static_cast<bool>(*bVar) ?
							                       "true" :
							                       "false";
						ofs << var.first << " " << valueStr << "\n";
					}
					break;

					case CVAR_TYPE::INT: {
						const auto* iVar = dynamic_cast<ConVar<int>*>(var
							.
							second);
						if (!iVar) {
							break;
						}
						ofs << var.first << " " << static_cast<int>(*iVar) <<
							"\n";
					}
					break;

					case CVAR_TYPE::FLOAT: {
						const auto* fVar = dynamic_cast<ConVar<float>*>(
							var.
							second);
						if (!fVar) {
							break;
						}
						ofs << var.first << " " << static_cast<float>(*fVar) <<
							"\n";
					}
					break;

					case CVAR_TYPE::DOUBLE: {
						const auto* dVar = dynamic_cast<ConVar<double>*>(
							var.
							second);
						if (!dVar) {
							break;
						}
						ofs << var.first << " " << static_cast<double>(*dVar) <<
							"\n";
					}
					break;

					case CVAR_TYPE::STRING: {
						const auto* sVar = dynamic_cast<ConVar<
							std::string>*>(
							var.
							second);
						if (!sVar) {
							break;
						}
						ofs << var.first << " " << static_cast<std::string>(*
								sVar)
							<< "\n";
					}
					break;
					case CVAR_TYPE::VEC3:
						// 使う...か?
						break;
				}
			}
		}

		ofs.flush();
	}
}
