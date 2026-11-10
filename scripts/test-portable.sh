#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

libs="$(pkg-config --cflags --libs libcurl)"
for name in source_ranking_smoke; do
  cc -O2 -Wall -Wextra -Isource source/catalog.c "tests/${name}.c" $libs -o "${out}/${name}"
  "${out}/${name}"
done

for name in country_filter_smoke date_sort_smoke details_metadata_smoke playerjs_playlist_smoke; do
  cc -O2 -Wall -Wextra -Isource "tests/${name}.c" $libs -o "${out}/${name}"
  "${out}/${name}"
done

echo 'Portable UAPlay checks passed.'

cc -O2 -Wall -Wextra -Ips5 ps5/evo_bridge.c tests/evo_bridge_smoke.c $libs -o "${out}/evo_bridge_smoke"
"${out}/evo_bridge_smoke"

