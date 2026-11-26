"""Check the native executable's full mandatory PE dependency closure.
Copy omitted MSYS2 DLLs into the package and verify imported symbols.
Windows system DLLs and API-set forwarding are resolved by Windows itself.
"""
import os
import shutil
import sys
from pathlib import Path
import pefile

root = Path(sys.argv[1]).resolve()
prefix = Path(sys.argv[2]).resolve()
system = Path(os.environ["SystemRoot"]) / "System32"
errors = []
seen = set()
cache = {}

def pe(path):
    key = str(path).lower()
    if key not in cache:
        obj = pefile.PE(str(path), fast_load=True)
        obj.parse_data_directories(directories=[
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"],
        ])
        cache[key] = obj
    return cache[key]

def local_dir(folder):
    return {p.name.lower(): p for p in folder.iterdir() if p.is_file()}

host = local_dir(prefix)
system_files = local_dir(system)
bundle = local_dir(root)

def verify(path):
    key = str(path).lower()
    if key in seen:
        return
    seen.add(key)
    obj = pe(path)
    if obj.FILE_HEADER.Machine != 0x8664:
        errors.append(f"{path.name}: not x86-64")
    for item in getattr(obj, "DIRECTORY_ENTRY_IMPORT", []):
        name = item.dll.decode("ascii").lower()
        if name.startswith(("api-ms-win-", "ext-ms-win-")):
            continue
        target = bundle.get(name)
        if target is None and name in system_files:
            continue
        if target is None and name in host:
            target = root / host[name].name
            shutil.copy2(host[name], target)
            bundle[name] = target
            print(f"Added omitted runtime DLL: {target.name}", flush=True)
        if target is None:
            errors.append(f"{path.name}: missing {name}")
            continue
        dep = pe(target)
        exports = getattr(getattr(dep, "DIRECTORY_ENTRY_EXPORT", None), "symbols", [])
        names = {e.name for e in exports if e.name}
        ordinals = {e.ordinal for e in exports}
        for imp in item.imports:
            if imp.name is not None and imp.name not in names:
                errors.append(f"{path.name}: {target.name} does not export {imp.name.decode(errors='replace')}")
            elif imp.name is None and imp.ordinal not in ordinals:
                errors.append(f"{path.name}: {target.name} does not export ordinal {imp.ordinal}")
        verify(target)

verify(root / "UAPlay.exe")
for path in sorted(seen):
    print(f"Verified {Path(path).name}")
if errors:
    print("\n".join(sorted(set(errors))), file=sys.stderr)
    sys.exit(1)
print(f"PASS: verified {len(seen)} native executable and bundled DLLs.")
