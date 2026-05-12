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

    output_dir.mkdir(parents=True, exist_ok=True)
    command = [executable, str(image_path.resolve()), "all"]
    result = subprocess.run(
        command,
        cwd=output_dir,
        capture_output=True,
        text=True,
        check=False,
    )

    log_path = output_dir / "uefiextract.log"
    log_path.write_text(
        f"$ {' '.join(command)}\n\nSTDOUT:\n{result.stdout}\n\nSTDERR:\n{result.stderr}",
        encoding="utf-8",
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"uefiextract failed with exit code {result.returncode}. See {log_path}."
        )

    return output_dir

