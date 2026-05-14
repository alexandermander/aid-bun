from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from uanalyze.collect_pe import collect_pe_files
from uanalyze.decompile import decompile_pe_files
from uanalyze.extract import extract_uefi_image


def _default_paths(repo_root: Path) -> tuple[Path, Path]:
    return (
        repo_root / "opt" / "ghidra" / "support" / "analyzeHeadless",
        repo_root / "opt" / "ghidra-firmware-utils" / "ghidra_scripts",
    )


def build_parser() -> argparse.ArgumentParser:
    repo_root = Path(__file__).resolve().parent.parent
    default_ghidra_headless, default_uefi_scripts = _default_paths(repo_root)

    parser = argparse.ArgumentParser(
        description="Extract a UEFI image, collect PE payloads, and decompile them with Ghidra."
    )
    parser.add_argument("image", help="Path to the UEFI image to analyze.")
    parser.add_argument(
        "--work-dir",
        default="work",
        help="Disposable output root. Default: %(default)s",
    )
    parser.add_argument(
        "--extracted-dir",
        default="extracted",
        help="Subdirectory under work-dir for uefiextract output.",
    )
    parser.add_argument(
        "--pe-dir",
        default="only_pe_files",
        help="Subdirectory under work-dir for copied PE files.",
    )
    parser.add_argument(
        "--decompile-dir",
        default="decompiled",
        help="Subdirectory under work-dir for Ghidra decompilation output.",
    )
    parser.add_argument(
        "--project-dir",
        default="ghidra_projects",
        help="Subdirectory under work-dir for temporary Ghidra projects.",
    )
    parser.add_argument(
        "--uefiextract",
        default="uefiextract",
        help="Path to the uefiextract executable.",
    )
    parser.add_argument(
        "--ghidra-headless",
        default=str(default_ghidra_headless),
        help=(
            "Path to Ghidra's analyzeHeadless launcher. "
            f"Default: %(default)s"
        ),
    )
    parser.add_argument(
        "--uefi-scripts",
        default=str(default_uefi_scripts),
        help=(
            "Path to the ghidra-firmware-utils ghidra_scripts directory. "
            f"Default: %(default)s"
        ),
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=max(1, (os.cpu_count() or 2) // 2),
        help="Parallel Ghidra jobs. Default: half CPU count.",
    )
    parser.add_argument(
        "--min-pe-size",
        type=int,
        default=8 * 1024,
        help="Minimum size in bytes for candidate body.bin files.",
    )
    parser.add_argument(
        "--skip-decompile",
        action="store_true",
        help="Run extraction and PE collection only.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    default_ghidra_headless, default_uefi_scripts = _default_paths(repo_root)
    image_path = Path(args.image).resolve()
    work_dir = (repo_root / args.work_dir).resolve()
    extracted_dir = work_dir / args.extracted_dir
    pe_dir = work_dir / args.pe_dir
    decompile_dir = work_dir / args.decompile_dir
    project_dir = work_dir / args.project_dir
    export_script = repo_root / "ExportDecompiled.java"

    if not export_script.is_file():
        raise FileNotFoundError(f"Missing ExportDecompiled.java at {export_script}")

    print(f"[1/3] Extracting firmware into {extracted_dir}")
    extract_uefi_image(image_path, extracted_dir, args.uefiextract)

    print(f"[2/3] Collecting PE files into {pe_dir}")
    collected = collect_pe_files(
        extracted_dir,
        pe_dir,
        min_size_bytes=args.min_pe_size,
    )
    if not collected:
        print("No PE files found after extraction.")
        return 1

    print(f"Collected {len(collected)} PE file(s).")
    if args.skip_decompile:
        print("Skipping decompilation.")
        return 0

    ghidra_headless = Path(args.ghidra_headless).resolve()
    uefi_scripts = Path(args.uefi_scripts).resolve()

    if not ghidra_headless.is_file():
        if ghidra_headless == default_ghidra_headless.resolve():
            raise FileNotFoundError(
                "Default Ghidra headless launcher not found. "
                "Run ./setup_ghidra.sh or pass --ghidra-headless explicitly."
            )
        raise FileNotFoundError(f"Ghidra headless launcher not found: {ghidra_headless}")

    if not uefi_scripts.is_dir():
        if uefi_scripts == default_uefi_scripts.resolve():
            raise FileNotFoundError(
                "Default ghidra-firmware-utils script directory not found. "
                "Run ./setup_ghidra.sh or pass --uefi-scripts explicitly."
            )
        raise FileNotFoundError(f"Ghidra script path not found: {uefi_scripts}")

    print(f"[3/3] Decompiling PE files into {decompile_dir}")
    results = decompile_pe_files(
        [item.copied_to for item in collected],
        decompile_dir,
        project_dir,
        ghidra_headless,
        [repo_root, uefi_scripts],
        jobs=args.jobs,
    )

    failures = 0
    for result in sorted(results, key=lambda item: item.input_file.name):
        status = "OK" if result.return_code == 0 else f"FAIL({result.return_code})"
        print(f"{status} {result.input_file.name} ({result.elapsed_seconds:.1f}s)")
        if result.return_code != 0:
            failures += 1

    if failures:
        print(f"Completed with {failures} failure(s).")
        return 1

    print("Completed successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
