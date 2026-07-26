#!/usr/bin/env python3
"""Run objective repository checks used by the maintenance audit.

This tool is deliberately not a C++ compiler or a design reviewer. It scans
lexical class/struct declarations and verifies only mechanically decidable
policy. Macro-expanded declarations, semantic comment quality, Concepts design,
and project-organization decisions remain manual review items.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import pathlib
import re
import subprocess
import sys
from collections import Counter


CPP_SUFFIXES = {".h", ".hpp", ".hh", ".inl", ".cpp", ".cc", ".cxx"}
EXCLUDED_PREFIXES = {
    "src/thirdparty/": "vendored third-party source",
    "build/": "generated build files",
    "bin/": "build output",
    "artifacts/": "packaged output",
}
REQUIRED_ARTIFACTS = {
    "docs/maintenance/class-comments-project-organization-execplan.md",
    "docs/maintenance/class-comments-project-organization-ledger.json",
    "tools/verify_repository_requirements.py",
    "tools/tests/test_verify_repository_requirements.py",
}
FORBIDDEN_TRACKED_PATTERNS = (
    re.compile(r"^\.vs/"),
    re.compile(r"^bin/"),
    re.compile(r"^build/projects/"),
    re.compile(r"(^|/)imgui\.ini$"),
    re.compile(r"\.vsix$", re.IGNORECASE),
    re.compile(
        r"\.(?:obj|pch|pdb|ipdb|iobj|ilk|idb|tlog|lastbuildstate)$",
        re.IGNORECASE,
    ),
    re.compile(r"\.(?:sln|slnx|vcxproj|vcxproj\.filters|user)$", re.IGNORECASE),
)
REQUIRED_GITIGNORE_RULES = {
    ".vs/",
    "[Bb]in/",
    "*.obj",
    "*.pch",
    "*.pdb",
    "*.user",
    "*.vcxproj",
    "*.vcxproj.filters",
    "*.sln",
    "*.slnx",
    ".idea/",
    "*.DotSettings.user",
    "imgui.ini",
    "*.log",
    "*.tmp",
    "*.cache",
    "*.vsix",
    ".serena/",
    "__pycache__/",
    "*.pyc",
    "/artifacts/",
    "/dist/",
    "/cvarhelper.ini",
}
EPHEMERAL_COUNT_KEYS = {
    "tracked_cpp_files",
    "untracked_cpp_files",
    "untracked_required_artifacts",
}
GENERIC_BRIEF_PATTERNS = (
    re.compile(r"(?:クラス|構造体)です$"),
    re.compile(r"^(?:データ|情報|状態|値)を保持します$"),
    re.compile(r"^(?:処理|機能|管理|制御)を行います$"),
    re.compile(r"^(?:対象|要素|項目)を管理します$"),
)

# These records are review decisions, not automatic proof that the design is good.
CONCEPT_DECISIONS = {
    ("src/core/containers/RingBuffer.h", "RingBuffer"):
        "ADDED: Capacity > 0, default_initializable<T>, and assignable_from<T&, const T&> match std::array storage plus Push/Pop copy assignment.",
    ("src/engine/rhi/UploadBuffer.h", "UploadBuffer"):
        "NOT_ADDED: trivially_copyable is necessary for byte copying but insufficient for GPU/HLSL ABI compatibility; no stable used public contract exists.",
    ("src/engine/tween/TweenInstance.h", "TweenInstance"):
        "NOT_ADDED: TweenLerp is a customization point, while the removed constraint omitted required construction/assignment and could reject future interpolated types.",
    ("src/engine/ui/retained/UiAnimatedValue.h", "UiAnimatedValue"):
        "NOT_ADDED: copyable may be excessive, default construction was omitted, and operator syntax does not express interpolation semantics.",
    ("src/engine/unnamed/subsystem/console/concommand/ConVar.h", "ConVar"):
        "NOT_ADDED: supported types remain distributed across parsing, writing, dynamic casts, and type checks; Vec3 persistence is not a stable unified contract.",
    ("src/engine/Animation/KeyFrame.h", "Keyframe"):
        "NOT_ADDED: storage alone has no additional stable operation contract.",
    ("src/engine/Animation/Node.h", "AnimationCurve"):
        "NOT_ADDED: consumers, rather than storage, impose the relevant operations.",
    ("src/core/filesystem/Path.h", "std::formatter"):
        "EXISTING_SPECIALIZATION: this is not an open primary template contract.",
    ("src/engine/tween/TweenLerp.h", "TweenLerp"):
        "EXISTING_SPECIALIZATION: explicit specializations are the current customization set.",
    ("src/engine/unnamed/subsystem/input/InputSystem.h", "std::hash"):
        "EXISTING_SPECIALIZATION: this is not an open primary template contract.",
}


class RepositoryAuditError(RuntimeError):
    """Expected inventory or input failure suitable for a concise CLI error."""


@dataclasses.dataclass(frozen=True)
class RepositoryFile:
    path: str
    source_control: str


@dataclasses.dataclass(frozen=True)
class Token:
    text: str
    line: int
    offset: int


@dataclasses.dataclass
class TypeRecord:
    file: str
    line: int
    kind: str
    name: str
    qualified_name: str
    declaration: str
    origin: str
    source_control: str
    brief: str
    comment_scope: str
    syntax_status: str
    style_status: str
    semantic_status: str
    style_issues: list[str]
    class_template: bool
    concept_decision: str
    opening_token: int | None = dataclasses.field(default=None, repr=False)
    token_index: int = dataclasses.field(default=0, repr=False)


def run_git(root: pathlib.Path, *arguments: str) -> bytes:
    try:
        result = subprocess.run(
            ["git", "-c", f"safe.directory={root.as_posix()}", *arguments],
            cwd=root,
            check=True,
            capture_output=True,
        )
    except FileNotFoundError as error:
        raise RepositoryAuditError("git executable was not found") from error
    except subprocess.CalledProcessError as error:
        raw_detail = error.stderr or error.stdout or b""
        if isinstance(raw_detail, bytes):
            raw_detail = raw_detail.decode("utf-8", errors="replace")
        detail = raw_detail.strip().splitlines()
        suffix = f": {detail[-1]}" if detail else ""
        raise RepositoryAuditError(
            f"git {' '.join(arguments)} failed with exit code {error.returncode}{suffix}"
        ) from error
    if isinstance(result.stdout, bytes):
        return result.stdout
    return result.stdout.encode("utf-8", errors="surrogateescape")


def run_git_text(root: pathlib.Path, *arguments: str) -> str:
    """Return textual Git output only for commands whose format is line based."""
    return run_git(root, *arguments).decode("utf-8", errors="surrogateescape")


def null_paths(output: bytes) -> set[str]:
    """Decode NUL-delimited repository-relative Git paths without quote parsing."""
    return {
        item.decode("utf-8", errors="surrogateescape")
        for item in output.split(b"\0")
        if item
    }


def repository_files(root: pathlib.Path) -> tuple[list[RepositoryFile], list[str]]:
    tracked = null_paths(run_git(root, "ls-files", "-z"))
    untracked = null_paths(
        run_git(root, "ls-files", "--others", "--exclude-standard", "-z")
    )
    missing_tracked = sorted(path for path in tracked if not (root / path).is_file())
    files = [
        RepositoryFile(path, "tracked")
        for path in sorted(tracked)
        if (root / path).is_file()
    ]
    files.extend(
        RepositoryFile(path, "untracked")
        for path in sorted(untracked - tracked)
        if (root / path).is_file()
    )
    return files, missing_tracked


def changed_type_briefs(
    root: pathlib.Path, files: list[RepositoryFile]
) -> set[tuple[str, str]] | None:
    """Return added/modified brief text; None means an unborn HEAD, so enforce all."""
    try:
        run_git_text(root, "rev-parse", "--verify", "HEAD")
    except RepositoryAuditError:
        return None
    diff = run_git_text(
        root,
        "diff",
        "--unified=0",
        "--no-ext-diff",
        "HEAD",
        "--",
        "*.h",
        "*.hpp",
        "*.hh",
        "*.inl",
        "*.cpp",
        "*.cc",
        "*.cxx",
    )
    changed: set[tuple[str, str]] = set()
    current_path = ""
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current_path = line[6:].replace("\\", "/")
            continue
        if not current_path or not line.startswith("+") or line.startswith("+++"):
            continue
        match = re.search(r"///[ \t]*@brief[ \t]+(.+?)\s*$", line[1:])
        if match:
            changed.add((current_path, match.group(1).strip()))
    for entry in files:
        if entry.source_control != "untracked" or pathlib.PurePosixPath(entry.path).suffix.lower() not in CPP_SUFFIXES:
            continue
        source = (root / entry.path).read_text(encoding="utf-8-sig", errors="replace")
        for match in re.finditer(r"(?m)///[ \t]*@brief[ \t]+(.+?)\s*$", source):
            changed.add((entry.path, match.group(1).strip()))
    return changed


def classify_origin(path: str) -> tuple[str, str]:
    normalized = path.replace("\\", "/")
    for prefix, reason in EXCLUDED_PREFIXES.items():
        if normalized.startswith(prefix):
            return ("external" if prefix == "src/thirdparty/" else "generated", reason)
    return "first-party", ""


def mask_preprocessor(source: str) -> str:
    """Blank directives and definitely inactive #if 0 branches, preserving offsets."""
    chars = list(source)
    stack: list[dict[str, bool]] = []
    offset = 0
    continuation = False
    for line in source.splitlines(keepends=True):
        stripped = line.lstrip()
        is_directive = continuation or stripped.startswith("#")
        if is_directive and not continuation:
            match = re.match(r"#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", stripped)
            if match:
                directive, expression = match.groups()
                expression = re.sub(r"\s+", "", expression.split("//", 1)[0])
                if directive == "if":
                    stack.append({"known_zero": expression in {"0", "(0)"}, "inactive": expression in {"0", "(0)"}})
                elif directive in {"ifdef", "ifndef"}:
                    stack.append({"known_zero": False, "inactive": False})
                elif directive in {"else", "elif"} and stack:
                    frame = stack[-1]
                    if frame["known_zero"]:
                        frame["inactive"] = False
                elif directive == "endif" and stack:
                    stack.pop()

        inactive = any(frame["inactive"] for frame in stack)
        if is_directive or inactive:
            for index in range(offset, offset + len(line)):
                if chars[index] not in "\r\n":
                    chars[index] = " "
        continuation = is_directive and line.rstrip("\r\n").rstrip().endswith("\\")
        offset += len(line)
    return "".join(chars)


