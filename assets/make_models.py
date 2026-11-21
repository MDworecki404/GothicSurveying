"""Generuje modele niwelacji (łata 3 m, niwelator na statywie, żabka), tekstury i krzyż kresek (.3DS + .TGA).

Użycie:  python make_models.py "<katalog gry>"          (zapis do <gra>/_work/Data/Meshes i Textures)
Jednostki = cm (1 jednostka = 1 cm w Gothicu). UP = "Z": plik w układzie Z-w-górę jak 3ds Max;
jeśli w grze modele leżą poziomo, ustaw UP = "Y" i wygeneruj ponownie.
Każdy trójkąt jest zapisany w obu kierunkach (obustronny), więc kierunek ścianek nie gra roli.
Łata to 6 odcinków po 50 cm, każdy z własną teksturą 256x2048 (~41 px/cm pionowo), żeby w lunecie było widać podziałkę.
"""
import math
import shutil
import struct
import sys
from pathlib import Path

UP = "Z"


# ---------- geometria ----------
class Mesh:
    def __init__(self):
        self.v, self.uv, self.f, self.fm = [], [], [], []   # fm = indeks materiału każdego trójkąta

    def tri(self, a, b, c, uv, mat=0):
        i = len(self.v)
        self.v += [a, b, c]
        self.uv += [uv(a), uv(b), uv(c)]
        self.f += [(i, i + 1, i + 2), (i, i + 2, i + 1)]  # obustronnie
        self.fm += [mat, mat]


def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a):
    n = math.sqrt(sum(x * x for x in a))
    return mul(a, 1 / n)


def prism(m, a, b, r, n, uv, mat=0):
    """Graniastosłup n-kątny (n=4 kwadratowy, n=12 prawie walec) od punktu a do b, promień r."""
    ax = norm(sub(b, a))
    helper = (1, 0, 0) if abs(ax[0]) < 0.9 else (0, 1, 0)
    u = norm(cross(ax, helper))
    w = cross(ax, u)
    ring = []
    for k in range(n):
        t = 2 * math.pi * (k + 0.5) / n if n == 4 else 2 * math.pi * k / n
        d = add(mul(u, math.cos(t) * r), mul(w, math.sin(t) * r))
        ring.append((add(a, d), add(b, d)))
    for k in range(n):
        p0, q0 = ring[k]
        p1, q1 = ring[(k + 1) % n]
        m.tri(p0, p1, q1, uv, mat)
        m.tri(p0, q1, q0, uv, mat)
        m.tri(a, p1, p0, uv, mat)   # denko
        m.tri(b, q0, q1, uv, mat)   # wieko


def box(m, cx, cy, cz, sx, sy, sz, uv, mat=0):
    h = (sx / 2, sy / 2, sz / 2)
    c = [(cx + (i * 2 - 1) * h[0], cy + (j * 2 - 1) * h[1], cz + (k * 2 - 1) * h[2])
         for i in (0, 1) for j in (0, 1) for k in (0, 1)]
    for q in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        m.tri(c[q[0]], c[q[1]], c[q[2]], uv, mat)
        m.tri(c[q[0]], c[q[2]], c[q[3]], uv, mat)


# ---------- modele (Z-w-górę, cm) ----------
ROD_H, ROD_W, ROD_T = 300.0, 12.0, 3.0
SEG_CM, NSEG = 50.0, 6                    # łata = 6 odcinków po 50 cm, każdy z własną teksturą


def build_rod():
    m = Mesh()
    for k in range(NSEG):
        z0 = k * SEG_CM
        box(m, 0, 0, z0 + SEG_CM / 2, ROD_W, ROD_T, SEG_CM,
            lambda p, z0=z0: (min(max(p[0] / ROD_W + 0.5, 0), 1), min(max((p[2] - z0) / SEG_CM, 0), 1)), mat=k)
    return m


def block(i):  # stały punkt w i-tym z 8 bloków atlasu 128x16: drewno | stal | ciemny | pomarańcz
    return lambda p: ((i + 0.5) / 8, 0.5)


