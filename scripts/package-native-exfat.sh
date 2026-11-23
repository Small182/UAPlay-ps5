#!/usr/bin/env bash
# Create and verify a native PS5 title exFAT image for ShadowMountPlus.
# Source layout MUST have eboot.bin and sce_sys/ at the image filesystem root.
# Build tool only; image launch remains unverified until tested on a PS5.
set -euo pipefail

: "${PS5_NATIVE_OUTPUT:?Set PS5_NATIVE_OUTPUT to an existing title output directory}"
title_id="${PS5_TITLE_ID:-PPSA99283}"
[[ "$title_id" =~ ^PPSA[0-9]{5}$ ]] || { echo "Invalid PS5 title ID: $title_id" >&2; exit 2; }

source_dir="$PS5_NATIVE_OUTPUT/$title_id"
image="$PS5_NATIVE_OUTPUT/$title_id.exfat"
[[ -d "$source_dir" ]] || { echo "Missing title folder: $source_dir" >&2; exit 2; }
[[ ! -e "$image" ]] || { echo "Refusing to overwrite existing image: $image" >&2; exit 2; }
for path in eboot.bin sce_sys/param.json sce_module/libc.prx romfs/InterVariable.ttf; do
    [[ -s "$source_dir/$path" ]] || { echo "Missing or empty title file: $path" >&2; exit 2; }
done
if find "$source_dir" -type l -print -quit | grep -q .; then
    echo "Title folder contains symlinks; exFAT cannot represent them safely" >&2
    exit 2
fi

for binary in python3 truncate mkfs.exfat mountpoint rsync cmp; do
    command -v "$binary" >/dev/null || { echo "Missing dependency: $binary" >&2; exit 2; }
done

# Account for every 64 KiB allocation block, directory entries, FAT, and 96 MiB
# headroom. ShadowMountPlus requires a fixed 64 KiB exFAT cluster.
size_mb=$(python3 - "$source_dir" <<'PY'
from pathlib import Path
import os
import sys

root = Path(sys.argv[1])
cluster = 65536
allocated = 0
files = 0
directories = 0
for base, folders, names in os.walk(root):
    directories += 1
    for name in names:
        path = Path(base) / name
        size = path.stat().st_size
        allocated += max(1, (size + cluster - 1) // cluster) * cluster
        files += 1
if not files:
    raise SystemExit("Title folder has no files")
metadata = (files + directories) * 1024 + directories * cluster
fat = (allocated // cluster + 65536) * 5
total = allocated + metadata + fat + 96 * 1024 * 1024
mb = max(128, (total + 1024 * 1024 - 1) // (1024 * 1024))
print(mb)
PY
)

temporary=$(mktemp "$PS5_NATIVE_OUTPUT/.$title_id.XXXXXXXX.exfat")
mount_dir=$(mktemp -d)
cleanup() {
    if mountpoint -q "$mount_dir"; then
        sudo umount "$mount_dir" || true
    fi
    rmdir "$mount_dir" 2>/dev/null || true
    rm -f -- "$temporary"
}
trap cleanup EXIT

truncate -s "${size_mb}M" "$temporary"
mkfs.exfat -c 64K "$temporary" >/dev/null
uid=$(id -u)
gid=$(id -g)

# GitHub-hosted Ubuntu normally provides a kernel exFAT mount; FUSE is the
# fallback. Never copy into an unmounted directory.
if ! sudo mount -t exfat -o "loop,uid=$uid,gid=$gid" "$temporary" "$mount_dir"; then
    sudo mount -t exfat-fuse -o "loop,uid=$uid,gid=$gid" "$temporary" "$mount_dir"
fi
mountpoint -q "$mount_dir" || { echo "exFAT image is not mounted" >&2; exit 1; }
rsync -r "$source_dir/" "$mount_dir/"
sync

# Compare ALL file bytes, rather than checking only title metadata.
while IFS= read -r -d '' file; do
    relative=${file#"$source_dir"/}
    cmp -s "$file" "$mount_dir/$relative" || {
        echo "Image readback differs from source: $relative" >&2
        exit 1
    }
done < <(find "$source_dir" -type f -print0)

sudo umount "$mount_dir"
python3 - "$temporary" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
with path.open("rb") as file:
    boot = file.read(128)
if boot[3:11] != b"EXFAT   ":
    raise SystemExit("Incorrect exFAT boot sector")
sector_shift, cluster_shift = boot[108], boot[109]
cluster_bytes = 1 << (sector_shift + cluster_shift)
if cluster_bytes != 65536:
    raise SystemExit(f"Invalid exFAT cluster size: {cluster_bytes}")
print(f"Validated exFAT boot sector: 64 KiB clusters, {path.stat().st_size} bytes")
PY
mv -- "$temporary" "$image"
echo "Verified ShadowMountPlus exFAT candidate: $image"
