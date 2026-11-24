"""Wyciaga mapy swiatow z archiwow VDF gry (pergaminy MAP_<swiat>.TGA, 1024x768) do _work/Data/Textures/LVL_GMAP_<swiat>.TGA.
Mapa osnowy w grze uzywa ich zamiast wygenerowanych map (bez nich dziala mapa wygenerowana). Pliki sa wlasnoscia gry i nie sa czescia repozytorium ani paczki.

Uzycie:  python assets/make_game_maps.py "<katalog gry Gothic II>"
Czyta Data/*.vdf (oraz Data/ModVDF/*): Textures.vdf (Khorinis, Dolina Gornicza), Textures_Addon.vdf (Jarkendar).
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from make_texture_colors import read_vdf          # ten sam czytnik VDF co dla miniaturek tekstur

WORLDS = ("NEWWORLD", "OLDWORLD", "ADDONWORLD")


def main():
    if len(sys.argv) < 2:
        sys.exit("Uzycie: python make_game_maps.py <katalog gry Gothic II>")
    game = Path(sys.argv[1])
    out = game / "_work/Data/Textures"
    out.mkdir(parents=True, exist_ok=True)
    found = {}
    for f in sorted(list((game / "Data").glob("*.vdf")) + list((game / "Data/ModVDF").glob("*.*"))):
        try:
            r = read_vdf(f)
        except Exception:
            continue
        if not r:
            continue
        ents, d = r
        for name, off, size in ents:
            u = name.upper()
            for w in WORLDS:
                if u == f"MAP_{w}.TGA":
                    found[w] = d[off:off + size]            # pozniejsze archiwum (mod) nadpisuje wczesniejsze
    for w in WORLDS:
        if w in found:
            (out / f"LVL_GMAP_{w}.TGA").write_bytes(found[w])
            print(f"LVL_GMAP_{w}.TGA: {len(found[w])} bajtow")
        else:
            print(f"MAP_{w}.TGA: brak w archiwach (mapa wygenerowana zostaje)")


if __name__ == "__main__":
    main()
