"""
PlatformIO post-build script: generate a full-flash recovery image.

The normal firmware.bin is only the application image for 0x10000. On devices
using dual OTA slots, flashing app-only binaries can be confusing if the
bootloader is currently pointed at the other slot. This merged image bundles
bootloader + partitions + boot_app0 + firmware so it can be flashed once at
0x0000 as a deterministic recovery/update image.
"""

from pathlib import Path
import subprocess
import sys


def build_recovery(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    firmware = build_dir / "firmware.bin"
    recovery = build_dir / "recovery.bin"

    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    if framework_dir is None:
        print("WARNING [build_recovery_image.py]: framework package not found; skipping recovery image",
              file=sys.stderr)
        return

    boot_app0 = Path(framework_dir) / "tools" / "partitions" / "boot_app0.bin"
    required = (bootloader, partitions, firmware, boot_app0)
    missing = [str(path) for path in required if not path.exists()]
    if missing:
      print(
          "WARNING [build_recovery_image.py]: missing build artifacts; skipping recovery image: "
          + ", ".join(missing),
          file=sys.stderr,
      )
      return

    cmd = [
        env.subst("$PYTHONEXE"),
        "-m",
        "esptool",
        "--chip",
        "esp32c3",
        "merge-bin",
        "-o",
        str(recovery),
        "--flash-mode",
        "dio",
        "--flash-freq",
        "80m",
        "--flash-size",
        "16MB",
        "0x0000",
        str(bootloader),
        "0x8000",
        str(partitions),
        "0xe000",
        str(boot_app0),
        "0x10000",
        str(firmware),
    ]

    subprocess.run(cmd, check=True)
    print(f"Generated recovery image: {recovery}")


Import("env")
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", build_recovery)
