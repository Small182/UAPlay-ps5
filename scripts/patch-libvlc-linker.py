#!/usr/bin/env python3
"""Add the pinned VLC port's static archives to the native SDL title link.
This is an experimental link recipe; do not publish an exFAT on link failure.
"""
import pathlib
import sys

p = pathlib.Path(sys.argv[1])
s = p.read_text()
marker = '    link_inputs+=(--start-group "${pacbrew_libs[@]}" --end-group)'
if s.count(marker) != 1:
    raise SystemExit("Expected template static-library hook")
s = s.replace(marker, '''    vlc_libs=()
    while IFS= read -r item; do
        case "$item" in
            -Wl,--start-group|-Wl,--end-group|-L*) ;;
            -l*)
                lib="$UAPLAY_VLC_WORK/prefix-ps5/lib/lib${item#-l}.a"
                if [[ -f "$lib" ]]; then vlc_libs+=("$lib"); fi ;;
            /*.a)
                test -s "$item"
                vlc_libs+=("$item") ;;
            *) echo "Unhandled VLC link input: $item" ;;
        esac
    done < "$UAPLAY_VLC_WORK/static-ps5/link.rsp"
    vlc_libs+=("$UAPLAY_VLC_WORK/prefix-ps5/lib/libps5compat.a")
    link_inputs+=(--start-group "${vlc_libs[@]}" "${pacbrew_libs[@]}" --end-group)''')
marker = 'wrap_options=()\n'
if s.count(marker) != 1:
    raise SystemExit("Expected template wrapper hook")
s = s.replace(marker, marker + '''wrap_options+=(--wrap=mmap --wrap=munmap --wrap=geteuid --wrap=getegid -u vlc_static_modules)
# Reuse VLC's console compatibility symbol mappings, except for resolver
# calls already implemented by UAPlay's console_curl.c.
while IFS= read -r name; do
    case "$name" in getnameinfo|gai_strerror|gethostbyname) continue;; esac
    wrap_options+=("--defsym=$name=xemu_ps5_$name")
done < <(grep -oE 'xemu_ps5_[a-z_0-9]+\\(' "$UAPLAY_VLC_UPSTREAM/ps5/compat/libc_missing.c" | sed -e 's/^xemu_ps5_//' -e 's/($//' | sort -u)
wrap_options+=(--defsym=__isthreaded=vlc_ps5_isthreaded)
''')
p.write_text(s)
