#!/usr/bin/env python3
"""Adapt the pinned PS5 title template to PacBrew's static pkg-config libs.

The SDK's import stubs are already supplied explicitly by the template. Some
PacBrew .pc files nevertheless emit Linux compile definitions, -lpthread, and
unresolvable -lSce* names in their *link* flags. Filter only those known flags.
The template source stays pinned and unchanged outside the temporary build.
"""

import pathlib
import sys


def main() -> None:
    options = sys.argv[2:]
    if len(sys.argv) < 2 or options not in ([], ["--without-ps5-keyboard"]):
        raise SystemExit(
            "usage: patch-ps5-linker.py <template/tools/build.sh> [--without-ps5-keyboard]"
        )
    target = pathlib.Path(sys.argv[1])
    text = target.read_text(encoding="utf-8")
    marker = '    printf \'PacBrew dependencies: %s\\n\' "${pacbrew_packages[*]:-(manual archives)}"\n'
    if text.count(marker) != 1:
        raise SystemExit("pinned PS5 template changed: PacBrew link hook not found once")
    replacement = """    # PacBrew .pc metadata mixes host compile flags and console SDK -l names
    # into Libs.private. The template already links all PS5 SDK .so stubs by
    # absolute path below; passing -lSce* here instead breaks the PS5 linker.
    filtered_pacbrew_libs=()
    for flag in "${pacbrew_libs[@]}"; do
        case "$flag" in
            -D_THREAD_SAFE|-lpthread|-lSce*) ;;
            *) filtered_pacbrew_libs+=("$flag") ;;
        esac
    done
    pacbrew_libs=("${filtered_pacbrew_libs[@]}")
"""
    text = text.replace(marker, replacement + marker)
    if options:
        wrap_marker = "wrap_options=()\n"
        if text.count(wrap_marker) != 1:
            raise SystemExit("pinned PS5 template changed: linker wrap hook not found once")
        wraps = (
            "--wrap=PS5_Keyboard_Init",
            "--wrap=PS5_Keyboard_Open",
            "--wrap=PS5_Keyboard_PumpEvents",
            "--wrap=PS5_HasScreenKeyboardSupport",
            "--wrap=PS5_ShowScreenKeyboard",
            "--wrap=PS5_HideScreenKeyboard",
            "--wrap=PS5_IsScreenKeyboardShown",
        )
        text = text.replace(wrap_marker, wrap_marker + "wrap_options+=(" + " ".join(wraps) + ")\n")
    target.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