def tokenize_cpp(source: str) -> tuple[list[Token], list[str]]:
    masked = mask_preprocessor(source)
    tokens: list[Token] = []
    errors: list[str] = []
    index = 0
    line = 1
    while index < len(masked):
        if masked[index].isspace():
            if masked[index] == "\n":
                line += 1
            index += 1
            continue
        if masked.startswith("//", index):
            end = masked.find("\n", index)
            index = len(masked) if end < 0 else end
            continue
        if masked.startswith("/*", index):
            end = masked.find("*/", index + 2)
            if end < 0:
                errors.append(f"unterminated block comment at line {line}")
                break
            line += masked[index : end + 2].count("\n")
            index = end + 2
            continue
        raw = re.match(r'(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\(', masked[index:])
        if raw:
            terminator = ")" + raw.group(1) + '"'
            end = masked.find(terminator, index + raw.end())
            if end < 0:
                errors.append(f"unterminated raw string at line {line}")
                break
            end += len(terminator)
            line += masked[index:end].count("\n")
            index = end
            continue
        quoted = re.match(r"(?:u8|u|U|L)?['\"]", masked[index:])
        if quoted:
            quote = quoted.group(0)[-1]
            end = index + len(quoted.group(0))
            escaped = False
            while end < len(masked):
                current = masked[end]
                end += 1
                if current == "\n":
                    line += 1
                if escaped:
                    escaped = False
                elif current == "\\":
                    escaped = True
                elif current == quote:
                    break
            index = end
            continue
        token_line = line
        identifier = re.match(r"[A-Za-z_][A-Za-z0-9_]*", masked[index:])
        if identifier:
            text = identifier.group(0)
        else:
            text = next(
                (
                    candidate
                    for candidate in ("::", "[[", "]]", "&&", "||")
                    if masked.startswith(candidate, index)
                ),
                masked[index],
            )
        tokens.append(Token(text, token_line, index))
        index += len(text)
    return tokens, errors


