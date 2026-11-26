"""Generate static Inter instances matching UAPlay-web typography.
Use the bundled OFL Inter font and a pinned OFL Japanese fallback.
Static instances also work with the PS5 SDL_ttf/FreeType build.
"""
from pathlib import Path
from hashlib import sha1
from io import BytesIO
from urllib.request import urlopen
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

root = Path(__file__).resolve().parents[1] / "romfs"
source = root / "InterVariable.ttf"
for weight in (400, 550, 580, 700, 740):
    with TTFont(source) as font:
        axes = {axis.axisTag: axis.defaultValue for axis in font["fvar"].axes}
        axes["wght"] = weight
        instance = instantiateVariableFont(font, axes, inplace=True)
        assert "fvar" not in instance
        assert instance["OS/2"].usWeightClass == weight
        cmap = instance.getBestCmap()
        assert all(ord(ch) in cmap for ch in "UAPlayЇЄІҐїєіґ")
        # Unique family names for the derived static faces.
        family = f"UAPlay UI {weight}"
        for record in instance["name"].names:
            value = {1: family, 2: "Regular", 4: family, 6: f"UAPlayUI-{weight}",
                     16: family, 17: "Regular"}.get(record.nameID)
            if value:
                record.string = value.encode(record.getEncoding())
        dest = root / f"UAPlayUI-{weight}.ttf"
        instance.save(dest)
        print(f"Typography: {dest.name}, weight {weight}, Cyrillic verified")

# Japanese titles and IPTV station names need kana and kanji. Pin the
# upstream Google Fonts revision and verify the Git blob hashes before use.
jp_revision = "295d98a7a0c17c68f1341eaeea354e7960ea70d3"
jp_base = f"https://raw.githubusercontent.com/google/fonts/{jp_revision}/ofl/notosansjp/"

def verified_download(name, expected):
    last_error = None
    for _ in range(3):
        try:
            with urlopen(jp_base + name, timeout=90) as response:
                data = response.read()
            actual = sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
            if actual != expected:
                raise ValueError(f"Unexpected upstream blob: {name}")
            return data
        except (OSError, ValueError) as error:
            last_error = error
    raise RuntimeError(f"Unable to obtain Japanese font asset: {name}") from last_error

jp_path = root / "NotoSansJP.ttf"
license_path = root / "NotoSansJP-OFL.txt"
if not jp_path.exists() or not license_path.exists():
    jp_data = verified_download("NotoSansJP%5Bwght%5D.ttf", "cdd8f083c1f5928ff3361f8cda4d3fc9462cbe89")
    license_data = verified_download("OFL.txt", "1c9f43281b8f216c5461fe9ac729afbade7724e4")
    with TTFont(BytesIO(jp_data)) as font:
        instance = instantiateVariableFont(font, {"wght": 400}, inplace=True)
        assert "fvar" not in instance
        instance.save(jp_path)
    license_path.write_bytes(license_data)
with TTFont(jp_path) as font:
    cmap = font.getBestCmap()
    assert all(ord(ch) in cmap for ch in "日本語ひらがなカタカナ東京テレビ")
    assert "fvar" not in font
print("Typography: Japanese kana and kanji fallback verified")
