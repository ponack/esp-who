#!/usr/bin/env python3
"""Generate an LVGL font subset for the PP-OCRv6 example overlay.

Extracts every character in `pp_ocr_v6_dict.hpp` plus printable ASCII, and
drives `lv_font_conv` to emit `main/assets/pp_ocr_v6_cjk_<size>.c`.

    python3 tools/gen_cjk_font.py <ttf> --size 24 --bpp 2

`--bpp 2` is required — 4 bpp doubles the source file and pushes the
rodata past the OCR overlay's PSRAM headroom guard.

Recommended font: Noto Sans CJK SC (SIL OFL). Prerequisites: `lv_font_conv`
via npm (or `npx`) and a CJK TTF/OTF/TTC file. For `.ttc` collections the
script transparently extracts the selected face via `fonttools`.

After generating, enable it via `idf.py menuconfig` → **PP-OCRv6 example →
Include CJK LVGL font for the on-screen overlay** and rebuild.
"""
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
EXAMPLE_DIR = THIS_DIR.parent


def _resolve_default_dict() -> Path:
    """Locate `pp_ocr_v6_dict.hpp` in `managed_components/` (registry
    download) or an override `esp-dl/` sibling checkout. Falls back to the
    primary path so the error message points there."""
    candidates = [
        EXAMPLE_DIR
        / "managed_components"
        / "espressif__pp_ocr_v6"
        / "pp_ocr_v6_dict.hpp",
        EXAMPLE_DIR / "../../../../esp-dl/models/pp_ocr_v6/pp_ocr_v6_dict.hpp",
    ]
    for p in candidates:
        resolved = p.resolve()
        if resolved.exists():
            return resolved
    return candidates[0].resolve()


DEFAULT_DICT = _resolve_default_dict()
DEFAULT_SIZE = 24
DEFAULT_BPP = 2
DEFAULT_ASSETS_DIR = EXAMPLE_DIR / "main" / "assets"


def default_output_for_size(size: int) -> Path:
    return DEFAULT_ASSETS_DIR / f"pp_ocr_v6_cjk_{size}.c"


def default_symbol_for_size(size: int) -> str:
    return f"pp_ocr_v6_cjk_{size}"


STRING_LITERAL_RE = re.compile(rb'"((?:[^"\\]|\\.)*)"')
HEX_ESCAPE_RE = re.compile(rb"\\x([0-9a-fA-F]{2})")


def decode_literal(literal: bytes) -> str:
    """Decode a C string literal from the dict header (contains `\\xNN`
    escapes and plain ASCII bytes) to UTF-8."""
    buf = bytearray()
    i = 0
    while i < len(literal):
        if literal[i : i + 2] == b"\\x":
            buf.append(int(literal[i + 2 : i + 4], 16))
            i += 4
        elif literal[i : i + 1] == b"\\":
            buf.append(literal[i + 1])
            i += 2
        else:
            buf.append(literal[i])
            i += 1
    try:
        return buf.decode("utf-8")
    except UnicodeDecodeError:
        return ""


def extract_charset(dict_path: Path) -> set[str]:
    data = dict_path.read_bytes()
    chars: set[str] = set()
    for m in STRING_LITERAL_RE.finditer(data):
        s = decode_literal(m.group(1))
        for ch in s:
            chars.add(ch)
    chars.discard("")
    return chars


def extract_ttf_from_ttc(ttc_path: Path, face_index: int, out_path: Path) -> None:
    """lv_font_conv can't parse .ttc; extract `face_index` to a temp .ttf."""
    try:
        from fontTools.ttLib import TTCollection
    except ImportError:
        sys.exit(
            "error: reading .ttc requires the `fonttools` python package.\n"
            "Install it with `pip install fonttools`, or pass a plain .ttf/.otf."
        )
    ttc = TTCollection(str(ttc_path))
    if face_index < 0 or face_index >= len(ttc.fonts):
        faces = "\n  ".join(
            f"{i}: {f['name'].getBestFullName()}" for i, f in enumerate(ttc.fonts)
        )
        sys.exit(
            f"error: --face-index {face_index} is out of range for {ttc_path} "
            f"(has {len(ttc.fonts)} faces).\nAvailable faces:\n  {faces}"
        )
    ttc.fonts[face_index].save(str(out_path))


