#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

libs="$(pkg-config --cflags --libs libcurl)"
for name in source_ranking_smoke; do
  cc -O2 -Wall -Wextra -Isource source/catalog.c source/i18n.c "tests/${name}.c" $libs -o "${out}/${name}"
  "${out}/${name}"
done

for name in country_filter_smoke date_sort_smoke details_metadata_smoke playerjs_playlist_smoke; do
  cc -O2 -Wall -Wextra -Isource source/i18n.c "tests/${name}.c" $libs -o "${out}/${name}"
  "${out}/${name}"
done

# Episode history uses a per-test scratch directory rather than touching the
# user's existing SD or the repository checkout.
cc -O2 -Wall -Wextra -Isource source/library.c tests/episode_history_smoke.c \
   -o "${out}/episode_history_smoke"
(cd "${out}" && ./episode_history_smoke)

cc -O2 -Wall -Wextra -Isource tests/web_ui_search_smoke.c \
  -o "${out}/web_ui_search_smoke"
"${out}/web_ui_search_smoke"

echo 'Portable UAPlay checks passed.'

