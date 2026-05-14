from __future__ import annotations

import re
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CollectedPeFile:
    source: Path
    copied_to: Path


def is_pe32(path: Path) -> bool:
    """Use the system file command to detect EFI PE payloads."""
    try:
        result = subprocess.run(
            ["file", "-b", str(path)],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return False

    if result.returncode != 0:
        return False

    description = result.stdout.strip()
    return "PE32" in description and "EFI" in description


def collect_pe_files(
    extracted_root: Path,
    output_dir: Path,
    *,
    min_size_bytes: int = 8 * 1024,
) -> list[CollectedPeFile]:
    output_dir.mkdir(parents=True, exist_ok=True)
    collected: list[CollectedPeFile] = []

    for body_file in extracted_root.rglob("body.bin"):
        if not body_file.is_file() or body_file.stat().st_size <= min_size_bytes:
            continue
        if not is_pe32(body_file):
            continue

        relative_parent = body_file.parent.relative_to(extracted_root)
        destination_name = _build_destination_name(relative_parent, body_file.name)
        destination_path = _dedupe_path(output_dir / destination_name)
        shutil.copy2(body_file, destination_path)
        collected.append(CollectedPeFile(source=body_file, copied_to=destination_path))

    return collected


def _build_destination_name(relative_parent: Path, filename: str) -> str:
    raw = "_".join(relative_parent.parts[-3:]) if relative_parent.parts else "root"
    safe = re.sub(r"[^A-Za-z0-9._-]+", "_", raw).strip("._")
    if not safe:
        safe = "module"
    return f"{safe}_{filename}"


def _dedupe_path(path: Path) -> Path:
    if not path.exists():
        return path

    stem = path.stem
    suffix = path.suffix
    parent = path.parent
    index = 1
    while True:
        candidate = parent / f"{stem}_{index}{suffix}"
        if not candidate.exists():
            return candidate
        index += 1
