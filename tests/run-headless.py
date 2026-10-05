#!/usr/bin/env python3
"""Run make check without using the active desktop or its runtime directory."""
import os
from pathlib import Path
import signal
import shutil
import subprocess
import tempfile

from PIL import Image

scratch = Path.home() / "codex-tmp"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="dwl-check-", dir=scratch) as directory:
    runtime = Path(directory)
    env = dict(os.environ, WLR_BACKENDS="headless", WLR_HEADLESS_OUTPUTS="2",
               WLR_RENDERER="pixman", XDG_RUNTIME_DIR=directory,
               DWL_TEST_SCREENSHOT=str(runtime / "capture.png"),
               DWL_TEST_TILE_SCREENSHOT=str(runtime / "tile.png"),
               ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    for key in ("WAYLAND_DISPLAY", "DISPLAY"):
        env.pop(key, None)
    with (runtime / "session.log").open("w+") as log:
        process = subprocess.Popen(["tests/headless"], env=env, stdin=subprocess.PIPE,
                                   stdout=log, stderr=log, start_new_session=True)
        try:
            process.wait(timeout=45)
            log.seek(0)
            output = log.read()
            print(output, end="")
            if process.returncode:
                raise SystemExit(f"Headless tests failed: {process.returncode}")
            capture = runtime / "capture.png"
            assert capture.read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
            print("PASS PNG screenshot produced")
            geometry = next(line for line in output.splitlines() if line.startswith("TILE_CAPTURE "))
            fields = geometry.split()
            x, y, width, height, divider_x, divider_y, gap = map(int, fields[1:8])
            selected, normal = (tuple((int(value, 16) >> shift) & 255 for shift in (24, 16, 8))
                                for value in fields[8:10])
            with Image.open(runtime / "tile.png") as capture:
                pixels = capture.convert("RGB")
                background = pixels.getpixel((x + width // 4, y + height // 4))
                for point in ((x, y + height // 4), (x + width // 4, y),
                              (x + width - 1, y + height // 4),
                              (x + width // 4, y + height - 1)):
                    assert pixels.getpixel(point) == background, f"Outer border at {point}"
                for dx in range(gap):
                    for py in range(y, y + height):
                        assert pixels.getpixel((divider_x + dx, py)) == selected
                for dy in range(gap):
                    for px in range(divider_x + gap, x + width):
                        assert pixels.getpixel((px, divider_y + dy)) == normal
                print("PASS screenshot pixels: no outer frames, exact separator widths, continuous junction")
            if destination := os.environ.get("DWL_TEST_ARTIFACT_DIR"):
                Path(destination).mkdir(parents=True, exist_ok=True)
                shutil.copy2(runtime / "tile.png", Path(destination) / "inner-borders.png")
        finally:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
