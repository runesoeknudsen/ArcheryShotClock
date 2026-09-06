"""Embeds the web pages into the firmware, gzipped.

The pages are served straight from flash rather than copied into a String
first. A 20 KB String allocation on a board that is also running a Wi-Fi
access point is exactly the kind of request that fails under memory pressure,
and a page that arrives truncated looks like a broken UI rather than an
out-of-memory error - the tail of the document, which is where the script
lives, simply never arrives.

Gzipping also cuts the transfer from about 20 KB to under 6 KB, which matters
on a busy 2.4 GHz field.
"""

import gzip
from pathlib import Path
import sys

Import("env")

if env.subst("$PIOENV") == "native" and sys.platform == "win32":
    toolchain = Path("C:/msys64/ucrt64/bin")
    env.PrependENVPath("PATH", str(toolchain))

project_dir = Path(env.subst("$PROJECT_DIR"))
if project_dir.name == "firmware":
    software_dir = project_dir.parent
    firmware_dir = project_dir
else:
    software_dir = project_dir / "software"
    firmware_dir = software_dir / "firmware"


def embed(source: Path, target: Path, symbol: str) -> None:
    html = source.read_text(encoding="utf-8")
    operator = software_dir / "web" / "demo" / "operator.js"
    marker = '<script src="./demo/operator.js"></script>'
    if marker in html:
        html = html.replace(
            marker,
            "<script>\n" + operator.read_text(encoding="utf-8") + "\n</script>",
        )
    raw = html.encode("utf-8")
    # mtime=0 keeps the output byte-identical between builds, so an unchanged
    # page does not force a rebuild or show up as a diff.
    packed = gzip.compress(raw, compresslevel=9, mtime=0)

    lines = []
    for offset in range(0, len(packed), 16):
        chunk = packed[offset : offset + 16]
        lines.append("  " + " ".join(f"0x{byte:02x}," for byte in chunk))

    target.write_text(
        "#pragma once\n\n"
        "#include <pgmspace.h>\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        f"// Generated from {source.name} by software/tools/embed_web.py. Do not edit.\n"
        f"// {len(raw)} bytes of HTML, {len(packed)} bytes gzipped.\n"
        f"const uint8_t {symbol}[] PROGMEM = {{\n" + "\n".join(lines) + "\n};\n\n"
        f"const size_t {symbol}_LEN = {len(packed)};\n",
        encoding="utf-8",
    )
    print(f"embed_web: {source.name} {len(raw)} -> {len(packed)} bytes gzipped")


embed(software_dir / "web" / "index.html", firmware_dir / "src" / "web_page.h", "PAGE_GZ")
