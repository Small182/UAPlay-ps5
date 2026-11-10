#!/usr/bin/env bash
set -euo pipefail

if [[ ! "${TMDB_API_KEY:-}" =~ ^[0-9a-fA-F]{32}$ ]]; then
  echo "::error::TMDB_API_KEY is missing or not a 32-character hexadecimal TMDb API v3 key."
  exit 1
fi

# The header is generated only on the CI runner; it is never committed or uploaded.
umask 077
printf '#ifndef UAPLAY_TMDB_BUILD_KEY_H\n#define UAPLAY_TMDB_BUILD_KEY_H\n#define UAPLAY_TMDB_BUILD_KEY "%s"\n#endif\n' "$TMDB_API_KEY" > source/tmdb_build_key.h
echo "TMDb build header prepared (secret value hidden)."
