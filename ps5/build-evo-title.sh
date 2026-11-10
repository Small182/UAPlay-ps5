#!/usr/bin/env bash
# Experimental EVO-derived host build. GPL-3.0-or-later upstream is preserved.
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work_root="${RUNNER_TEMP:-/tmp}/uaplay-evo-build"
upstream="$work_root/nuvio-source"
output="$project_root/build/evo-title"
mkdir -p "$work_root" "$output"

if [[ ! -d "$upstream/.git" ]]; then
  git clone https://github.com/bilawalriaz/nuvio-ps5-homebrew.git "$upstream"
fi
git -C "$upstream" checkout --detach 3de5c54ca1539bd585eb07f6429fd21a5ead1b72

# The Nuvio port provides an already tested EVO native host. Its source
# identity is rewritten only inside this temporary build; never in the repo.
python3 - "$upstream" <<'PY'
from pathlib import Path
import json
import sys
root=Path(sys.argv[1])
for path in [root/'scripts/build.py',root/'scripts/ui_server.c',root/'scripts/bootstrap.c']:
    if path.exists():
        content=path.read_text()
        path.write_text(content.replace('PPSA99997','PPSA99282'))
meta=root/'sce_sys/param.json'
data=json.loads(meta.read_text())
data['titleId']='PPSA99282'
data['conceptId']='99282'
data['contentId']='UP9000-PPSA99282_00-UAPLAYPS50000000'
data['localizedParameters']['en-US']['titleName']='UAPlay PS5 (EVO test)'
meta.write_text(json.dumps(data,indent=2)+'\n')
PY

# Nuvio's verified adapter compiles EVO's FFmpeg, native decoder, audio,
# sandbox bridge, and title folder. The first build can take a long time.
(cd "$upstream" && make build)

title_dir="$(find "$upstream/build" -type d -path '*/output/app/PPSA99282' -print -quit)"
if [[ -z "$title_dir" ]]; then
  echo 'EVO native title output was not found' >&2
  exit 1
fi

# Replace the browser frontend, not the native EVO decoder. Keep the upstream
# GPL notices and package the separate boot helpers for diagnostic deployment.
mkdir -p "$title_dir/webui"
cp -a "$project_root/ps5/webui/." "$title_dir/webui/"

cp -a "$title_dir" "$output/PPSA99282"
for helper in nuvio.elf promote.elf close.elf; do
  if [[ -f "$upstream/build/$helper" ]]; then
    cp "$upstream/build/$helper" "$output/$helper"
  fi
done
cp "$upstream/LICENSE" "$output/NUVIO-UPSTREAM-LICENSE" 2>/dev/null || true
printf '%s\n' 'EVO-derived test host. Not validated on PS5 hardware.' > "$output/README-TEST.txt"
tar -C "$output" -czf "$project_root/UAPlay-PS5-EVO-test.tar.gz" .
echo 'Experimental EVO-native PS5 title packaged.'
