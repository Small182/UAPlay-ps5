#!/usr/bin/env bash
# Configure an optional first-party UAPlay-web Worker origin for native PS5.
# This is a PUBLIC origin, never a TMDB token or an API secret.
set -euo pipefail
python3 - <<'PY'
import os
from pathlib import Path
from urllib.parse import urlsplit

origin = os.environ.get("UAPLAY_WEB_API_URL", "").strip().rstrip("/")
target = Path("source/web_api_build_config.h")
if not origin:
    target.unlink(missing_ok=True)
    print("UAPlay-web API URL is not set; native TMDB fallback remains active.")
    raise SystemExit(0)

url = urlsplit(origin)
if (url.scheme != "https" or not url.hostname or url.path or url.query
        or url.fragment or url.username or url.password
        or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:"
               for c in origin[len("https://"):])):
    raise SystemExit("UAPLAY_WEB_API_URL must be an HTTPS origin without path or credentials")
if len(origin) >= 220:
    raise SystemExit("UAPLAY_WEB_API_URL is too long")

target.write_text(
    '#ifndef UAPLAY_WEB_API_BUILD_CONFIG_H\n'
    '#define UAPLAY_WEB_API_BUILD_CONFIG_H\n'
    '#define UAPLAY_WEB_API_URL "' + origin + '"\n'
    '#endif\n', encoding="utf-8")
print("Configured UAPlay-web Worker API origin for PS5 (no token embedded).")
PY