def template_metadata(tokens: list[Token]) -> tuple[set[int], dict[int, int]]:
    parameters: set[int] = set()
    ends: dict[int, int] = {}
    index = 0
    while index + 1 < len(tokens):
        if tokens[index].text != "template" or tokens[index + 1].text != "<":
            index += 1
            continue
        depth = 0
        cursor = index + 1
        while cursor < len(tokens):
            if tokens[cursor].text == "<":
                depth += 1
            elif tokens[cursor].text == ">":
                depth -= 1
                if depth == 0:
                    parameters.update(range(index + 1, cursor))
                    ends[index] = cursor
                    break
            cursor += 1
        index = cursor + 1
    return parameters, ends


def preceding_template(
    tokens: list[Token], type_index: int, template_ends: dict[int, int]
) -> int | None:
    candidates = [
        start
        for start, end in template_ends.items()
        if end < type_index
        and not any(token.text in {";", "{", "}"} for token in tokens[end + 1 : type_index])
    ]
    return max(candidates, default=None)


def looks_like_export_macro(text: str) -> bool:
    return (
        text.isupper()
        or text.endswith("_API")
        or text.endswith("_EXPORT")
        or text.startswith("UNNAMED_")
    )


def scan_type_candidate(
    tokens: list[Token], type_index: int, template_parameters: set[int]
) -> tuple[str, int | None, str] | None:
    if type_index in template_parameters:
        return None
    if type_index > 0 and tokens[type_index - 1].text == "enum":
        return None

    cursor = type_index + 1
    name = ""
    name_complete = False
    square_depth = 0
    paren_depth = 0
    angle_depth = 0
    opening: int | None = None
    while cursor < len(tokens):
        text = tokens[cursor].text
        if text in {"[[", "["}:
            square_depth += 1
        elif text in {"]]", "]"}:
            square_depth = max(0, square_depth - 1)
        elif text == "(":
            paren_depth += 1
        elif text == ")":
            paren_depth = max(0, paren_depth - 1)
        elif text == "<":
            angle_depth += 1
            if name:
                name_complete = True
        elif text == ">":
            angle_depth = max(0, angle_depth - 1)
        elif square_depth == 0 and paren_depth == 0 and angle_depth == 0:
            if text == "{":
                opening = cursor
                break
            if text == ";":
                break
            if text in {":", "final"} and name:
                name_complete = True
            elif re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", text):
                if not name and (
                    text in {"alignas", "__declspec"} or looks_like_export_macro(text)
                ):
                    pass
                elif not name:
                    name = text
                elif (
                    not name_complete
                    and cursor > 0
                    and tokens[cursor - 1].text == "::"
                ):
                    name += "::" + text
        cursor += 1

    if cursor >= len(tokens) or tokens[cursor].text not in {"{", ";"}:
        return None
    if not name:
        name = f"<anonymous@{tokens[type_index].line}>"
    declaration = "definition" if opening is not None else "forward declaration"
    return name, opening, declaration


