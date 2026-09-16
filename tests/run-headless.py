#!/usr/bin/env python3
"""Run make check without using the active desktop or its runtime directory."""
import os
from pathlib import Path
import signal
import subprocess
import tempfile

scratch = Path.home() / "codex-tmp"
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="dwl-check-", dir=scratch) as directory:
    runtime = Path(directory)
    env = dict(os.environ, WLR_BACKENDS="headless", WLR_HEADLESS_OUTPUTS="2",
               WLR_RENDERER="pixman", XDG_RUNTIME_DIR=directory,
               DWL_TEST_SCREENSHOT=str(runtime / "capture.png"),
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
            print(log.read(), end="")
            if process.returncode:
                raise SystemExit(f"Headless tests failed: {process.returncode}")
            capture = runtime / "capture.png"
            assert capture.read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
            print("PASS PNG screenshot produced")
        finally:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
