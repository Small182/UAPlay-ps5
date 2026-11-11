#!/usr/bin/env bash
# Build UAPlay using PS4 OpenOrbis; no PS5 native binary is reused.
set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
: "${OPENORBIS:?Source /opt/pacbrew/ps4/openorbis/ps4vars.sh first}"
test -x "${OPENORBIS}/usr/bin/openorbis-cmake"
test -x "${OPENORBIS}/bin/create-fself"
test -f "${repo}/source/tmdb_build_key.h" || {
    echo 'Missing TMDb header: set TMDB_API_KEY and run scripts/prepare-tmdb-key.sh' >&2
    exit 2
}

build_dir="${PS4_BUILD_DIR:-${repo}/build/ps4}"
mkdir -p "$build_dir"

"${OPENORBIS}/usr/bin/openorbis-cmake" -S "${repo}/ps4" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --target uaplay_ps4_pkg --verbose --parallel "$(nproc)"

output="${PS4_PKG_OUTPUT:-${build_dir}/UAPlay-PS4.pkg}"
candidate=$(find "$build_dir" -maxdepth 1 -type f -name '*.pkg' -print -quit)
test -n "$candidate" && test -s "$candidate"
if [[ "$candidate" != "$output" ]]; then
    cp -- "$candidate" "$output"
fi
echo "PS4 package candidate: $output (NOT hardware-validated)"
