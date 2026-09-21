"""只读检查迁移资产能否脱离项目 CoreRedirects。

PowerShell: py -3 Scripts/Python/migration/audit_redirect_independence.py
默认扫描磁盘并运行独立 NullRHI commandlet；--scan-only 仅检查旧路径字符串。
不修改正式配置、不保存资产、不删除 Redirector、不编译 C++。
报告中的字符串命中是保守候选，可能包含元数据，不能等同于有效依赖。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import runpy
import subprocess
import tempfile


REDIRECT = re.compile(r'^\s*\+((?:Package|Object)Redirects)=\(OldName="([^"]+)",NewName="([^"]+)"\)\s*$')
PACKAGE_PATH = re.compile(rb"/Game/[A-Za-z0-9_/-]+")


def migration_redirects(text):
    """只识别本次项目迁移的精确映射，其他引擎/插件映射保持不变。"""
    result = []
    section = ""
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped.lower()
        match = REDIRECT.fullmatch(line)
        if section == "[coreredirects]" and match:
            kind, old, new = match.groups()
            if old.startswith("/Game/") and new.startswith("/Game/ProjectArcaneArena/"):
                result.append((kind, old, new))
    return result


def without_migration_redirects(text, redirects):
    """生成临时配置副本，只移除已识别迁移映射，不写正式 DefaultEngine。"""
    excluded = set(redirects)
    output = []
    section = ""
    for line in text.splitlines(keepends=True):
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped.lower()
        match = REDIRECT.fullmatch(line.rstrip("\r\n"))
        if section == "[coreredirects]" and match and match.groups() in excluded:
            continue
        output.append(line)
    return "".join(output)


def scan_serialized_paths(project, old_packages):
    """扫描实际资产字节中的旧包名；同时覆盖 UTF-8 和 UTF-16 名称。"""
    candidates = []
    count = 0
    for path in sorted((project / "Content").rglob("*")):
        if path.suffix.lower() not in {".uasset", ".umap"} or not path.is_file():
            continue
        count += 1
        data = path.read_bytes()
        if data.startswith(b"version https://git-lfs.github.com/spec/"):
            raise RuntimeError(f"LFS asset has not been downloaded: {path}")
        names = {m.group().decode("ascii") for m in PACKAGE_PATH.finditer(data)}
        # ASCII 路径以 UTF-16 序列化时，去掉 NUL 后再匹配；结果仅作为候选。
        if b"/\x00G\x00a\x00m\x00e\x00/\x00" in data:
            names.update(m.group().decode("ascii") for m in PACKAGE_PATH.finditer(data.replace(b"\x00", b"")))
        hits = sorted(names.intersection(old_packages))
        if hits:
            candidates.append({"asset": path.relative_to(project).as_posix(), "old_packages": hits})
    return count, candidates


def commandlet_check():
    """在临时配置启动的新 UE 进程中复用只读引用验收，保存断言结果。"""
    import unreal

    result_path = Path(os.environ["ARENA_REDIRECT_AUDIT_RESULT"])
    result = {"completed": False, "failures": [], "error": None}
    checks = runpy.run_path(str(Path(__file__).with_name("validate_migrated_references.py")))
    try:
        checks["main"](check_legacy_paths=False)
        result["completed"] = True
    except Exception as exc:
        result["error"] = str(exc)
        unreal.log_error("[RedirectIndependence] " + str(exc))
    finally:
        result["failures"] = list(checks["failures"])
        result_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    if not result["completed"]:
        raise RuntimeError("Assets are not independent of migration redirects; see audit report")


def launch_audit():
    """输出可复查报告，通过 DEFENGINEINI 隔离禁用映射，不修改当前编辑器配置。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scan-only", action="store_true")
    parser.add_argument("--editor", type=Path, default=Path(
        "E:/Unreal engine/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"))
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[3]
    config = project / "Config/DefaultEngine.ini"
    original = config.read_bytes()
    text = original.decode("utf-8-sig")
    redirects = migration_redirects(text)
    if not redirects:
        raise RuntimeError("No migration redirects found; restore the mapping before auditing removal")
    report_root = project / "Saved/MigrationReports"
    report_root.mkdir(parents=True, exist_ok=True)
    report_dir = Path(tempfile.mkdtemp(prefix="redirect_audit_", dir=report_root))
    packages = {old for kind, old, _ in redirects if kind == "PackageRedirects"}
    scanned, candidates = scan_serialized_paths(project, packages)
    report = {"status": "SCAN_ONLY", "scanned_assets": scanned, "migration_redirects": len(redirects),
              "candidate_count": len(candidates), "serialized_path_candidates": candidates,
              "source_config_sha256": hashlib.sha256(original).hexdigest(), "runtime": None,
              "limitations": "String hits may be metadata or redirector assets. NullRHI does not verify rendering, complete gameplay, Cook or external branches."}
    print(f"Scanned {scanned} assets; {len(candidates)} contain old migration package names.", flush=True)
    if not args.scan_only:
        if not args.editor.is_file():
            raise FileNotFoundError(args.editor)
        isolated = report_dir / "DefaultEngine.ini"
        isolated.write_text(without_migration_redirects(text, redirects), encoding="utf-8")
        assert not migration_redirects(isolated.read_text(encoding="utf-8"))
        result_path = report_dir / "runtime_checks.json"
        log_path = report_dir / "Unreal.log"
        env = dict(os.environ, ARENA_REDIRECT_AUDIT_RESULT=str(result_path))
        command = [str(args.editor), str(project / "ProjectArcaneArena.uproject"),
                   "-run=pythonscript", "-script=" + str(Path(__file__).resolve()),
                   "-DEFENGINEINI=" + str(isolated), "-unattended", "-NullRHI", "-nosound", "-nop4",
                   "-traceautostart=0", "-abslog=" + str(log_path)]
        print("Checking in a fresh Unreal process with a temporary redirect-free DefaultEngine.ini...", flush=True)
        startup = None
        if os.name == "nt":
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (report_dir / "process_output.log").open("w", encoding="utf-8") as output:
            try:
                process = subprocess.run(command, env=env, cwd=project, stdout=output,
                                         stderr=subprocess.STDOUT, startupinfo=startup, timeout=180)
                report["process_exit_code"] = process.returncode
            except subprocess.TimeoutExpired:
                report["process_exit_code"] = "TIMEOUT"
        report["runtime"] = json.loads(result_path.read_text(encoding="utf-8")) if result_path.exists() else {
            "completed": False, "error": "No runtime report; inspect process_output.log"}
        log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
        errors = [line for line in log.splitlines() if any(token in line for token in (
            "LoadErrors:", "Failed to load", "SkipPackage:", "LogLinker: Warning", "LogPython: Error"))
                  and ("/Game/" in line or "MigrationReferences" in line or "RedirectIndependence" in line)]
        report["load_errors"] = errors
        failed = report["process_exit_code"] != 0 or not report["runtime"]["completed"] or bool(errors)
        report["status"] = "NOT_READY" if failed else ("REVIEW_CANDIDATES" if candidates else "LOAD_CHECKS_PASSED")
    report["source_config_unchanged"] = config.read_bytes() == original
    if not report["source_config_unchanged"]:
        report["status"] = "CONFIG_CHANGED_DURING_AUDIT"
    report_path = report_dir / "report.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    summary = ["# 迁移重定向独立性检查", "", f"结果：`{report['status']}`", "",
               f"扫描资产：{scanned}；包含旧包名的候选文件：{len(candidates)}。", "",
               "本脚本没有重存资产、删除 Redirector 或修改正式配置。候选字节命中也可能来自元数据，不等于真实依赖。",
               "通过加载检查后仍需渲染、PIE 与 Cook 验证，不能直接宣称全部映射可以删除。", "",
               "## 旧路径候选（前 50 个，完整列表见 report.json）", ""]
    summary.extend(f"- `{item['asset']}` → {', '.join(item['old_packages'])}" for item in candidates[:50])
    (report_dir / "README.md").write_text("\n".join(summary) + "\n", encoding="utf-8")
    print(f"{report['status']}: {report_path}", flush=True)
    print(f"Original DefaultEngine.ini unchanged: {report['source_config_unchanged']}", flush=True)
    return 0 if report["status"] == "LOAD_CHECKS_PASSED" else 2


if __name__ == "__main__":
    if os.environ.get("ARENA_REDIRECT_AUDIT_RESULT"):
        commandlet_check()
    else:
        raise SystemExit(launch_audit())
