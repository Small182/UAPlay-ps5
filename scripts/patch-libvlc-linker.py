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
    # VLC's adaptive streaming and Matroska plugins are C++, unlike the old
    # Kitchensink adapter. Use the target SDK runtime, never the host libc++.
    cpp_libs=()
    for lib in libc++.a libc++abi.a libunwind.a; do
        file="$sdk_root/target/lib/$lib"
        test -s "$file" || { echo "Missing PS5 C++ runtime: $file" >&2; exit 2; }
        cpp_libs+=("$file")
    done
    builtins="$("$target_compiler" --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a"
    test -s "$builtins" || { echo "Missing clang compiler runtime: $builtins" >&2; exit 2; }
    cpp_libs+=("$builtins")
    if [[ -f "$sdk_root/target/lib/libps5platform.a" ]]; then
        cpp_libs+=("$sdk_root/target/lib/libps5platform.a")
    fi
    link_inputs+=(--start-group "${vlc_libs[@]}" "${pacbrew_libs[@]}" "${cpp_libs[@]}" --end-group)''')
marker = 'wrap_options=()\n'
if s.count(marker) != 1:
    raise SystemExit("Expected template wrapper hook")
s = s.replace(marker, marker + '''# libc++ embeds an ELF dependent-library directive for pthread, but PS5
# supplies pthread symbols via the explicit libkernel imports, not libpthread.
# Ignore automatic library directives; unresolved symbols are still fatal.
wrap_options+=(--no-dependent-libraries --error-limit=0 --exclude-libs=ALL --wrap=mmap --wrap=munmap --wrap=geteuid --wrap=getegid -u vlc_static_modules)
# Reuse VLC's console compatibility symbol mappings, except for resolver
# calls already implemented by UAPlay's console_curl.c.
while IFS= read -r name; do
    case "$name" in getnameinfo|gai_strerror|gethostbyname) continue;; esac
    wrap_options+=("--defsym=$name=xemu_ps5_$name")
done < <(grep -oE 'xemu_ps5_[a-z_0-9]+\\(' "$UAPLAY_VLC_UPSTREAM/ps5/compat/libc_missing.c" | sed -e 's/^xemu_ps5_//' -e 's/($//' | sort -u)
wrap_options+=(--defsym=__isthreaded=vlc_ps5_isthreaded)
''')
p.write_text(s)