def build_instrument():
    m = Mesh()
    cx, cy = 0.0, 0.0
    for ang in (90, 210, 330):                       # statyw: 3 nogi rozstawione na zewnątrz
        a = math.radians(ang)
        prism(m, (cx + 55 * math.cos(a), cy + 55 * math.sin(a), 0), (cx + 3 * math.cos(a), cy + 3 * math.sin(a), 140), 2.2, 4, block(0))
    prism(m, (0, 0, 138), (0, 0, 146), 9, 12, block(1))          # głowica statywu
    prism(m, (0, 0, 146), (0, 0, 147.5), 7, 12, block(3))        # limbus (pomarańcz)
    box(m, 0, 0, 148.5, 9, 9, 3, block(1))                        # podstawa lunety
    prism(m, (0, -18, 150), (0, 14, 150), 3.5, 12, block(1))     # tubus lunety, oś = 150 cm
    prism(m, (0, 14, 150), (0, 22, 150), 4.8, 12, block(2))      # obiektyw
    prism(m, (0, -24, 150), (0, -18, 150), 2.4, 12, block(2))    # okular
    box(m, 0, 0, 155, 2.5, 8, 2.0, block(3))                      # libela
    return m


def build_plate():
    m = Mesh()
    prism(m, (0, 0, 0), (0, 0, 3), 10, 12, block(1))              # żabka: stalowy talerz
    prism(m, (0, 0, 3), (0, 0, 4.5), 2.0, 12, block(1))           # grzybek, na którym stoi łata
    return m


def build_pole():
    """Tyczka z odbiornikiem GNSS: pret 2,00 m, antena (walec) i pierscien na szczycie. Poczatek = grunt pod tyczka."""
    m = Mesh()
    prism(m, (0, 0, 0), (0, 0, 200), 1.2, 8, block(2))              # pret
    prism(m, (0, 0, 200), (0, 0, 206), 9.0, 12, block(1))           # antena
    prism(m, (0, 0, 206), (0, 0, 208), 10.0, 12, block(3))          # pierscien
    return m


def build_prism_pole(h_cm):
    """Tyczka z lustrem (pryzmatem) na wysokosci h_cm: pret, oprawa i tarcza lustra zwrocona w +Y (ku tachimetrowi). Poczatek = grunt."""
    m = Mesh()
    prism(m, (0, 0, 0), (0, 0, h_cm - 5.5), 1.2, 8, block(2))              # pret
    box(m, 0, 0, h_cm, 11, 6, 11, block(2))                                  # oprawa lustra, srodek na wysokosci h
    prism(m, (0, 3.0, h_cm), (0, 3.8, h_cm), 4.5, 12, block(3))             # szklo lustra (pomaranczowa tarcza)
    prism(m, (0, 3.8, h_cm), (0, 4.1, h_cm), 1.0, 8, block(1))              # srodek tarczy
    return m


def build_tachy():
    """Tachimetr na statywie: statyw, spodarka, alidada z dwoma slupkami, luneta (os 150 cm), uchwyt, panel sterowania."""
    m = Mesh()
    for ang in (90, 210, 330):                                               # statyw: 3 nogi
        a = math.radians(ang)
        prism(m, (55 * math.cos(a), 55 * math.sin(a), 0), (4 * math.cos(a), 4 * math.sin(a), 128), 2.2, 4, block(0))
    prism(m, (0, 0, 126), (0, 0, 132), 10, 12, block(1))                     # glowica statywu
    prism(m, (0, 0, 132), (0, 0, 136), 8.5, 12, block(2))                    # spodarka (trojnog ze sruba)
    prism(m, (0, 0, 136), (0, 0, 141), 8.0, 12, block(1))                    # podstawa alidady
    box(m, -10, 0, 151, 5, 10, 22, block(1))                                 # lewy slupek alidady
    box(m, 10, 0, 151, 5, 10, 22, block(1))                                  # prawy slupek alidady
    box(m, 0, 0, 164, 25, 7, 3, block(1))                                    # mostek / uchwyt nad luneta
    box(m, 0, 0, 168, 3, 14, 3, block(2))                                    # raczka do przenoszenia
    prism(m, (-12, 0, 150), (12, 0, 150), 2.0, 12, block(2))                 # os pozioma lunety
    prism(m, (0, -9, 150), (0, 13, 150), 4.2, 12, block(1))                  # tubus lunety, os = 150 cm
    prism(m, (0, 13, 150), (0, 19, 150), 5.4, 12, block(2))                  # obiektyw
    prism(m, (0, -14, 150), (0, -9, 150), 3.0, 12, block(2))                 # okular
    box(m, 12.6, 0, 148, 1.5, 8, 10, block(3))                               # panel sterowania (pomaranczowy)
    box(m, 0, 9, 142, 10, 2, 4, block(2))                                    # wyswietlacz/libela na podstawie
    return m


