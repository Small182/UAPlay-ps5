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
    # The VLC upstream uses the PS5_Vulkan SDK fork, which overlays this
    # platform archive on the same v0.42 SDK binaries we already cache.
    # Build only the overlay; no need to rebuild libVLC or change its cache.
    platform_rev=b83202be73e930050e00de0f1b4d9ec46c0391bf
    platform_tree="$build/ps5-platform-source"
    mkdir -p "$platform_tree"
    git -C "$platform_tree" init --quiet
    git -C "$platform_tree" fetch --quiet --depth=1 https://github.com/mihawk-99/PS5_PayloadSDK.git "$platform_rev"
    git -C "$platform_tree" checkout --detach FETCH_HEAD
    llvm_bin="$(dirname "$(readlink -f "$target_compiler")")"
    make -C "$platform_tree/platform" -j"$(nproc)" LLVM_CONFIG=true LLVM_BINDIR="$llvm_bin"
    platform_archive="$platform_tree/platform/build/ps5/libps5platform.a"
    test -s "$platform_archive"
    cpp_libs+=("$platform_archive")
    link_inputs+=(--start-group "${vlc_libs[@]}" "${pacbrew_libs[@]}" "${cpp_libs[@]}" --end-group)''')
marker = 'wrap_options=()\n'
if s.count(marker) != 1:
    raise SystemExit("Expected template wrapper hook")
s = s.replace(marker, marker + '''# libc++ embeds an ELF dependent-library directive for pthread, but PS5
# supplies pthread symbols via the explicit libkernel imports, not libpthread.
# Ignore automatic library directives; unresolved symbols are still fatal.
wrap_options+=(--no-dependent-libraries --error-limit=0 --exclude-libs=ALL --wrap=mmap --wrap=munmap --wrap=aligned_alloc --wrap=geteuid --wrap=getegid -u vlc_static_modules)
# Reuse VLC's console compatibility symbol mappings, except for resolver
# calls already implemented by UAPlay's console_curl.c.
while IFS= read -r name; do
    case "$name" in getnameinfo|gai_strerror|gethostbyname) continue;; esac
    wrap_options+=("--defsym=$name=xemu_ps5_$name")
done < <(grep -oE 'xemu_ps5_[a-z_0-9]+\\(' "$UAPLAY_VLC_UPSTREAM/ps5/compat/libc_missing.c" | sed -e 's/^xemu_ps5_//' -e 's/($//' | sort -u)
wrap_options+=(--defsym=__isthreaded=vlc_ps5_isthreaded)
# Bind the locale and filesystem APIs missing from the stock SDK. Keep
# UAPlay's existing network resolver, heap, and POSIX shims untouched.
# Directory streams must use the entire matching implementation family.
platform_names=(
    strtoll_l strtoull_l strtof_l strtod_l strtold_l sscanf_l snprintf_l asprintf_l
    __runes_for_locale strftime_l mbsrtowcs_l catopen catgets catclose strcoll_l strxfrm_l
    wcscoll_l wcsxfrm_l iswctype_l ___runetype_l ___toupper_l ___tolower_l btowc_l wctob_l
    wcsnrtombs_l wcrtomb_l mbsnrtowcs_l mbrtowc_l mbtowc_l ___mb_cur_max_l mbrlen_l localeconv_l
    __xuname backtrace backtrace_symbols_fd opendir fdopendir readdir rewinddir dirfd closedir
    __cxa_thread_atexit_impl
)
platform_symbols=$("$llvm_bin/llvm-nm" --defined-only "$platform_archive")
for name in "${platform_names[@]}"; do
    if ! grep -q " T ps5_$name$" <<<"$platform_symbols"; then
        echo "Missing PS5 platform implementation: ps5_$name" >&2
        exit 2
    fi
    wrap_options+=("--defsym=$name=ps5_$name")
