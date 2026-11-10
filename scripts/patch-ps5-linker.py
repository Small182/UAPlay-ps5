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
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch-ps5-linker.py <template/tools/build.sh>")
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
    target.write_text(text.replace(marker, replacement + marker), encoding="utf-8")


if __name__ == "__main__":
    main()
