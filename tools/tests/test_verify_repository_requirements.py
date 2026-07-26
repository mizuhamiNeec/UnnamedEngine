from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


TOOLS_DIR = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS_DIR))

import verify_repository_requirements as verifier


class CppScannerFixtureTests(unittest.TestCase):
    def test_declarations_comments_attributes_macros_strings_and_if_zero(self) -> None:
        source = r'''
/// @brief tracked型の責務を説明します
class TrackedDocumented {};

struct TrackedMissing {};

/// @brief 属性付き型の責務を説明します
[[nodiscard]] struct Attributed {};

/// @brief export対象型の責務を説明します
class TEST_API Exported {};

class ForwardOnly;
const char* text = R"fixture(class RawStringFake {};)fixture";
#if 0
struct DisabledFake {};
#endif
'''
        records, errors = verifier.scan_cpp_file("src/fixture.h", source, "first-party", "tracked")
        self.assertEqual([], errors)
        definitions = {record.name: record for record in records if record.declaration == "definition"}
        self.assertEqual({"TrackedDocumented", "TrackedMissing", "Attributed", "Exported"}, set(definitions))
        self.assertEqual("pass", definitions["TrackedDocumented"].syntax_status)
        self.assertEqual("fail", definitions["TrackedMissing"].style_status)
        self.assertEqual("unreviewed", definitions["TrackedDocumented"].semantic_status)
        self.assertEqual({"ForwardOnly"}, {record.name for record in records if record.declaration == "forward declaration"})

    def test_style_policy_detects_period_and_generic_brief(self) -> None:
        period = verifier.style_issues_for_comment("/// @brief 型固有の責務を説明します。", "型固有の責務を説明します。")
        generic = verifier.style_issues_for_comment("/// @brief データを保持します", "データを保持します")
        self.assertIn("one-line @brief ends with Japanese full stop", period)
        self.assertIn("forbidden generic @brief", generic)


class RepositoryInventoryFixtureTests(unittest.TestCase):
    def create_repository(self, *, required_artifacts_tracked: bool = True) -> pathlib.Path:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = pathlib.Path(temporary.name)
        subprocess.run(["git", "init", "-q"], cwd=root, check=True)
        (root / ".gitignore").write_text("\n", encoding="utf-8")
        (root / "tracked.h").write_text("/// @brief tracked型の責務を説明します\nstruct Tracked {};\n", encoding="utf-8")
        (root / "untracked.h").write_text("struct UntrackedMissing {};\n", encoding="utf-8")
        for path in verifier.REQUIRED_ARTIFACTS:
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("fixture\n", encoding="utf-8")
        paths = [".gitignore", "tracked.h"]
        if required_artifacts_tracked:
            paths.extend(sorted(verifier.REQUIRED_ARTIFACTS))
        subprocess.run(["git", "add", *paths], cwd=root, check=True)
        return root

    def test_audit_includes_tracked_and_untracked_cpp(self) -> None:
        result = verifier.audit(self.create_repository())
        counts = result["counts"]
        self.assertEqual(1, counts["untracked_cpp_files"])
        untracked = [entry for entry in result["types"] if entry["name"] == "UntrackedMissing"]
        self.assertEqual("untracked", untracked[0]["source_control"])
        self.assertEqual("fail", untracked[0]["syntax_status"])

    def test_untracked_required_artifacts_fail_verification(self) -> None:
        result = verifier.audit(self.create_repository(required_artifacts_tracked=False))
        self.assertEqual(len(verifier.REQUIRED_ARTIFACTS), result["counts"]["untracked_required_artifacts"])
        self.assertIn("required artifacts are untracked", "\n".join(verifier.verification_errors(result)))

    def test_non_ascii_tracked_path_is_not_missing(self) -> None:
        root = self.create_repository()
        japanese = root / "日本語 path" / "空白を含む.h"
        japanese.parent.mkdir(parents=True)
        japanese.write_text("/// @brief 非ASCIIパスの型を説明します\nstruct JapanesePath {};\n", encoding="utf-8")
        subprocess.run(["git", "add", str(japanese.relative_to(root))], cwd=root, check=True)
        result = verifier.audit(root)
        self.assertEqual([], result["missing_tracked_files"])
        self.assertEqual({"日本語 path/引用符\".h", "back\\slash.h"}, verifier.null_paths("日本語 path/引用符\".h\0back\\slash.h\0".encode("utf-8")))

    def test_missing_tracked_file_fails_verification(self) -> None:
        root = self.create_repository()
        (root / "tracked.h").unlink()
        result = verifier.audit(root)
        self.assertIn("tracked.h", result["missing_tracked_files"])
        self.assertIn("tracked files are missing", "\n".join(verifier.verification_errors(result)))

    def test_duplicate_brief_is_detected_by_repository_audit(self) -> None:
        root = self.create_repository()
        duplicate = root / "duplicate.h"
        duplicate.write_text("/// @brief 同じ責務を説明します\nstruct First {};\n/// @brief 同じ責務を説明します\nstruct Second {};\n", encoding="utf-8")
        subprocess.run(["git", "add", "duplicate.h"], cwd=root, check=True)
        result = verifier.audit(root)
        self.assertEqual(1, result["counts"]["duplicate_briefs"])

    def test_old_json_ledger_is_reported_stale(self) -> None:
        root = self.create_repository()
        fresh = verifier.audit(root)
        ledger = root / "ledger.json"
        ledger.write_text(json.dumps({"schema_version": 3, "counts": {}, "types": []}), encoding="utf-8")
        self.assertEqual("stale", verifier.compare_ledger(fresh, ledger)["status"])

    def test_git_subprocess_failure_is_concise_repository_error(self) -> None:
        failure = subprocess.CalledProcessError(128, ["git", "ls-files"], stderr="fatal: not a git repository\n")
        with mock.patch.object(verifier.subprocess, "run", side_effect=failure):
            with self.assertRaisesRegex(verifier.RepositoryAuditError, r"git ls-files -z failed with exit code 128: fatal: not a git repository"):
                verifier.run_git(pathlib.Path("."), "ls-files", "-z")


if __name__ == "__main__":
    unittest.main()
