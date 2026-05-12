from __future__ import annotations

import re
import shutil
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CollectedPeFile:
    source: Path
    copied_to: Path


def is_pe32(path: Path) -> bool:
    """Lightweight PE signature check without external tools."""
    try:
        with path.open("rb") as handle:
            if handle.read(2) != b"MZ":
                return False
            handle.seek(0x3C)
            pe_offset_bytes = handle.read(4)
            if len(pe_offset_bytes) != 4:
                return False
            pe_offset = int.from_bytes(pe_offset_bytes, "little")
            handle.seek(pe_offset)
            if handle.read(4) != b"PE\x00\x00":
                return False
            machine_bytes = handle.read(2)
            if len(machine_bytes) != 2:
                return False
            machine = int.from_bytes(machine_bytes, "little")
            return machine in {0x014C, 0x8664, 0x0200, 0xAA64}
    except OSError:
        return False


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

