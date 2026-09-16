# Headless regression checks

Run `make check` after configuring `config.h`. The checks use two headless
outputs and software rendering in a private runtime directory under
`~/codex-tmp`. They do not connect to the active desktop.

Requirements: the normal build dependencies, GCC or Clang with AddressSanitizer
and UndefinedBehaviorSanitizer, Python 3, `foot`, `grim`, and XWayland when enabled.

The test includes the compositor source to inspect state while real Wayland
terminals and an X11 client connect. It checks bar clicks, keybinding definitions,
per-tag settings, status input, smart borders, fullscreen and floating layouts,
monitor navigation, XWayland popup stacking, screenshot capture, and teardown.
It uses the standard three layouts and expects two outputs with a visible bar.

The sanitizer checks stop on invalid memory accesses or undefined behavior.
Leak detection is disabled because the process includes system libraries with
process-lifetime allocations. Hardware display handling, GPU rendering, full
IME interaction, and portal-based screen sharing still need a desktop session.
