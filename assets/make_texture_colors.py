"""Wyciaga miniaturki tekstur z archiwow VDF Gothica do pliku GothicSurveying/texcolors.bin (kolor punktow skanera laserowego).

Uzycie:  python assets/make_texture_colors.py "<katalog gry Gothic II>"

Czyta Data/Textures*.vdf (oraz Data/ModVDF/*.vdf, *.mod), dla kazdej skompilowanej tekstury "<NAZWA>-C.TEX" (format ZTEX: nagłówek, paleta, poziomy mipmap od najmniejszego)
dekoduje poziom o rozmiarze ok. 16-32 px i zapisuje miniaturke (max 16x16) usredniona w blokach.
Format wyjscia: "GSTC", uint32 liczba tekstur, potem dla kazdej: uint8 dlugosc nazwy, nazwa (ASCII, wielkie litery, bez -C.TEX), uint8 szerokosc, uint8 wysokosc, w*h*3 bajtow RGB.
"""
import struct
import sys
from pathlib import Path

DXT1, DXT2, DXT3, DXT4, DXT5 = 10, 11, 12, 13, 14
BPP = {0: 4, 1: 4, 2: 4, 3: 4, 4: 3, 5: 3, 6: 2, 7: 2, 8: 2, 9: 1}   # format zTRnd_TextureFormat -> bajty na piksel (bez DXT)
THUMB = 16


def read_vdf(path):
    """Zwraca liste (nazwa, offset, rozmiar) plikow z archiwum VDF (format 2.00)."""
    d = open(path, "rb").read()
    if d[256:264] != b"PSVDSC_V" and d[256:262] != b"PSVDSC":
        return []
    num_entries, num_files, _ts, _size, root, entry_size = struct.unpack_from("<6I", d, 272)
    out = []
    for i in range(num_entries):
        o = root + i * entry_size
        name = d[o:o + 64].decode("latin-1").rstrip(" \x00")
        jump, size, typ, _attr = struct.unpack_from("<4I", d, o + 64)
        if typ & 0x80000000:
            continue                                  # katalog
        out.append((name, jump, size))
    return out, d