def build_reper():
    m = Mesh()
    prism(m, (0, 0, 0), (0, 0, 10), 3.5, 12, block(1))            # słupek znaku wysokościowego
    prism(m, (0, 0, 10), (0, 0, 12), 5.0, 12, block(4))           # czerwona główka, na niej staje łata
    return m


# ---------- zapis .3DS ----------
def chunk(cid, payload):
    return struct.pack("<HI", cid, 6 + len(payload)) + payload


def cstr(s): return s.encode("ascii") + b"\0"


def to_3ds(name, mesh, materials):
    """materials: lista (nazwa materiału, plik tekstury); mesh.fm wskazuje materiał każdego trójkąta."""
    sw = (lambda p: p) if UP == "Z" else (lambda p: (p[0], p[2], p[1]))
    verts = b"".join(struct.pack("<3f", *sw(p)) for p in mesh.v)
    uvs = b"".join(struct.pack("<2f", u, v) for u, v in mesh.uv)
    faces = b"".join(struct.pack("<4H", a, b, c, 0) for a, b, c in mesh.f)
    groups = b""
    for mi, (mname, _) in enumerate(materials):
        idx = [i for i, m in enumerate(mesh.fm) if m == mi]
        if idx:
            groups += chunk(0x4130, cstr(mname) + struct.pack("<H", len(idx)) + b"".join(struct.pack("<H", i) for i in idx))
    tri = (chunk(0x4110, struct.pack("<H", len(mesh.v)) + verts) +
           chunk(0x4140, struct.pack("<H", len(mesh.uv)) + uvs) +
           chunk(0x4120, struct.pack("<H", len(mesh.f)) + faces + groups))
    mats = b"".join(chunk(0xAFFF, chunk(0xA000, cstr(mn)) +
                          chunk(0xA020, chunk(0x0011, bytes([255, 255, 255]))) +
                          chunk(0xA200, chunk(0xA300, cstr(tf)))) for mn, tf in materials)
    editor = chunk(0x3D3D, chunk(0x0100, struct.pack("<f", 1.0)) + mats + chunk(0x4000, cstr(name) + chunk(0x4100, tri)))
    return chunk(0x4D4D, chunk(0x0002, struct.pack("<I", 3)) + editor)


# ---------- tekstury .TGA ----------
def tga(path, w, h, pixel):
    rows = b"".join(bytes(c for x in range(w) for c in pixel(x, y)[::-1]) for y in range(h))  # BGR
    path.write_bytes(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 24, 0) + rows)


def tga32(path, w, h, pixel):
    rows = b"".join(bytes(c for x in range(w) for c in (lambda p: (p[2], p[1], p[0], p[3]))(pixel(x, y))) for y in range(h))
    path.write_bytes(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 8) + rows)


def reticle_pixel(w, h):
    """Luneta: czarna maska poza elipsą (okrąg na ekranie 16:9) i cienki krzyż kresek z białą obwódką."""
    cx, cy = (w - 1) / 2, (h - 1) / 2
    rx, ry = 0.55 * h * 1.125, 0.98 * h / 2

    def px(x, y):
        e = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
        if e > 1:
            return (0, 0, 0, 255)
        if e > 0.97:
            return (0, 0, 0, int(255 * (e - 0.97) / 0.03))      # miękka krawędź
        dx, dy = abs(x - cx), abs(y - cy)
        if dx < 0.9 or dy < 0.9:
            return (0, 0, 0, 255)                                 # kreska
        if dx < 2.0 or dy < 2.0:
            return (255, 255, 255, 190)                           # biała obwódka, widać na czarnej łacie
        return (0, 0, 0, 0)
    return px


