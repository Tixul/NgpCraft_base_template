#!/usr/bin/env python3
"""
apply_live_zip.py — drop a NgpCraft Live Editor ZIP into this base template
and patch the makefile so any extra user .c files get built and linked.

Usage:
    py -3 tools/apply_live_zip.py path/to/ngpcraft-project-<timestamp>.zip
    py -3 tools/apply_live_zip.py path/to/zip --dry-run

What it does:
  1. Validates the ZIP and lists its entries.
  2. Copies each `src/<name>.c|h` from the ZIP into `<project>/src/`,
     overwriting existing files. Nothing outside `src/` is touched.
  3. Rewrites the marker block in `makefile`:
       # >>> NGPC_LIVE_EDITOR_OBJS auto (do not edit by hand)
       OBJS += $(OBJ_DIR)/src/<user_source>.rel
       ...
       # <<< NGPC_LIVE_EDITOR_OBJS auto
     covering every top-level `src/*.c` except `src/main.c` (which the base
     OBJS list already includes).
  4. Idempotent: re-running replaces the previous marker block in place.

Safety:
  - Refuses ZIP entries whose path escapes `src/` (no `..`, no absolute,
    no backslash; must match `src/<name>.[ch]`).
  - Backs up the makefile to `makefile.bak` before any write.
  - `--dry-run` prints the plan without touching disk.

Limitations:
  - Only top-level `src/*.c` are auto-added. Hand-written sources in
    `src/<subdir>/` must still be added to the base OBJS list manually.
"""

import argparse
import re
import shutil
import sys
import zipfile
from pathlib import Path

BLOCK_BEGIN = "# >>> NGPC_LIVE_EDITOR_OBJS auto (do not edit by hand)"
BLOCK_END   = "# <<< NGPC_LIVE_EDITOR_OBJS auto"

# Anchor on first-run injection. Closing line of the base OBJS = \ block in
# the stock makefile. Everything past it already uses `OBJS += ...`, which
# is also how we add our entries.
ANCHOR = "$(OBJ_DIR)/src/core/ngpc_syspatch.rel"

SRC_ENTRY_RE = re.compile(r"^src/[A-Za-z_][\w-]*\.[ch]$")


def validate_entry_name(name: str) -> bool:
    if "\\" in name or name.startswith("/") or ".." in name.split("/"):
        return False
    return bool(SRC_ENTRY_RE.match(name))


def collect_zip_entries(zip_path: Path):
    out = []
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            name = info.filename.replace("\\", "/")
            if not validate_entry_name(name):
                print(f"  skip (rejected path): {info.filename}")
                continue
            out.append((name, zf.read(info)))
    return out


def write_src_files(entries, project_root: Path, dry_run: bool):
    (project_root / "src").mkdir(parents=True, exist_ok=True)
    for name, data in entries:
        dest = project_root / name
        verb = "overwrite" if dest.exists() else "create   "
        if dry_run:
            print(f"  {verb} {name} ({len(data)} bytes)")
        else:
            dest.write_bytes(data)
            print(f"  {verb} {name} ({len(data)} bytes)")


def discover_user_sources(project_root: Path):
    out = []
    for p in sorted((project_root / "src").glob("*.c")):
        if p.name == "main.c":
            continue
        out.append(p.relative_to(project_root).as_posix())
    return out


def build_block(user_sources) -> str:
    lines = [BLOCK_BEGIN]
    if user_sources:
        for s in user_sources:
            rel = s[:-2] + ".rel"          # src/foo.c -> src/foo.rel
            lines.append(f"OBJS += $(OBJ_DIR)/{rel}")
    else:
        lines.append("# (no extra user sources)")
    lines.append(BLOCK_END)
    return "\n".join(lines) + "\n"


def patch_makefile(project_root: Path, user_sources, dry_run: bool) -> bool:
    mk = project_root / "makefile"
    text = mk.read_text()
    block = build_block(user_sources)

    existing = re.compile(
        re.escape(BLOCK_BEGIN) + r".*?" + re.escape(BLOCK_END) + r"\n?",
        re.DOTALL,
    )
    if existing.search(text):
        new_text = existing.sub(block, text)
    else:
        if ANCHOR not in text:
            raise SystemExit(
                f"Cannot find anchor line in makefile:\n  {ANCHOR}\n"
                "The base template may have changed — adjust ANCHOR in this script."
            )
        lines = text.splitlines(keepends=True)
        for i, line in enumerate(lines):
            if ANCHOR in line:
                lines.insert(i + 1, "\n" + block)
                break
        new_text = "".join(lines)

    if new_text == text:
        print("makefile already up to date — no changes.")
        return False

    if dry_run:
        print("makefile would be patched (use without --dry-run to apply).")
        return True

    shutil.copy2(mk, mk.with_suffix(".bak"))
    mk.write_text(new_text)
    print(f"makefile patched (backup at {mk.with_suffix('.bak').name}).")
    return True


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("zip", type=Path, help="Path to ngpcraft-project-*.zip")
    ap.add_argument(
        "--project-root", type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Project root (default: parent of tools/)",
    )
    ap.add_argument("--dry-run", action="store_true",
                    help="Print plan without writing")
    args = ap.parse_args()

    if not args.zip.is_file():
        sys.exit(f"ZIP not found: {args.zip}")
    if not (args.project_root / "makefile").is_file():
        sys.exit(f"Not a base template (no makefile): {args.project_root}")

    print(f"Project: {args.project_root}")
    print(f"ZIP:     {args.zip}")
    print()
    print("ZIP entries:")
    entries = collect_zip_entries(args.zip)
    if not entries:
        sys.exit("No valid src/*.c|h entries in the ZIP.")
    for name, data in entries:
        print(f"  {name} ({len(data)} bytes)")
    print()
    print("Extracting:")
    write_src_files(entries, args.project_root, args.dry_run)

    print()
    print("Patching makefile:")
    user_sources = discover_user_sources(args.project_root)
    if user_sources:
        print(f"  user sources to add to OBJS: {len(user_sources)}")
        for s in user_sources:
            print(f"    {s}")
    else:
        print("  no extra user sources (base OBJS already covers src/main.c)")
    patch_makefile(args.project_root, user_sources, args.dry_run)

    if not args.dry_run:
        print()
        print("Done. Build with: make")


if __name__ == "__main__":
    main()
