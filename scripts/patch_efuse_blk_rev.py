"""
Post-build script: zero out min_efuse_blk_rev in firmware.bin.

ESP-IDF pre-compiled libraries on Linux (GitHub Actions) set min_efuse_blk_rev_major
to non-zero values in esp_app_desc_t, causing OTA validation to fail on devices
with older eFuse block revisions (e.g. v1.3). Patching to 0 makes the firmware
compatible with all ESP32-C3 chip revisions.

The esp_app_desc_t starts with magic 0xABCD5432 (LE: 0x32 0x54 0xCD 0xAB).
min_efuse_blk_rev fields are the two uint16_t values immediately following
the 64-byte reserv2 block after the SHA256 hash (offset 176 from magic).
"""

import os

Import("env")  # noqa: F821 — PlatformIO SCons global


def patch_efuse_blk_rev(source, target, env):  # noqa: ARG001
    firmware_path = str(target[0])
    if not firmware_path.endswith(".bin"):
        # Try the .bin alongside the ELF
        firmware_path = firmware_path.replace(".elf", ".bin")

    if not os.path.isfile(firmware_path):
        print(f"patch_efuse_blk_rev: {firmware_path} not found, skipping")
        return

    with open(firmware_path, "rb") as f:
        data = bytearray(f.read())

    # esp_app_desc_t magic (little-endian 0xABCD5432)
    magic = bytes([0x32, 0x54, 0xCD, 0xAB])
    idx = data.find(magic)
    if idx == -1:
        print("patch_efuse_blk_rev: esp_app_desc magic not found, skipping")
        return

    # Offset 176 from magic = min_efuse_blk_rev_major (uint16_t, 2 bytes)
    # Offset 178 from magic = min_efuse_blk_rev_minor (uint16_t, 2 bytes)
    maj_off = idx + 176
    min_off = idx + 178

    orig_maj = int.from_bytes(data[maj_off: maj_off + 2], "little")
    orig_min = int.from_bytes(data[min_off: min_off + 2], "little")

    if orig_maj == 0 and orig_min == 0:
        print(f"patch_efuse_blk_rev: already 0.0, no patch needed ({firmware_path})")
        return

    data[maj_off] = 0
    data[maj_off + 1] = 0
    data[min_off] = 0
    data[min_off + 1] = 0

    with open(firmware_path, "wb") as f:
        f.write(data)

    print(
        f"patch_efuse_blk_rev: patched v{orig_maj}.{orig_min} → 0.0 in {firmware_path}"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", patch_efuse_blk_rev)