def declaration_comment_offset(source: str, token: Token) -> int:
    line_start = source.rfind("\n", 0, token.offset) + 1
    prefix = source[line_start:token.offset].strip()
    if prefix and (
        re.fullmatch(r"(?:\[\[.*\]\]\s*)+", prefix)
        or re.fullmatch(r"[A-Z_][A-Z0-9_]*(?:\([^)]*\))?", prefix)
    ):
        offset = line_start
    else:
        offset = token.offset
    while line_start > 0:
        previous_end = line_start - 1
        previous_start = source.rfind("\n", 0, previous_end) + 1
        previous = source[previous_start:previous_end].strip()
        if not re.fullmatch(r"(?:\[\[.*\]\]\s*)+", previous):
            break
        offset = previous_start
        line_start = previous_start
    return offset


def documentation_before(source: str, offset: int) -> tuple[str, str]:
    prefix = source[:offset].rstrip()
    line_block = re.search(r"(?m)(?P<block>(?:^[ \t]*///[^\n]*(?:\n|$))+)\Z", prefix)
    if line_block:
        raw = line_block.group("block")
        brief_match = re.search(r"(?m)^[ \t]*///[ \t]*@brief[ \t]+(.+?)\s*$", raw)
        return raw.strip(), brief_match.group(1).strip() if brief_match else ""
    if prefix.endswith("*/"):
        start = max(prefix.rfind("/**"), prefix.rfind("/*!"))
        if start >= 0:
            raw = prefix[start:]
            brief_match = re.search(r"@brief[ \t]+([^\r\n*]+)", raw)
            return raw.strip(), brief_match.group(1).strip() if brief_match else ""
    return "", ""


def style_issues_for_comment(documentation: str, brief: str) -> list[str]:
    if not documentation:
        return ["missing documentation comment"]
    if not brief:
        return ["missing @brief"]
    issues: list[str] = []
    normalized = re.sub(r"\s+", " ", brief).strip()
    if normalized.endswith("。"):
        issues.append("one-line @brief ends with Japanese full stop")
    if any(pattern.search(normalized) for pattern in GENERIC_BRIEF_PATTERNS):
        issues.append("forbidden generic @brief")
    return issues


