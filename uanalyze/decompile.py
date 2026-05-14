from __future__ import annotations

import os
import shutil
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


@dataclass(frozen=True)
class DecompileResult:
    input_file: Path
    output_dir: Path
    return_code: int
    elapsed_seconds: float


def decompile_pe_files(
    pe_files: list[Path],
    out_root: Path,
    project_root: Path,
    ghidra_headless: Path,
    script_roots: list[Path],
    *,
    jobs: int,
    progress_callback: Callable[[int, int, DecompileResult], None] | None = None,
) -> list[DecompileResult]:
    if not pe_files:
        return []

    if not ghidra_headless.is_file():
        raise FileNotFoundError(
            f"Ghidra headless launcher not found: {ghidra_headless}"
        )
    for script_root in script_roots:
        if not script_root.exists():
            raise FileNotFoundError(f"Ghidra script path not found: {script_root}")

    out_root.mkdir(parents=True, exist_ok=True)
    project_root.mkdir(parents=True, exist_ok=True)

    results: list[DecompileResult] = []
    with ThreadPoolExecutor(max_workers=max(1, jobs)) as executor:
        futures = [
            executor.submit(
                _run_one,
                pe_file,
                out_root,
                project_root,
                ghidra_headless,
                script_roots,
            )
            for pe_file in pe_files
        ]
        total = len(futures)
        for completed, future in enumerate(as_completed(futures), start=1):
            result = future.result()
            results.append(result)
            if progress_callback is not None:
                progress_callback(completed, total, result)
        return results


def _run_one(
    pe_file: Path,
    out_root: Path,
    project_root: Path,
    ghidra_headless: Path,
    script_roots: list[Path],
) -> DecompileResult:
    name = _derive_name(pe_file)
    out_dir = out_root / name
    out_dir.mkdir(parents=True, exist_ok=True)

    copied_input = out_dir / "main.efi"
    shutil.copy2(pe_file, copied_input)
    decompiled_output = out_dir / "decompiled_main.c"
    script_path = ";".join(str(path.resolve()) for path in script_roots)
    project_name = f"efi_analysis_{name}_{int(time.time() * 1000)}_{os.getpid()}"

    command = [
        str(ghidra_headless),
        str(project_root.resolve()),
        project_name,
        "-import",
        str(copied_input.resolve()),
        "-overwrite",
        "-scriptPath",
        script_path,
        "-postScript",
        "UEFIHelper.java",
        "-postScript",
        "ExportDecompiled.java",
        str(decompiled_output),
    ]

    start = time.time()
    try:
        result = subprocess.run(
            command,
            cwd=out_dir,
            capture_output=True,
            text=True,
            check=False,
        )
        (out_dir / "ghidra.log").write_text(
            f"$ {' '.join(command)}\n\nSTDOUT:\n{result.stdout}\n\nSTDERR:\n{result.stderr}",
            encoding="utf-8",
        )
        return_code = result.returncode
    finally:
        shutil.rmtree(project_root / project_name, ignore_errors=True)

    elapsed_seconds = time.time() - start
    return DecompileResult(
        input_file=pe_file,
        output_dir=out_dir,
        return_code=return_code,
        elapsed_seconds=elapsed_seconds,
    )


def _derive_name(path: Path) -> str:
    stem = path.stem
    if stem.endswith("_body"):
        return stem[:-5]
    return stem