def hair_pixel(w, h):
    """Kreska dalmiercza: czarny środek z białą obwódką (jak główny krzyż); rozciągana na widok ~1200x28."""
    def px(x, y):
        return (0, 0, 0, 255) if 2 <= y < h - 2 else (255, 255, 255, 190)
    return px


def panel_pixel(w, h):
    """Tło pól tekstowych: ciemne, półprzezroczyste (złotą ramkę rysują osobne paski LVL_GOLDH/V, żeby miała stałą grubość)."""
    return lambda x, y: (8, 8, 16, 225)


GOLD = [(40, 25, 8), (120, 85, 20), (205, 165, 50), (245, 215, 120), (215, 175, 60), (150, 110, 30), (90, 60, 15), (30, 18, 5)]   # ramka jak w menu Gothica


def gold_pixel(horizontal):
    """Pasek ramki 8x8 px: gradient ciemny brąz -> złoto -> jasne złoto -> brąz, w poprzek paska."""
    return (lambda x, y: GOLD[y] + (255,)) if horizontal else (lambda x, y: GOLD[x] + (255,))


FLIP_V = False   # jeśli cyfry na łacie są w grze do góry nogami, ustaw True i wygeneruj ponownie
RW, RH = 256, 2048                       # jeden odcinek łaty: 12 cm x 50 cm  (21 px/cm w poziomie, 41 px/cm w pionie)
PXCM_V = RH / SEG_CM
BLACK, RED, WHITE = (20, 20, 20), (200, 25, 25), (245, 245, 240)
DIGITS = {
    "0": ("01110", "10001", "10011", "10101", "11001", "10001", "01110"), "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    "2": ("01110", "10001", "00001", "00010", "00100", "01000", "11111"), "3": ("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
    "4": ("00010", "00110", "01010", "10010", "11111", "00010", "00010"), "5": ("11111", "10000", "11110", "00001", "00001", "10001", "01110"),
    "6": ("00110", "01000", "10000", "11110", "10001", "10001", "01110"), "7": ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    "8": ("01110", "10001", "10001", "01110", "10001", "10001", "01110"), "9": ("01110", "10001", "10001", "01111", "00001", "00010", "01100"),
}
LABEL_X0, SCALE_X0 = 4, 120              # kolumna opisów | pole podziałki (px)
SPINE = 32                               # szerokość grzbietu litery E (px)
CELL_W, CELL_H = 10, 24                  # komórka glifu: 10 px (0,47 cm) x 24 px (0,59 cm) -> cyfra 2,3 x 4,1 cm
GLYPH_W = 5 * CELL_W


