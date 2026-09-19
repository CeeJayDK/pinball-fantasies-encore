#!/usr/bin/env python3
"""Upscales every PNG under a folder (or a single PNG) with Lanczos and a mild unsharp mask.

    upscale.py <folder-or-image.png> [scale]

Each result is saved next to its source as <name>_<scale>x.png. Files that are already
upscaled results (ending in _<n>x) are skipped, so the script can be run again safely.

Needs Pillow, from the tools virtual environment, which the script switches to by itself.
Set it up once with:
    python3 -m venv tools/.venv && tools/.venv/bin/pip install -r tools/requirements.txt
"""
import os
import re
import sys
from pathlib import Path

try:
    from PIL import Image, ImageFilter
except ModuleNotFoundError:
    # Not running inside the tools virtual environment: restart with its Python.
    venv = Path(__file__).resolve().parent / ".venv"
    venv_python = venv / "bin" / "python"
    if venv_python.exists() and Path(sys.prefix).resolve() != venv.resolve():
        os.execv(str(venv_python), [str(venv_python), *sys.argv])
    sys.exit("Pillow is missing. Set up the tools environment:\n"
             "  python3 -m venv tools/.venv && tools/.venv/bin/pip install -r tools/requirements.txt")

UPSCALED = re.compile(r"_\d+x$")


def upscale(path: Path, scale: int) -> Path:
    img = Image.open(path).convert("RGBA")
    # High-quality non-generative upscale.
    img = img.resize((img.width * scale, img.height * scale), Image.Resampling.LANCZOS)
    # Mild sharpening to restore edge definition lost during interpolation.
    img = img.filter(ImageFilter.UnsharpMask(radius=1.0, percent=80, threshold=2))
    out = path.with_name(f"{path.stem}_{scale}x{path.suffix}")
    img.save(out, "PNG", optimize=True)
    return out


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <folder-or-image.png> [scale]")
        sys.exit(1)
    target = Path(sys.argv[1]).expanduser()
    scale = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    files = [target] if target.is_file() else sorted(target.rglob("*.png"))
    files = [f for f in files if not UPSCALED.search(f.stem)]
    for f in files:
        out = upscale(f, scale)
        print(f"{f.relative_to(target.parent)} -> {out.name}")
    print(f"upscaled {len(files)} images {scale}x")


if __name__ == "__main__":
    main()
