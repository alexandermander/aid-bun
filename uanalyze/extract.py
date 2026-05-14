from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


def extract_uefi_image(
    image_path: Path,
    output_dir: Path,
    uefiextract: str = "uefiextract",
) -> Path:
    """Extract a UEFI image into ``output_dir`` using UEFITool's extractor."""
    if not image_path.is_file():
        raise FileNotFoundError(f"UEFI image not found: {image_path}")

    executable = shutil.which(uefiextract) if Path(uefiextract).name == uefiextract else uefiextract
    if not executable:
        raise FileNotFoundError(
            "Could not find 'uefiextract'. Pass --uefiextract with the full path to the executable."
        )

    expected_dump_dir = image_path.parent / f"{image_path.name}.dump"
    if expected_dump_dir.exists():
        shutil.rmtree(expected_dump_dir)

    if output_dir.exists():
        shutil.rmtree(output_dir)

    output_dir.parent.mkdir(parents=True, exist_ok=True)
    command = [executable, str(image_path.resolve()), "all"]
    result = subprocess.run(
        command,
        cwd=output_dir.parent,
        capture_output=True,
        text=True,
        check=False,
    )

    log_path = output_dir / "uefiextract.log"
    output_dir.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        f"$ {' '.join(command)}\n\nSTDOUT:\n{result.stdout}\n\nSTDERR:\n{result.stderr}",
        encoding="utf-8",
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"uefiextract failed with exit code {result.returncode}. See {log_path}."
        )

    if not expected_dump_dir.is_dir():
        raise RuntimeError(
            f"uefiextract reported success but did not create {expected_dump_dir}."
        )

    shutil.rmtree(output_dir)
    shutil.move(str(expected_dump_dir), str(output_dir))
    log_path = output_dir / "uefiextract.log"
    log_path.write_text(
        f"$ {' '.join(command)}\n\nSTDOUT:\n{result.stdout}\n\nSTDERR:\n{result.stderr}",
        encoding="utf-8",
    )

    return output_dir