def rgb565(c):
    return ((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31


def decode_dxt(data, w, h, fmt):
    """Dekoduje poziom DXT do listy wierszy RGB (None = przezroczysty tekstel DXT1)."""
    bs = 8 if fmt == DXT1 else 16
    bw = max(1, (w + 3) // 4)
    px = [[None] * w for _ in range(h)]
    for by in range((h + 3) // 4):
        for bx in range(bw):
            o = (by * bw + bx) * bs + (8 if bs == 16 else 0)
            if o + 8 > len(data):
                return px
            c0, c1 = struct.unpack_from("<2H", data, o)
            bits = struct.unpack_from("<I", data, o + 4)[0]
            a, b = rgb565(c0), rgb565(c1)
            if c0 > c1 or fmt != DXT1:
                pal = [a, b, tuple((2 * a[k] + b[k]) // 3 for k in range(3)), tuple((a[k] + 2 * b[k]) // 3 for k in range(3))]
            else:
                pal = [a, b, tuple((a[k] + b[k]) // 2 for k in range(3)), None]
            for y in range(4):
                for x in range(4):
                    yy, xx = by * 4 + y, bx * 4 + x
                    if yy < h and xx < w:
                        px[yy][xx] = pal[(bits >> (2 * (y * 4 + x))) & 3]
    return px


def decode_raw(data, w, h, fmt):
    bpp = BPP[fmt]
    px = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            o = (y * w + x) * bpp
            p = data[o:o + bpp]
            if len(p) < bpp:
                return px
            if fmt == 0:   px[y][x] = (p[2], p[1], p[0])           # ARGB: w pamieci B G R A
            elif fmt == 1: px[y][x] = (p[0], p[1], p[2])           # ABGR
            elif fmt == 2: px[y][x] = (p[3], p[2], p[1])           # RGBA
            elif fmt == 3: px[y][x] = (p[1], p[2], p[3])           # BGRA
            elif fmt == 4: px[y][x] = (p[0], p[1], p[2])
            elif fmt == 5: px[y][x] = (p[2], p[1], p[0])
            elif fmt == 6:
                v = p[0] | (p[1] << 8); px[y][x] = (((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17)
            elif fmt == 7:
                v = p[0] | (p[1] << 8); px[y][x] = (((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31)
            elif fmt == 8:
                px[y][x] = rgb565(p[0] | (p[1] << 8))
    return px


def level_size(w, h, fmt):
    if DXT1 <= fmt <= DXT5:
        return max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * (8 if fmt == DXT1 else 16)
    return w * h * BPP.get(fmt, 0)


def thumbnail(tex):
    """tex = bajty ZTEX -> (w, h, [rgb...]) albo None."""
    if tex[:4] != b"ZTEX":
        return None
    _ver, fmt, w, h, mips, _rw, _rh, _avg = struct.unpack_from("<8I", tex, 4)
    o = 36
    if fmt == 9:
        return None                                             # paletowe: pomijamy
    if fmt not in BPP and not (DXT1 <= fmt <= DXT5):
        return None
    # poziomy zapisane od najmniejszego do najwiekszego
    sizes = []
    for l in range(mips):
        lw, lh = max(1, w >> l), max(1, h >> l)
        sizes.append((lw, lh, level_size(lw, lh, fmt)))
    offs = {}
    pos = o
    for l in range(mips - 1, -1, -1):
        offs[l] = pos
        pos += sizes[l][2]
    # poziom o najwiekszym rozmiarze nie wiekszym niz 32 (albo najmniejszy dostepny > 32)
    cand = [l for l in range(mips) if max(sizes[l][0], sizes[l][1]) <= 32]
    pick = min(cand) if cand else mips - 1
    lw, lh, sz = sizes[pick]
    data = tex[offs[pick]:offs[pick] + sz]
    px = decode_dxt(data, lw, lh, fmt) if DXT1 <= fmt <= DXT5 else decode_raw(data, lw, lh, fmt)
    tw, th = min(THUMB, lw), min(THUMB, lh)
    out = []
    for ty in range(th):
        for tx in range(tw):
            x0, x1, y0, y1 = tx * lw // tw, (tx + 1) * lw // tw, ty * lh // th, (ty + 1) * lh // th
            acc = [0, 0, 0]; n = 0
            for y in range(y0, max(y1, y0 + 1)):
                for x in range(x0, max(x1, x0 + 1)):
                    p = px[y][x]
                    if p is not None:
                        acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; n += 1
            out.append(tuple(a // n for a in acc) if n else None)
    # puste (przezroczyste) pola: srednia z reszty
    valid = [p for p in out if p]
    if not valid:
        return None
    avg = tuple(sum(p[k] for p in valid) // len(valid) for k in range(3))
    return tw, th, [p if p else avg for p in out]


def main():
    if len(sys.argv) < 2:
        sys.exit('Uzycie: python make_texture_colors.py "<katalog gry Gothic II>"')
    game = Path(sys.argv[1])
    archives = sorted((game / "Data").glob("Textures*.vdf")) + sorted((game / "Data" / "ModVDF").glob("*.vdf")) + sorted((game / "Data" / "ModVDF").glob("*.mod"))
    thumbs = {}
    for arc in archives:
        res = read_vdf(arc)
        if not res:
            continue
        entries, d = res
        for name, off, size in entries:
            up = name.upper()
            if not up.endswith("-C.TEX"):
                continue
            t = thumbnail(d[off:off + size])
            if t:
                thumbs[up[:-6]] = t
        print(f"{arc.name}: razem {len(thumbs)} tekstur")
    out = game / "GothicSurveying" / "texcolors.bin"
    out.parent.mkdir(exist_ok=True)
    with open(out, "wb") as f:
        f.write(b"GSTC" + struct.pack("<I", len(thumbs)))
        for name, (w, h, px) in thumbs.items():
            nb = name.encode("ascii", "replace")[:255]
            f.write(struct.pack("<B", len(nb)) + nb + struct.pack("<BB", w, h) + b"".join(bytes(p) for p in px))
    print(f"zapisano {out} ({out.stat().st_size / 1024:.0f} KB, {len(thumbs)} tekstur)")


if __name__ == "__main__":
    main()