def glyph_on(ch, gx, gy):
    """gx 0..49, gy 0..167 (od góry) -> czy piksel cyfry jest zapalony."""
    return DIGITS[ch][gy // CELL_H][gx // CELL_W] == "1"


def rod_pixel(w, h, seg):
    def px(x, y):
        yy = h - 1 - y if FLIP_V else y
        cm = seg * SEG_CM + yy / PXCM_V             # wysokość na łacie [cm]
        d = int(cm // 10)                           # numer decymetra od podstawy łaty (0..29)
        c = cm - 10 * d                              # cm wewnątrz decymetra (0..10)
        ink = RED if (d // 10) % 2 == 1 else BLACK   # nieparzyste metry czerwone
        if x >= SCALE_X0:                            # podziałka: E co 5 cm, pręt i przerwa po 1 cm
            half, j = divmod(min(int(c), 9), 5)
            sx = x - SCALE_X0
            spine = sx < SPINE if half == 0 else sx >= (w - SCALE_X0) - SPINE   # druga połowa dm: lustrzane E
            return ink if (spine or j % 2 == 0) else WHITE
        if d >= 1 and c < 0.15 and x >= LABEL_X0:    # kreska początku decymetra (1,5 mm)
            return ink
        if d >= 1:                                   # opis: liczba decymetrów od podstawy (np. 14 = 1,40 m)
            label = str(d)
            gy = int(c * PXCM_V - 0.6 * PXCM_V)      # piksele od dołu decymetra; glif zaczyna się 0,6 cm nad kreską
            if 0 <= gy < 7 * CELL_H:
                gy = 7 * CELL_H - 1 - gy
                for k, ch in enumerate(label):
                    gx = x - (LABEL_X0 + (2 - len(label)) * 29 + k * (GLYPH_W + 8))
                    if 0 <= gx < GLYPH_W and glyph_on(ch, gx, gy):
                        return ink
        return WHITE
    return px


def instr_pixel(x, y):                   # atlas 8 bloków po 16 px
    return [(112, 78, 46), (150, 156, 164), (35, 35, 40), (235, 120, 20), (225, 15, 15)][min(x // 16, 4)]


def hivis_pixel(x, y):                   # atlas 4 bloki po 16 px
    return [(255, 205, 0), (215, 220, 225), (40, 40, 45), (255, 230, 90)][min(x // 16, 3)]


def main():
    if len(sys.argv) < 2:
        sys.exit("Uzycie: python make_models.py <katalog gry Gothic II>")
    game = Path(sys.argv[1])
    meshes, tex = game / "_work/Data/Meshes", game / "_work/Data/Textures"
    meshes.mkdir(parents=True, exist_ok=True)
    tex.mkdir(parents=True, exist_ok=True)
    for k in range(NSEG):
        tga(tex / f"LVL_ROD{k}.TGA", RW, RH, rod_pixel(RW, RH, k))
    tga(tex / "LVL_INSTR.TGA", 128, 16, instr_pixel)
    tga(tex / "LVL_HIVIS.TGA", 64, 16, hivis_pixel)
    tga32(tex / "LVL_RETICLE.TGA", 2048, 1024, reticle_pixel(2048, 1024))   # 2x gęstsza => krzyż ok. 2x cieńszy na ekranie
    tga32(tex / "LVL_PANEL.TGA", 64, 64, panel_pixel(64, 64))
    tga32(tex / "LVL_HAIR.TGA", 16, 8, hair_pixel(16, 8))
    tga32(tex / "LVL_GOLDH.TGA", 8, 8, gold_pixel(True))      # górny/dolny pasek ramki paneli
    tga32(tex / "LVL_GOLDV.TGA", 8, 8, gold_pixel(False))     # lewy/prawy pasek
    one = [("LVL_INSTR", "LVL_INSTR.TGA")]
    for fname, mesh, mats in (("LVL_ROD", build_rod(), [(f"LVL_ROD{k}", f"LVL_ROD{k}.TGA") for k in range(NSEG)]),
                              ("LVL_INSTR", build_instrument(), one),
                              ("LVL_PLATE", build_plate(), one),
                              ("LVL_POLE", build_pole(), one),
                              ("LVL_TACHY", build_tachy(), one),
                              ("LVL_PRISM13", build_prism_pole(130.0), one),
                              ("LVL_PRISM15", build_prism_pole(150.0), one),
                              ("LVL_PRISM18", build_prism_pole(180.0), one),
                              ("LVL_PRISM20", build_prism_pole(200.0), one),
                              ("LVL_REPER", build_reper(), one)):
        (meshes / f"{fname}.3DS").write_bytes(to_3ds(fname, mesh, mats))
        print(f"{fname}.3DS: {len(mesh.v)} wierzcholkow, {len(mesh.f)} trojkatow, {len(mats)} mat.")
    for f in sorted((Path(__file__).parent / "imported").glob("LVL_*")):     # kask i kamizelka z import_glb.py (CC-BY, ATTRIBUTION.txt)
        shutil.copy(f, (meshes if f.suffix.upper() == ".3DS" else tex) / f.name)


if __name__ == "__main__":
    main()