def namespace_openings(tokens: list[Token]) -> dict[int, list[str]]:
    openings: dict[int, list[str]] = {}
    for index, token in enumerate(tokens):
        if token.text != "namespace":
            continue
        names: list[str] = []
        cursor = index + 1
        while cursor < len(tokens) and tokens[cursor].text not in {"{", ";", "="}:
            if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", tokens[cursor].text):
                names.append(tokens[cursor].text)
            cursor += 1
        if cursor < len(tokens) and tokens[cursor].text == "{":
            openings[cursor] = names or [f"<anonymous-namespace@{token.line}>"]
    return openings


def scan_cpp_file(
    path: str,
    source: str,
    origin: str,
    source_control: str,
    changed_briefs: set[tuple[str, str]] | None = None,
) -> tuple[list[TypeRecord], list[str]]:
    tokens, errors = tokenize_cpp(source)
    parameter_tokens, template_ends = template_metadata(tokens)
    records: list[TypeRecord] = []
    class_openings: dict[int, str] = {}
    for index, token in enumerate(tokens):
        if token.text not in {"class", "struct"}:
            continue
        candidate = scan_type_candidate(tokens, index, parameter_tokens)
        if candidate is None:
            continue
        name, opening, declaration = candidate
        template_start = preceding_template(tokens, index, template_ends)
        start_token = tokens[template_start] if template_start is not None else token
        comment_offset = declaration_comment_offset(source, start_token)
        documentation, brief = documentation_before(source, comment_offset)
        detected_style_issues = style_issues_for_comment(documentation, brief)
        is_changed = changed_briefs is None or (path, brief) in changed_briefs
        if declaration == "forward declaration":
            issues = []
            syntax_status = "not-applicable"
            style_status = "not-applicable"
        else:
            issues = [
                issue
                for issue in detected_style_issues
                if is_changed
                or issue in {"missing documentation comment", "missing @brief"}
            ]
            syntax_status = "pass" if documentation and brief else "fail"
            style_status = (
                "fail" if issues else "pass" if is_changed else "out-of-scope"
            )
        decision = (
            CONCEPT_DECISIONS.get((path, name), "")
            if template_start is not None and declaration == "definition"
            else ""
        )
        record = TypeRecord(
            file=path,
            line=token.line,
            kind=token.text,
            name=name,
            qualified_name=name,
            declaration=declaration,
            origin=origin,
            source_control=source_control,
            brief=brief,
            comment_scope="changed" if is_changed else "pre-existing",
            syntax_status=syntax_status,
            style_status=style_status,
            semantic_status="unreviewed",
            style_issues=issues,
            class_template=template_start is not None and declaration == "definition",
            concept_decision=decision,
            opening_token=opening,
            token_index=index,
        )
        records.append(record)
        if opening is not None:
            class_openings[opening] = name.split("::")[-1]

    record_by_token = {record.token_index: record for record in records}
    namespaces = namespace_openings(tokens)
    scopes: list[str] = []
    brace_stack: list[int] = []
    structural_depth = 0
    for index, token in enumerate(tokens):
        record = record_by_token.get(index)
        if record is not None:
            local = [f"<local@{record.line}>"] if len(brace_stack) > structural_depth else []
            record.qualified_name = (
                record.name
                if "::" in record.name
                else "::".join(scopes + local + [record.name])
            )
        if token.text == "{":
            opened = namespaces.get(index)
            if opened is None and index in class_openings:
                opened = [class_openings[index]]
            pushed = len(opened) if opened else 0
            if opened:
                scopes.extend(opened)
                structural_depth += 1
            brace_stack.append(pushed)
        elif token.text == "}" and brace_stack:
            pushed = brace_stack.pop()
            if pushed:
                del scopes[-pushed:]
                structural_depth -= 1
    return records, errors


def canonical_type_rows(data: dict[str, object]) -> list[tuple[object, ...]]:
    rows = []
    for entry in data.get("types", []):
        if not isinstance(entry, dict):
            continue
        rows.append(
            (
                entry.get("file"),
                entry.get("line"),
                entry.get("kind"),
                entry.get("name"),
                entry.get("qualified_name"),
                entry.get("declaration"),
                entry.get("origin"),
                entry.get("brief"),
                entry.get("comment_scope"),
                entry.get("syntax_status"),
                entry.get("style_status"),
                tuple(entry.get("style_issues", [])),
                entry.get("class_template"),
                entry.get("concept_decision"),
            )
        )
    return sorted(rows, key=repr)


