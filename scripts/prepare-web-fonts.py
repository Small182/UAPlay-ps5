"""Generate static Inter instances matching UAPlay-web typography.
Only use the OFL font already bundled with UAPlay; no proprietary system fonts.
Static instances also work with the PS5 SDL_ttf/FreeType build.
"""
from pathlib import Path
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
