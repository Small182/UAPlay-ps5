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

# The original ps5-homebrew-ui repository was removed. The EVO maintainer's
# fork still serves the exact pinned commit and byte-identical tarball.
# Preserve the recorded SHA-256: the upstream build verifies it on download.
lock=root/'deps.lock'
old='https://codeload.github.com/blackbearreloaded/ps5-homebrew-ui/tar.gz/4bd942579dd981b3df9c740438489ca6a614ddc0'
new='https://codeload.github.com/sainsaji/ps5-homebrew-ui/tar.gz/4bd942579dd981b3df9c740438489ca6a614ddc0'
pins=json.loads(lock.read_text())
dependency=next(a for a in pins['artifacts'] if a['id']=='ps5-homebrew-ui-source')
assert dependency['url']==old, 'Unexpected ps5-homebrew-ui archive source'
assert dependency['sha256']=='e51a7e20d9bf5e051d8106c87268bcdcb22717919b69ece4510e063a754ecf0d'
dependency['url']=new
lock.write_text(json.dumps(pins,indent=2)+'\n')
PY

if command -v sips >/dev/null; then
  sips -s format png "$project_root/icon.jpg" --out "$upstream/sce_sys/icon0.png" >/dev/null
fi

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
if command -v sips >/dev/null && [[ -d "$title_dir/sce_sys" ]]; then
  sips -s format png "$project_root/icon.jpg" --out "$title_dir/sce_sys/icon0.png" >/dev/null
fi
if [[ "${TMDB_API_KEY:-}" =~ ^[0-9a-fA-F]{32}$ ]]; then
  # TMDb's application key is included in the installed client, as on Switch.
  # Do not print the key in Actions logs or commit the generated file.
  umask 077
  printf 'window.UAPLAY_TMDB_API_KEY="%s";\n' "$TMDB_API_KEY" > "$title_dir/webui/config.js"
else
  echo 'TMDB_API_KEY missing; the offline catalog will remain available.'
fi

cp -a "$title_dir" "$output/PPSA99282"
mkdir -p "$output/helpers"
for helper in nuvio.elf promote.elf close.elf control-{1,2,3,4,5}.elf; do
  if [[ -f "$upstream/build/$helper" ]]; then
    cp "$upstream/build/$helper" "$output/helpers/$helper"
  fi
done
python3 - "$upstream/build/build.json" "$output/PPSA99282" "$output/build.json" <<'PY'
from pathlib import Path
import hashlib
import json
import sys
receipt_path,title_path,output_path=map(Path,sys.argv[1:])
receipt=json.loads(receipt_path.read_text())
receipt['title_id']='PPSA99282'
receipt['firmware_validation']='[UNKNOWN - needs console test]'
receipt['frontend']='UAPlay PS5 experimental WebUI'
receipt['files']={str(path.relative_to(title_path)):
                  hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in title_path.rglob('*') if path.is_file()}
output_path.write_text(json.dumps(receipt,indent=2)+'\n')
PY
cp "$upstream/LICENSE" "$output/NUVIO-UPSTREAM-LICENSE" 2>/dev/null || true
cp "$project_root/ps5/DEPLOY.md" "$output/README-TEST.md"
tar -C "$output" -czf "$project_root/UAPlay-PS5-EVO-test.tar.gz" .
echo 'Experimental EVO-native PS5 title packaged.'