def compare_ledger(
    audit_result: dict[str, object], ledger_path: pathlib.Path
) -> dict[str, object]:
    if not ledger_path.is_file():
        return {"status": "missing", "differences": ["ledger file is missing"]}
    try:
        ledger = json.loads(ledger_path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        return {"status": "invalid", "differences": [f"ledger cannot be read: {error}"]}
    differences: list[str] = []
    if canonical_type_rows(audit_result) != canonical_type_rows(ledger):
        differences.append("type inventory or objective comment data differs from fresh audit")
    audit_counts = {
        key: value
        for key, value in audit_result.get("counts", {}).items()
        if key not in EPHEMERAL_COUNT_KEYS
    }
    ledger_counts = {
        key: value
        for key, value in ledger.get("counts", {}).items()
        if key not in EPHEMERAL_COUNT_KEYS
    }
    if audit_counts != ledger_counts:
        differences.append("summary counts differ from fresh audit")
    review_rows = ledger.get("semantic_reviews")
    if not isinstance(review_rows, list):
        differences.append("semantic review records are missing")
    else:
        expected = {
            (entry["file"], entry["line"], entry["qualified_name"])
            for entry in audit_result.get("types", [])
            if entry.get("declaration") == "definition"
            and entry.get("comment_scope") == "changed"
        }
        observed = {
            (entry.get("file"), entry.get("line"), entry.get("qualified_name"))
            for entry in review_rows
            if isinstance(entry, dict)
        }
        if observed != expected:
            differences.append("semantic review records do not cover each changed definition")
        elif any(
            entry.get("semantic_review") not in {"verified", "rewritten"}
            or not entry.get("actual_responsibility")
            or not entry.get("important_contracts")
            or not entry.get("evidence")
            for entry in review_rows
            if isinstance(entry, dict)
        ):
            differences.append("semantic review records are incomplete")
    return {"status": "current" if not differences else "stale", "differences": differences}


def audit(root: pathlib.Path) -> dict[str, object]:
    files, missing_tracked = repository_files(root)
    changed_briefs = changed_type_briefs(root, files)
    untracked_files = sorted(
        entry.path for entry in files if entry.source_control == "untracked"
    )
    cpp_files = [
        entry
        for entry in files
        if pathlib.PurePosixPath(entry.path).suffix.lower() in CPP_SUFFIXES
    ]
    file_rows: list[dict[str, object]] = []
    types: list[TypeRecord] = []
    scanner_errors: list[str] = []
    for entry in cpp_files:
        origin, reason = classify_origin(entry.path)
        if origin != "first-party":
            file_rows.append(
                {
                    "file": entry.path,
                    "origin": origin,
                    "source_control": entry.source_control,
                    "reason": reason,
                    "scanned": False,
                    "definitions": 0,
                    "forward_declarations": 0,
                }
            )
            continue
        try:
            source = (root / entry.path).read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as error:
            scanner_errors.append(f"{entry.path}: cannot read UTF-8 source: {error}")
            continue
        records, errors = scan_cpp_file(
            entry.path,
            source,
            origin,
            entry.source_control,
            changed_briefs,
        )
        types.extend(records)
        scanner_errors.extend(f"{entry.path}: {error}" for error in errors)
        file_rows.append(
            {
                "file": entry.path,
                "origin": origin,
                "source_control": entry.source_control,
                "reason": reason,
                "scanned": True,
                "definitions": sum(r.declaration == "definition" for r in records),
                "forward_declarations": sum(
                    r.declaration == "forward declaration" for r in records
                ),
            }
        )

    definitions = [
        record
        for record in types
        if record.origin == "first-party" and record.declaration == "definition"
    ]
    duplicate_counts = Counter(record.brief for record in definitions if record.brief)
    all_duplicate_briefs = {
        brief: count for brief, count in duplicate_counts.items() if count > 1
    }
    duplicate_briefs = {
        brief: count
        for brief, count in all_duplicate_briefs.items()
        if any(
            record.brief == brief and record.comment_scope == "changed"
            for record in definitions
        )
    }
    for record in definitions:
        if record.brief in duplicate_briefs and record.comment_scope == "changed":
            record.style_issues.append("exact duplicate @brief")
            record.style_status = "fail"

    tracked_paths = {entry.path for entry in files if entry.source_control == "tracked"}
    forbidden_tracked = sorted(
        path
        for path in tracked_paths
        if any(pattern.search(path) for pattern in FORBIDDEN_TRACKED_PATTERNS)
        and not (
            path.lower().endswith(".obj")
            and (path.startswith("content/") or "/content/" in path)
        )
    )
    try:
        gitignore_lines = {
            line.strip()
            for line in (root / ".gitignore").read_text(encoding="utf-8-sig").splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        }
    except OSError as error:
        raise RepositoryAuditError(f"cannot read .gitignore: {error}") from error
    missing_artifacts = sorted(
        path for path in REQUIRED_ARTIFACTS if not (root / path).is_file()
    )
    untracked_required_artifacts = sorted(
        path for path in REQUIRED_ARTIFACTS if path in untracked_files
    )
    missing_gitignore = sorted(REQUIRED_GITIGNORE_RULES - gitignore_lines)
    serialized_types = []
    for record in types:
        row = dataclasses.asdict(record)
        row.pop("opening_token")
        row.pop("token_index")
        serialized_types.append(row)
    return {
        "schema_version": 2,
        "scanner": "lexical C++ declaration inventory; not a compiler AST",
        "verification_scope": {
            "automatic": [
                "tracked and non-ignored untracked repository inventory",
                "lexical class/struct definitions and forward declarations",
                "@brief presence, terminal full stop, forbidden generic text, exact duplicates",
                "required artifact presence, tracked build artifacts, gitignore rules",
                "class-template decision record presence and JSON ledger freshness",
            ],
            "manual": [
                "semantic accuracy and sufficiency of comments",
                "macro-expanded class/struct definitions",
                "Concept design correctness",
                "PO-01 through PO-03 and Visual Studio target ownership",
            ],
        },
        "limitations": [
            "macro-generated declarations are not expanded and remain UNVERIFIED",
            "unknown preprocessor branches are scanned on both sides; only #if 0 is excluded",
            "local function names use a line-qualified placeholder",
        ],
        "counts": {
            "repository_cpp_files": len(cpp_files),
            "tracked_cpp_files": sum(e.source_control == "tracked" for e in cpp_files),
            "untracked_cpp_files": sum(e.source_control == "untracked" for e in cpp_files),
            "first_party_cpp_files": sum(row["origin"] == "first-party" for row in file_rows),
            "excluded_cpp_files": sum(row["origin"] != "first-party" for row in file_rows),
            "first_party_class_definitions": sum(r.kind == "class" for r in definitions),
            "first_party_struct_definitions": sum(r.kind == "struct" for r in definitions),
            "style_failures": sum(bool(r.style_issues) for r in definitions),
            "changed_type_briefs": (
                sum(r.comment_scope == "changed" for r in definitions)
                if changed_briefs is not None
                else len(definitions)
            ),
            "missing_class_comments": sum(
                r.kind == "class" and "missing documentation comment" in r.style_issues
                for r in definitions
            ),
            "missing_struct_comments": sum(
                r.kind == "struct" and "missing documentation comment" in r.style_issues
                for r in definitions
            ),
            "class_templates": sum(r.class_template for r in definitions),
            "class_templates_without_decision": sum(
                r.class_template and not r.concept_decision for r in definitions
            ),
            "duplicate_briefs": len(duplicate_briefs),
            "forbidden_tracked_files": len(forbidden_tracked),
            "missing_required_artifacts": len(missing_artifacts),
            "untracked_required_artifacts": len(untracked_required_artifacts),
        },
        "missing_tracked_files": missing_tracked,
        "scanner_errors": scanner_errors,
        "duplicate_briefs": duplicate_briefs,
        "preexisting_duplicate_briefs": {
            brief: count
            for brief, count in all_duplicate_briefs.items()
            if brief not in duplicate_briefs
        },
        "missing_required_artifacts": missing_artifacts,
        "untracked_required_artifacts": untracked_required_artifacts,
        "missing_gitignore_rules": missing_gitignore,
        "forbidden_tracked_files": forbidden_tracked,
        "files": file_rows,
        "types": serialized_types,
    }


def verification_errors(result: dict[str, object]) -> list[str]:
    counts = result["counts"]
    assert isinstance(counts, dict)
    errors: list[str] = []
    if result["scanner_errors"]:
        errors.append(f"{len(result['scanner_errors'])} lexical scanner errors")
    if counts["style_failures"]:
        errors.append(
            f"{counts['style_failures']} class/struct comments fail syntax/style policy"
        )
    if result["missing_tracked_files"]:
        errors.append(f"{len(result['missing_tracked_files'])} tracked files are missing")
    if counts["class_templates_without_decision"]:
        errors.append(
            f"{counts['class_templates_without_decision']} class templates lack a review decision"
        )
    if counts["forbidden_tracked_files"]:
        errors.append(f"{counts['forbidden_tracked_files']} forbidden generated files are tracked")
    if counts["missing_required_artifacts"]:
        errors.append(f"{counts['missing_required_artifacts']} required artifacts are missing")
    if counts["untracked_required_artifacts"]:
        errors.append(
            f"{counts['untracked_required_artifacts']} required artifacts are untracked"
        )
    if result["missing_gitignore_rules"]:
        errors.append(f"{len(result['missing_gitignore_rules'])} required gitignore rules are missing")
    ledger = result.get("ledger_comparison", {})
    if not isinstance(ledger, dict) or ledger.get("status") != "current":
        errors.append("JSON ledger is missing, invalid, or stale")
    return errors


def write_json(path: pathlib.Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def markdown_cell(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\r", " ").replace("\n", "<br>")


def update_markdown_ledger(path: pathlib.Path, data: dict[str, object]) -> None:
    begin = "<!-- BEGIN GENERATED REPOSITORY LEDGER -->"
    end = "<!-- END GENERATED REPOSITORY LEDGER -->"
    manual = path.read_text(encoding="utf-8-sig") if path.exists() else "# Maintenance ExecPlan\n"
    if begin in manual:
        manual = manual.split(begin, 1)[0].rstrip() + "\n"
    lines = [
        "",
        begin,
        "",
        "## Generated class/struct inventory",
        "",
        "| File | Line | Kind | Name | Declaration | Source | Brief | Syntax | Style | Concept decision |",
        "|---|---:|---|---|---|---|---|---|---|---|",
    ]
    for entry in data["types"]:
        values = (
            entry["file"],
            entry["line"],
            entry["kind"],
            entry["qualified_name"],
            entry["declaration"],
            entry["source_control"],
            entry["brief"] or "-",
            entry["syntax_status"],
            entry["style_status"],
            entry["concept_decision"] or "-",
        )
        lines.append("| " + " | ".join(markdown_cell(value) for value in values) + " |")
    lines.extend(
        [
            "",
            "## Generated C++ file coverage",
            "",
            "| File | Origin | Source | Scanned | Definitions | Forward declarations |",
            "|---|---|---|---|---:|---:|",
        ]
    )
    for entry in data["files"]:
        values = (
            entry["file"],
            entry["origin"],
            entry["source_control"],
            "yes" if entry["scanned"] else "no",
            entry["definitions"],
            entry["forward_declarations"],
        )
        lines.append("| " + " | ".join(markdown_cell(value) for value in values) + " |")
    lines.extend(["", end, ""])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(manual.rstrip() + "\n" + "\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--markdown-ledger", type=pathlib.Path)
    parser.add_argument(
        "--ledger",
        type=pathlib.Path,
        default=pathlib.Path(
            "docs/maintenance/class-comments-project-organization-ledger.json"
        ),
    )
    parser.add_argument("--verify", action="store_true")
    arguments = parser.parse_args()
    try:
        root = arguments.root.resolve()
        result = audit(root)
        ledger_path = arguments.ledger
        if not ledger_path.is_absolute():
            ledger_path = root / ledger_path
        result["ledger_comparison"] = compare_ledger(result, ledger_path)

        if arguments.output:
            output = arguments.output
            if not output.is_absolute():
                output = root / output
            output_result = result
            if output.resolve() == ledger_path.resolve():
                output_result = dict(result)
                output_result["ledger_comparison"] = {
                    "status": "current",
                    "differences": [],
                    "note": "generated from the fresh audit stored in this file",
                }
            write_json(output, output_result)
        if arguments.markdown_ledger:
            markdown = arguments.markdown_ledger
            if not markdown.is_absolute():
                markdown = root / markdown
            update_markdown_ledger(markdown, result)

        print(json.dumps(result["counts"], ensure_ascii=False, indent=2))
        print(f"scanner_errors={len(result['scanner_errors'])}")
        print(f"ledger_status={result['ledger_comparison']['status']}")
        if arguments.verify:
            errors = verification_errors(result)
            for error in errors:
                print(f"ERROR: {error}", file=sys.stderr)
            return 1 if errors else 0
        return 0
    except RepositoryAuditError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"ERROR: repository audit I/O failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
