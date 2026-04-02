#!/usr/bin/env python3
"""
Download Material Icons SVGs and convert to 1-bit bitmap C headers.
"""
import urllib.request
import cairosvg
import io
from PIL import Image

ICONS_DIR = "src/components/icons"

# Material Icons to download: (output_filename, material_icon_name, size, var_name)
ICONS = [
    ("book.h",     "menu_book",           32, "BookIcon"),
    ("book24.h",   "menu_book",           24, "Book24Icon"),
    ("folder.h",   "folder",              32, "FolderIcon"),
    ("folder24.h", "folder",              24, "Folder24Icon"),
    ("recent.h",   "schedule",            32, "RecentIcon"),
    ("settings.h", "tune",               32, "SettingsIcon"),
    ("settings2.h","settings",            32, "Settings2Icon"),
    ("wifi.h",     "wifi",               32, "WifiIcon"),
    ("hotspot.h",  "wifi_tethering",     32, "HotspotIcon"),
    ("transfer.h", "sync",               32, "TransferIcon"),
    ("library.h",  "collections_bookmark",32, "LibraryIcon"),
    ("file24.h",   "draft",              24, "File24Icon"),
    ("text24.h",   "text_snippet",       24, "Text24Icon"),
    ("image24.h",  "photo",              24, "Image24Icon"),
    ("cog.h",      "settings",           32, "CogIcon"),
    ("cover.h",    "photo",              32, "CoverIcon"),
]

BASE_URL = "https://raw.githubusercontent.com/google/material-design-icons/master/symbols/web/{name}/materialsymbolsrounded/{name}_24px.svg"

def svg_to_bitmap_bytes(svg_data: bytes, size: int) -> list[int]:
    """Convert SVG bytes to 1-bit packed bitmap bytes.
    Renderer uses frameBuffer &= srcByte, so:
      0 bit = draw black pixel
      1 bit = transparent (keep existing)
    Therefore: dark icon pixels → 0, light background → 1
    """
    png_data = cairosvg.svg2png(bytestring=svg_data, output_width=size, output_height=size)
    img = Image.open(io.BytesIO(png_data)).convert("RGBA")

    # White background
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    bg.paste(img, mask=img.split()[3])
    img = bg.convert("L")

    # Threshold: dark pixels (icon) → 0, light pixels (background) → 1
    threshold = 128
    pixels = list(img.getdata())

    result = []
    for row in range(size):
        for col in range(0, size, 8):
            byte = 0
            for bit in range(8):
                px = pixels[row * size + col + bit] if col + bit < size else 255
                if px >= threshold:  # light/background → 1 (transparent)
                    byte |= (0x80 >> bit)
            result.append(byte)
    return result

def write_header(path: str, var_name: str, size: int, bitmap: list[int]):
    bytes_per_row = size // 8
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "",
        f"// size: {size}x{size}",
        f"static const uint8_t {var_name}[] = {{",
    ]

    row_strs = []
    for row in range(size):
        chunk = bitmap[row * bytes_per_row:(row + 1) * bytes_per_row]
        row_strs.append("    " + ", ".join(f"0x{b:02X}" for b in chunk))
    lines.append(",\n".join(row_strs))
    lines.append("};")

    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

def main():
    import os
    os.makedirs(ICONS_DIR, exist_ok=True)

    for filename, icon_name, size, var_name in ICONS:
        url = BASE_URL.format(name=icon_name)
        print(f"  {icon_name} ({size}x{size}) → {filename} ... ", end="", flush=True)
        try:
            with urllib.request.urlopen(url, timeout=10) as r:
                svg_data = r.read()
            bitmap = svg_to_bitmap_bytes(svg_data, size)
            out_path = os.path.join(ICONS_DIR, filename)
            write_header(out_path, var_name, size, bitmap)
            print("OK")
        except Exception as e:
            print(f"FAILED: {e}")

if __name__ == "__main__":
    main()
