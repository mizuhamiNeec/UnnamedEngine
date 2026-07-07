#include "ShaderIncludeParser.h"

#include <cctype>
#include <string>
#include <utility>

namespace Unnamed {
	std::vector<ShaderIncludeReference> ShaderIncludeParser::Parse(
		const std::string_view source
	) {
		std::string logicalSource;
		logicalSource.reserve(source.size());

		bool inBlockComment = false;
		bool inLineComment  = false;
		bool inString       = false;
		bool escaped        = false;
		char stringDelimiter = '\0';

		for (size_t i = 0; i < source.size(); ++i) {
			const char current = source[i];
			const char next = i + 1 < source.size() ? source[i + 1] : '\0';

			if (current == '\\' && (next == '\n' || next == '\r')) {
				++i;
				if (next == '\r' && i + 1 < source.size() && source[i + 1] == '\n') {
					++i;
				}
				continue;
			}

			if (inLineComment) {
				if (current == '\n') {
					inLineComment = false;
					logicalSource.push_back('\n');
				} else {
					logicalSource.push_back(' ');
				}
				continue;
			}

			if (inBlockComment) {
				if (current == '*' && next == '/') {
					logicalSource.append("  ");
					++i;
					inBlockComment = false;
				} else {
					logicalSource.push_back(current == '\n' ? '\n' : ' ');
				}
				continue;
			}

			if (inString) {
				logicalSource.push_back(current);
				if (escaped) {
					escaped = false;
				} else if (current == '\\') {
					escaped = true;
				} else if (current == stringDelimiter) {
					inString = false;
				}
				continue;
			}

			if (current == '/' && next == '/') {
				logicalSource.append("  ");
				++i;
				inLineComment = true;
				continue;
			}
			if (current == '/' && next == '*') {
				logicalSource.append("  ");
				++i;
				inBlockComment = true;
				continue;
			}
			if (current == '"' || current == '\'') {
				inString       = true;
				stringDelimiter = current;
			}
			logicalSource.push_back(current);
		}

		std::vector<ShaderIncludeReference> references;
		size_t lineBegin = 0;
		while (lineBegin < logicalSource.size()) {
			const size_t lineEnd = logicalSource.find('\n', lineBegin);
			const size_t lineLength = lineEnd == std::string::npos ?
				logicalSource.size() - lineBegin : lineEnd - lineBegin;
			const std::string_view line(
				logicalSource.data() + lineBegin, lineLength
			);

			if (std::optional<ShaderIncludeReference> reference = ParseLine(line)) {
				references.emplace_back(std::move(*reference));
			}

			if (lineEnd == std::string::npos) {
				break;
			}
			lineBegin = lineEnd + 1;
		}

		return references;
	}

	std::optional<ShaderIncludeReference> ShaderIncludeParser::ParseLine(
		const std::string_view line
	) {
		size_t cursor = line.starts_with("\xEF\xBB\xBF") ? 3 : 0;
		while (cursor < line.size() &&
		       std::isspace(static_cast<unsigned char>(line[cursor]))) {
			++cursor;
		}
		if (cursor >= line.size() || line[cursor] != '#') {
			return std::nullopt;
		}
		++cursor;
		while (cursor < line.size() &&
		       std::isspace(static_cast<unsigned char>(line[cursor]))) {
			++cursor;
		}
		if (line.substr(cursor, 7) != "include") {
			return std::nullopt;
		}
		cursor += 7;
		if (cursor < line.size() &&
		    (std::isalnum(static_cast<unsigned char>(line[cursor])) ||
		     line[cursor] == '_')) {
			return std::nullopt;
		}
		while (cursor < line.size() &&
		       std::isspace(static_cast<unsigned char>(line[cursor]))) {
			++cursor;
		}
		if (cursor >= line.size()) {
			return std::nullopt;
		}

		const char open  = line[cursor];
		const char close = open == '"' ? '"' : open == '<' ? '>' : '\0';
		if (close == '\0') {
			return std::nullopt;
		}
		const size_t closePosition = line.find(close, cursor + 1);
		if (closePosition == std::string_view::npos ||
		    closePosition == cursor + 1) {
			return std::nullopt;
		}

		return ShaderIncludeReference{
			.kind = open == '"' ? ShaderIncludeKind::SourceRelative :
				ShaderIncludeKind::MountRootRelative,
			.path = std::string(line.substr(
				cursor + 1, closePosition - cursor - 1
			)),
		};
	}
}