def find_lv_font_conv() -> list[str]:
    if shutil.which("lv_font_conv"):
        return ["lv_font_conv"]
    if shutil.which("npx"):
        return ["npx", "--yes", "lv_font_conv"]
    sys.exit(
        "error: lv_font_conv not found on PATH.\n"
        "Install with `npm install -g lv_font_conv`, or make sure `npx` is available."
    )


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("ttf", help="Path to a CJK TTF/OTF (or TTC; use --face-index).")
    ap.add_argument(
        "--face-index",
        type=int,
        default=2,
        help="Face index inside a .ttc collection (default 2, which "
        "picks 'Noto Sans CJK SC' in NotoSansCJK-Regular.ttc; "
        "ignored for plain .ttf/.otf).",
    )
    ap.add_argument(
        "--dict",
        default=str(DEFAULT_DICT),
        help=f"pp_ocr_v6 dict header (default: {DEFAULT_DICT})",
    )
    ap.add_argument(
        "--size",
        type=int,
        default=DEFAULT_SIZE,
        help=f"Font pixel size (default: {DEFAULT_SIZE}). Non-24 "
        "requires renaming the LVGL symbol in main/CMakeLists.txt.",
    )
    ap.add_argument(
        "--bpp",
        type=int,
        default=DEFAULT_BPP,
        help=f"Bits per pixel (1/2/4/8; default: {DEFAULT_BPP}, "
        "required — see module docstring).",
    )
    ap.add_argument(
        "--font-name",
        default=None,
        help="LVGL symbol name (default: `pp_ocr_v6_cjk_<size>`).",
    )
    ap.add_argument(
        "--output",
        default=None,
        help="Output .c path (default: main/assets/pp_ocr_v6_cjk_<size>.c).",
    )
    args = ap.parse_args()

    if args.font_name is None:
        args.font_name = default_symbol_for_size(args.size)
    if args.output is None:
        args.output = str(default_output_for_size(args.size))

    dict_path = Path(args.dict).resolve()
    ttf_path = Path(args.ttf).resolve()
    out_path = Path(args.output).resolve()

    if not dict_path.is_file():
        sys.exit(f"error: dict header not found: {dict_path}")
    if not ttf_path.is_file():
        sys.exit(f"error: ttf not found: {ttf_path}")

    charset = extract_charset(dict_path)
    # Always keep printable ASCII so Latin/digit overlays survive dict changes.
    for cp in range(0x20, 0x7F):
        charset.add(chr(cp))
    charset_str = "".join(sorted(charset))
    print(
        f"[gen_cjk_font] extracted {len(charset)} unique characters from {dict_path.name}"
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)

    tmp_ttf_dir = None
    font_for_lvfc = ttf_path
    if ttf_path.suffix.lower() == ".ttc":
        tmp_ttf_dir = tempfile.mkdtemp(prefix="gen_cjk_font_")
        font_for_lvfc = Path(tmp_ttf_dir) / f"{ttf_path.stem}.face{args.face_index}.ttf"
        print(
            f"[gen_cjk_font] extracting face {args.face_index} from {ttf_path.name} -> {font_for_lvfc}"
        )
        extract_ttf_from_ttc(ttf_path, args.face_index, font_for_lvfc)

    cmd = find_lv_font_conv() + [
        "--font",
        str(font_for_lvfc),
        "--size",
        str(args.size),
        "--bpp",
        str(args.bpp),
        "--format",
        "lvgl",
        "--lv-font-name",
        args.font_name,
        "--symbols",
        charset_str,
        "--no-compress",
        "-o",
        str(out_path),
    ]
    printable = (
        cmd[: cmd.index("--symbols") + 1]
        + [f"<{len(charset_str)} chars>"]
        + cmd[cmd.index("-o") :]
    )
    print("[gen_cjk_font] running:", " ".join(printable))
    try:
        subprocess.check_call(cmd)
    finally:
        if tmp_ttf_dir:
            shutil.rmtree(tmp_ttf_dir, ignore_errors=True)

    size_kb = out_path.stat().st_size // 1024
    print(f"[gen_cjk_font] wrote {out_path} ({size_kb} KB source)")
    print("[gen_cjk_font] next steps:")
    print("    idf.py menuconfig    # PP-OCRv6 example → enable CJK LVGL font")
    print("    idf.py build flash monitor")


if __name__ == "__main__":
    main()