done
''')
p.write_text(s)

# Make the pinned native converter report every missing import in one pass.
# Keep conversion fatal: never generate an eboot with unresolved symbols.
converter = p.parent.parent / "tooling/native/sce_module_writer.cpp"
source = converter.read_text()
before = "    for (std::size_t i = 1; i < image.dynamic_symbols.size(); ++i)"
missing = '        require(provider != nullptr, "no public SDK stub exports required symbol " + symbol.name);'
after = "    std::vector<ModuleRecord> modules;"
for hook in (before, missing, after):
    if source.count(hook) != 1:
        raise SystemExit("Unexpected native converter import-validation hook: " + hook)
source = source.replace(before, "    std::string missing_imports;" + chr(10) + before)
source = source.replace(missing, """        if (provider == nullptr)
        {
            missing_imports += "\\n  " + symbol.name;
            continue;
        }""")
source = source.replace(after, """    require(missing_imports.empty(), "Missing PS5 SDK import implementations:" + missing_imports);

""" + after)
converter.write_text(source)


# VLC's picture pool uses C11 aligned_alloc even for its small pool header.
# Route it through the same checked, free-compatible allocator as the rest
# of the title. Also allow VLC's 20+ frame pools more space than the UI's
# original fixed 128 MiB heap. Fall back to the original size if constrained.
heap = p.parent.parent / "src/runtime/app_heap.c"
hs = heap.read_text()
def replace_heap(old, new):
    global hs
    if hs.count(old) != 1:
        raise SystemExit("Unexpected title heap hook: " + old)
    hs = hs.replace(old, new)
replace_heap("#define HUI_HEAP_SIZE (128u * 1024u * 1024u)",
             "#define HUI_HEAP_SIZE (384u * 1024u * 1024u)\nstatic size_t hui_heap_capacity;")
replace_heap("void *base = mmap(NULL, HUI_HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);",
"""size_t capacity = HUI_HEAP_SIZE;
    void *base = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (base == MAP_FAILED) {
        capacity = 128u * 1024u * 1024u;
        base = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    }""")
replace_heap('hui_heap_mspace = sceLibcMspaceCreate("ps5-homebrew-ui", base, HUI_HEAP_SIZE, 0);',
             'hui_heap_capacity = capacity;\n    hui_heap_mspace = sceLibcMspaceCreate("ps5-homebrew-ui", base, capacity, 0);')
replace_heap("munmap(base, HUI_HEAP_SIZE);", "munmap(base, capacity);")
replace_heap("value - base < HUI_HEAP_SIZE", "value - base < hui_heap_capacity")
replace_heap("""    int result = sceLibcMspacePosixMemalign(hui_heap_mspace, address, alignment, size);
    if (result == 0)
        hui_heap_record_allocation(*address, size != 0);
    else if (result == ENOMEM)
        atomic_fetch_add_explicit(&hui_heap_failures, 1, memory_order_relaxed);
    return result;""",
"""    *address = NULL;
    int result = sceLibcMspacePosixMemalign(hui_heap_mspace, address, alignment, size);
    if (result == 0 && *address != NULL) {
        hui_heap_record_allocation(*address, size != 0);
        return 0;
    }
    atomic_fetch_add_explicit(&hui_heap_failures, 1, memory_order_relaxed);
    return __real_posix_memalign(address, alignment, size);""")
hs += """
/* libc's aligned_alloc is not necessarily implemented by the title runtime.
   VLC's picture and pool allocations must use the same heap/free family. */
void *__wrap_aligned_alloc(size_t alignment, size_t size)
{
    if (alignment < sizeof(void *) || (alignment & (alignment - 1)) ||
        size % alignment) {
        errno = EINVAL;
        return NULL;
    }
    void *ptr = NULL;
    int rc = __wrap_posix_memalign(&ptr, alignment, size ? size : alignment);
    if (rc != 0) {
        errno = rc > 0 ? rc : ENOMEM;
        return NULL;
    }
    return ptr;
}
"""
heap.write_text(hs)
