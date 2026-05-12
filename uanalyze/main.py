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


def build_parser() -> argparse.ArgumentParser:
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
        help="Path to Ghidra's analyzeHeadless launcher. Required unless --skip-decompile is used.",
    )
    parser.add_argument(
        "--uefi-scripts",
        help="Path to the ghidra-firmware-utils ghidra_scripts directory.",
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

    if not args.ghidra_headless or not args.uefi_scripts:
        raise ValueError(
            "--ghidra-headless and --uefi-scripts are required unless --skip-decompile is used."
        )

    print(f"[3/3] Decompiling PE files into {decompile_dir}")
    results = decompile_pe_files(
        [item.copied_to for item in collected],
        decompile_dir,
        project_dir,
        Path(args.ghidra_headless),
        [repo_root, Path(args.uefi_scripts)],
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
