#!/usr/bin/env bash
# Create a ShadowMountPlus-compatible UFS2 image from the complete PS5 title.
# The native build must already have copied its romfs and runtime modules.
set -euo pipefail

: "${PS5_NATIVE_TEMPLATE:?Point to the pinned PS5 title template}"
: "${PS5_NATIVE_OUTPUT:?Point to the native title output directory}"

title="$PS5_NATIVE_OUTPUT/PPSA99282"
image="$PS5_NATIVE_OUTPUT/PPSA99282.ffpkg"
for required in "$title/eboot.bin" "$title/sce_sys/param.json" \
                "$title/sce_module/libc.prx" "$title/romfs/InterVariable.ttf"; do
    if [[ ! -s $required ]]; then
        printf 'Cannot package: required native title file missing: %s\n' "$required" >&2
        exit 2
    fi
done

# Title files are copied from the build stage and mounted read-only.
chmod a+rX "$title"
chmod a+rx "$title/eboot.bin"

ufs2tool=$(bash "$PS5_NATIVE_TEMPLATE/tools/setup-packaging-dependencies.sh" ffpkg)
"$ufs2tool" makefs -S 4096 -b 20% -t ffs \
    -o version=2,bsize=32768,fsize=4096,minfree=0,softupdates=0,optimization=space \
    "$image" "$title"

python3 - "$image" <<'PY'
import os
import struct
import sys

image = sys.argv[1]
if os.path.getsize(image) < 1024 * 1024:
    raise SystemExit("FFPKG image unexpectedly small")
with open(image, "rb") as stream:
    stream.seek(0x1055c)
    magic = struct.unpack("<I", stream.read(4))[0]
if magic != 0x19540119:
    raise SystemExit(f"Invalid UFS2 superblock magic: {magic:#x}")
print(f"Verified UFS2 FFPKG: {image} ({os.path.getsize(image)} bytes)")
PY
