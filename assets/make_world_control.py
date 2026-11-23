"""Jednorazowe narzedzie: analizuje modele swiatow Gothic II (repozytorium ekstraktora, paczki dist/g2nk/<swiat>) i proponuje zestaw punktow
osnowy geodezyjnej dla kazdego swiata. Wynik: src/calc/WorldControlData.hpp (dane wbudowane we wtyczke).

Uzycie:  python make_world_control.py <dist/g2nk> [--preview <katalog>] [--only khorinis]
Wymaga: numpy, trimesh (matplotlib tylko dla --preview).

Metoda (wszystko w metrach, ukladzie E, N, U; E = x_gry/100, N = z_gry/100, U = y_gry/100):
 1. world.glb -> raster wysokosci DSM (max wysokosc w komorce 1,5 m; teren + budynki).
 2. Kandydaci = siatka punktow (co 8 m, +-16 m) wokol sieci drog (waypoints.json): tam bohater moze dojsc. Odrzucane: wnetrza (wysokosc
    powierzchni != wysokosc drogi), nachylenie, uskoki/sciany, zaslonione niebo (jak GnssSkyBlockedPct we wtyczce) i bliskie przeszkody.
    Poziomy jakosci TIERS: wszedzie wybierany jest najlepszy poziom, ale na trudnym terenie dopuszczamy gorsze.
 3. Wybor: farthest-first po probkach obszaru chodzenia (promien pokrycia COVER), odstep >= MIN_SEP, nowy punkt woli widziec (linia 1,5 m nad
    gruntem) dotychczasowy w odleglosci 25-300 m. Dogeszczanie: punkt bez 2 widocznych sasiadow dostaje dodatkowego sasiada.
 4. Nazwy: najblizsza lokacja z locations.json (ASCII).
"""
import argparse
import json
import math
import struct
import unicodedata
from pathlib import Path

import numpy as np
import trimesh

RES = 1.5                    # komorka rastra [m]
M_PER_DEG_LON, M_PER_DEG_LAT = 111319.49, 110574.27          # skalibrowane na game_pos z paczek (E = lon*, N = lat*)
EYE = 1.5                                                     # wysokosc oka tachimetru i lustra [m]
WORLD_KEYS = {"khorinis": "NEWWORLD", "gornicza-dolina": "OLDWORLD", "jarkendar": "ADDONWORLD", "irdorath": "DRAGONISLAND"}
# Progi jakosci kandydata: (nachylenie [deg], zaslonienie nieba [%], najblizsza przeszkoda [m], uskok w oknie 3x3 [m]).
TIERS = [(4.0, 6.0, 8.0, 0.8), (7.0, 12.0, 6.0, 1.2), (10.0, 25.0, 4.0, 2.0)]
COVER, MIN_SEP, MAX_N = 130.0, 70.0, 28
MAP_PX, MAP_MARGIN = 512, 80.0         # tekstura mapy [px] i margines wokol obszaru chodzenia [m]


def ascii_name(s):
    s = s.replace("ł", "l").replace("Ł", "L")
    return unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode("ascii")


def load_dsm(glb):
    """Raster wysokosci (max w komorce RES) z world.glb; geometria bez tekstur, rasteryzacja porcjami (pamiec ograniczona)."""
    sc = trimesh.load(str(glb), process=False)
    vs, fs, off = [], [], 0
    for node in sc.graph.nodes_geometry:                       # sama geometria po transformacjach grafu sceny
        transform, gname = sc.graph[node]
        g = sc.geometry[gname]
        vs.append(trimesh.transformations.transform_points(np.asarray(g.vertices, dtype=np.float64), transform))
        fs.append(np.asarray(g.faces) + off); off += len(g.vertices)
    del sc
    v = np.concatenate(vs); faces = np.concatenate(fs)
    verts = np.c_[v[:, 0], -v[:, 2], v[:, 1]]                  # glTF = (E, U, -N) -> (E, N, U)
    e0, n0 = verts[:, 0].min() - 5, verts[:, 1].min() - 5
    w = int((verts[:, 0].max() - e0) / RES) + 6
    h = int((verts[:, 1].max() - n0) / RES) + 6
    zmax = np.full((h, w), -np.inf, dtype=np.float32)
    for k in range(0, len(faces), 20000):
        sv, sf = trimesh.remesh.subdivide_to_size(verts, faces[k:k + 20000], max_edge=RES * 1.5)
        samples = np.concatenate([sv, sv[sf].mean(axis=1)])
        ix = ((samples[:, 0] - e0) / RES).astype(np.int64)
        iy = ((samples[:, 1] - n0) / RES).astype(np.int64)
        ok = (ix >= 0) & (ix < w) & (iy >= 0) & (iy < h)
        np.maximum.at(zmax, (iy[ok], ix[ok]), samples[ok, 2].astype(np.float32))
    return {"e0": e0, "n0": n0, "zmax": zmax.astype(np.float64), "w": w, "h": h}


def cell(d, e, n):
    return int((n - d["n0"]) / RES), int((e - d["e0"]) / RES)


def sky_metrics(d, e, n, z0, ant=2.0):
    """Zasloniecie nieba [%] (jak GnssSkyBlockedPct) i najblizsza przeszkoda [m] z rastra - przyblizenie promieni z wtyczki."""
    rad = math.pi / 180.0
    s10 = math.sin(10 * rad)
    vis, nearest = 0.0, 1e9
    steps = np.arange(RES, 120.0, RES)
    for k in range(36):
        az = (k * 10 + 5) * rad
        iy = ((n + steps * math.cos(az) - d["n0"]) / RES).astype(int)
        ix = ((e + steps * math.sin(az) - d["e0"]) / RES).astype(int)
        ok = (iy >= 0) & (iy < d["h"]) & (ix >= 0) & (ix < d["w"])
        zz = np.full(steps.shape, -np.inf)
        zz[ok] = d["zmax"][iy[ok], ix[ok]]
        ang = np.degrees(np.arctan2(zz - (z0 + ant), steps))
        horizon = max(0.0, float(ang.max()))
        hit = np.where(ang >= 5.0)[0]
        if hit.size:
            nearest = min(nearest, float(steps[hit[0]]))
        vis += (1 - math.sin(max(10.0, horizon) * rad)) / (1 - s10)
    return 100.0 * (1 - vis / 36.0), nearest


def los(d, a, b):
    """Linia widocznosci oko-lustro (1,5 m nad gruntem na obu koncach) nad rastrem; a, b = (E, N, U)."""
    dist = math.hypot(b[0] - a[0], b[1] - a[1])
    if dist < 1:
        return True
    t = np.arange(RES, dist - RES, RES) / dist
    e = a[0] + t * (b[0] - a[0]); n = a[1] + t * (b[1] - a[1])
    line = (a[2] + EYE) + t * (b[2] - a[2])
    iy = ((n - d["n0"]) / RES).astype(int); ix = ((e - d["e0"]) / RES).astype(int)
    ok = (iy >= 0) & (iy < d["h"]) & (ix >= 0) & (ix < d["w"])
    z = np.full(t.shape, -np.inf); z[ok] = d["zmax"][iy[ok], ix[ok]]
    return bool(np.all(z < line - 0.1))


def slope_map(d):
    z = np.where(np.isfinite(d["zmax"]), d["zmax"], np.nan)
    gy, gx = np.gradient(z, RES)
    return np.degrees(np.arctan(np.hypot(gx, gy)))


def area_samples(nodes, edges):
    """Probki obszaru chodzenia: wezly i punkty na krawedziach sieci drog, jako (E, N, U) (U = wysokosc wezla)."""
    pts = [(e, n, u) for _, e, n, u in nodes]
    for i, j in edges:
        a, b = nodes[i], nodes[j]
        for t in (0.25, 0.5, 0.75):
            pts.append((a[1] + t * (b[1] - a[1]), a[2] + t * (b[2] - a[2]), a[3] + t * (b[3] - a[3])))
    return np.array(pts)


def candidates(d, area):
    slope = slope_map(d)
    zmax = d["zmax"]
    cand, seen = [], set()
    why = {"brzeg": 0, "wnetrze": 0, "nachylenie": 0, "niebo": 0, "przeszkoda": 0, "uskok": 0}
    tiers = [0] * len(TIERS)
    offsets = [-16, -8, 0, 8, 16]
    for ae, an, au in area:
        for do in offsets:
            for dn in offsets:
                e, n = ae + do, an + dn
                key = (round(e / 8), round(n / 8))
                if key in seen:
                    continue
                seen.add(key)
                iy, ix = cell(d, e, n)
                if iy < 3 or ix < 3 or iy >= d["h"] - 3 or ix >= d["w"] - 3:
                    why["brzeg"] += 1; continue
                win = zmax[iy - 2:iy + 3, ix - 2:ix + 3]
                if not np.isfinite(win).all():
                    why["brzeg"] += 1; continue
                z0 = float(np.median(win))
                if abs(z0 - au) > 2.0:                          # wnetrze / dach nad droga / inny poziom terenu niz najblizsza droga
                    why["wnetrze"] += 1; continue
                step = float(np.ptp(zmax[iy - 1:iy + 2, ix - 1:ix + 2]))
                sl = float(np.max(slope[iy - 1:iy + 2, ix - 1:ix + 2]))
                if sl > TIERS[-1][0] or step > TIERS[-1][3]:
                    why["nachylenie" if sl > TIERS[-1][0] else "uskok"] += 1; continue
                blocked, nearest = sky_metrics(d, e, n, z0)
                tier = next((t for t, (s_, b_, n_, st_) in enumerate(TIERS) if sl <= s_ and blocked <= b_ and nearest >= n_ and step <= st_), None)
                if tier is None:
                    why["niebo" if blocked > TIERS[-1][1] else "przeszkoda"] += 1; continue
                tiers[tier] += 1
                cand.append({"e": e, "n": n, "u": z0, "blocked": blocked, "nearest": nearest, "slope": sl, "tier": tier})
    print("  odrzucone:", why, "z", len(seen), "| kandydaci wg poziomu:", tiers)
    return cand


def vis_count(d, cand, i, chosen):
    a = (cand[i]["e"], cand[i]["n"], cand[i]["u"])
    return [j for j in chosen if j != i and 25 <= math.hypot(cand[j]["e"] - a[0], cand[j]["n"] - a[1]) <= 300
            and los(d, a, (cand[j]["e"], cand[j]["n"], cand[j]["u"]))]


def select(d, cand, area):
    if not cand:
        return []
    ce = np.array([[c["e"], c["n"]] for c in cand])
    ap = area[:, :2]
    tier = np.array([c["tier"] for c in cand])
    centre = np.median(ap, axis=0)
    seed = int(np.argmin(np.hypot(*(ce - centre).T) + 4.0 * np.array([c["blocked"] for c in cand]) + 150.0 * tier))
    chosen = [seed]
    dist = np.hypot(*(ap - ce[seed]).T)
    while len(chosen) < MAX_N:
        far = int(np.argmax(dist))
        if dist[far] < COVER:
            break
        order = np.argsort(np.hypot(*(ce - ap[far]).T) + 60.0 * tier)
        pick = None
        for i in order[:300]:
            if i in chosen or min(math.hypot(*(ce[i] - ce[j])) for j in chosen) < MIN_SEP:
                continue
            if vis_count(d, cand, int(i), chosen):
                pick = int(i); break
        if pick is None:                                        # nic widocznego: odpusc widocznosc, bierz najblizszego z odstepem
            for i in order[:300]:
                if i not in chosen and min(math.hypot(*(ce[i] - ce[j])) for j in chosen) >= MIN_SEP:
                    pick = int(i); break
        if pick is None:
            dist[far] = 0.0                                     # tego miejsca nie da sie pokryc
            continue
        chosen.append(pick)
        dist = np.minimum(dist, np.hypot(*(ap - ce[pick]).T))
    return chosen


def densify(d, cand, chosen):
    """Punkt bez 2 widocznych sasiadow dostaje dodatkowego sasiada (kandydat 40-200 m od niego, widzacy go i inny punkt)."""
    ce = np.array([[c["e"], c["n"]] for c in cand])
    for _ in range(3):
        added = False
        for i in list(chosen):
            if len(vis_count(d, cand, i, chosen)) >= 2 or len(chosen) >= MAX_N + 8:
                continue
            best = None
            for j in np.argsort(np.hypot(*(ce - ce[i]).T)):
                dd = math.hypot(*(ce[j] - ce[i]))
                if dd > 200:
                    break
                if j in chosen or dd < 40 or min(math.hypot(*(ce[j] - ce[k])) for k in chosen) < 40:
                    continue
                v = vis_count(d, cand, int(j), chosen)
                if i in v and len(v) >= 2:
                    score = cand[j]["tier"] * 100 + dd
                    if best is None or score < best[0]:
                        best = (score, int(j))
                elif i in v and best is None:
                    best = (cand[j]["tier"] * 100 + dd + 500, int(j))
            if best:
                chosen.append(best[1]); added = True
        if not added:
            break
    return chosen


def process(pkg, key):
    d = load_dsm(pkg / "world.glb")
    wp = json.loads((pkg / "waypoints.json").read_text(encoding="utf-8"))
    nodes = [(nm, lon * M_PER_DEG_LON, lat * M_PER_DEG_LAT, h) for nm, lon, lat, h, _ in wp["nodes"]]
    area = area_samples(nodes, wp["edges"])
    cand = candidates(d, area)
    chosen = densify(d, cand, select(d, cand, area))
    locs = json.loads((pkg / "locations.json").read_text(encoding="utf-8"))["locations"]
    lpos = [(ascii_name(l["name"]), l["center"][0] * M_PER_DEG_LON, l["center"][1] * M_PER_DEG_LAT) for l in locs]
    rows = []
    for i in chosen:
        c = cand[i]
        place = min(lpos, key=lambda l: math.hypot(l[1] - c["e"], l[2] - c["n"])) if lpos else ("okolice", 0, 0)
        rows.append({"e": round(c["e"], 3), "n": round(c["n"], 3), "u": round(c["u"], 3), "place": place[0][:30], "vis": len(vis_count(d, cand, i, chosen)),
                     "blocked": c["blocked"], "nearest": c["nearest"], "slope": c["slope"], "tier": c["tier"]})
    rows.sort(key=lambda r: (r["place"], r["e"]))
    for k, r in enumerate(rows, 1):
        r["id"] = "G%02d" % k
    print("%s: kandydatow %d, wybrano %d, bez 2 widocznych sasiadow: %d" % (key, len(cand), len(rows), sum(1 for r in rows if r["vis"] < 2)))
    return key, rows, d, cand, area, nodes, wp["edges"]


def save_tga24(path, rgb):
    """Nieskompresowany TGA 24-bit, poczatek lewy dolny (jak assets/make_models.py): rgb to tablica (h, w, 3) z gorna linia = polnoc."""
    h, w, _ = rgb.shape
    bgr = rgb[::-1, :, ::-1]                                    # TGA zaczyna od dolnego wiersza, kanaly BGR
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 24, 0) + np.ascontiguousarray(bgr).tobytes())


def map_bounds(area):
    """Kwadratowy zakres mapy (E0, N0, bok) wokol obszaru chodzenia z marginesem, zaokraglony do 50 m."""
    e0, e1 = area[:, 0].min() - MAP_MARGIN, area[:, 0].max() + MAP_MARGIN
    n0, n1 = area[:, 1].min() - MAP_MARGIN, area[:, 1].max() + MAP_MARGIN
    side = math.ceil(max(e1 - e0, n1 - n0) / 50.0) * 50.0
    return ((e0 + e1) / 2 - side / 2, (n0 + n1) / 2 - side / 2, side)


def render_map(d, area, nodes, edges, bounds, out):
    """Tekstura mapy MAP_PX x MAP_PX: cieniowany teren (wysokosc z DSM) i siec drog; polnoc u gory. Punkty osnowy rysuje gra."""
    from PIL import Image, ImageDraw
    from matplotlib import cm
    from matplotlib.colors import LightSource
    e0, n0, side = bounds
    px = side / MAP_PX
    ee = e0 + (np.arange(MAP_PX) + 0.5) * px
    nn = n0 + (np.arange(MAP_PX)[::-1] + 0.5) * px              # wiersz 0 = polnoc
    ix = ((ee - d["e0"]) / RES).astype(int)
    iy = ((nn - d["n0"]) / RES).astype(int)
    z = np.full((MAP_PX, MAP_PX), np.nan)
    okx = (ix >= 0) & (ix < d["w"]); oky = (iy >= 0) & (iy < d["h"])
    z[np.ix_(oky, okx)] = d["zmax"][np.ix_(iy[oky], ix[okx])]       # poza modelem swiata i dziury w siatce = morze
    z[~np.isfinite(z)] = np.nan
    lo, hi = np.nanpercentile(z, 2), np.nanpercentile(z, 98)
    z = np.where(np.isfinite(z), z, lo)
    shaded = LightSource(azdeg=315, altdeg=45).shade(np.clip(z, lo, hi), cmap=cm.terrain, blend_mode="soft", vert_exag=2.0, dx=px, dy=px)
    img = Image.fromarray((shaded[:, :, :3] * 255).astype(np.uint8))
    dr = ImageDraw.Draw(img)
    def pos(e, n):
        return ((e - e0) / px, MAP_PX - (n - n0) / px)
    for i, j in edges:                                          # siec drog: cienkie biale linie
        dr.line([pos(nodes[i][1], nodes[i][2]), pos(nodes[j][1], nodes[j][2])], fill=(255, 255, 255), width=1)
    save_tga24(out, np.asarray(img))


def preview(out, key, rows, d, cand, area):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    z = np.where(np.isfinite(d["zmax"]), d["zmax"], np.nan)
    ext = [d["e0"], d["e0"] + d["w"] * RES, d["n0"], d["n0"] + d["h"] * RES]
    fig, ax = plt.subplots(figsize=(11, 9), dpi=110)
    ax.imshow(z, origin="lower", extent=ext, cmap="terrain", vmin=np.nanpercentile(z, 2), vmax=np.nanpercentile(z, 98))
    ax.plot(area[:, 0], area[:, 1], ".", color="white", ms=1, alpha=0.6)
    ax.plot([c["e"] for c in cand], [c["n"] for c in cand], ".", color="yellow", ms=2)
    pts = np.array([[r["e"], r["n"]] for r in rows])
    for i, r in enumerate(rows):                                # linie widocznosci miedzy punktami 25-300 m
        for j in range(i + 1, len(rows)):
            q = rows[j]
            if 25 <= math.hypot(q["e"] - r["e"], q["n"] - r["n"]) <= 300 and los(d, (r["e"], r["n"], r["u"]), (q["e"], q["n"], q["u"])):
                ax.plot([r["e"], q["e"]], [r["n"], q["n"]], "-", color="red", lw=0.6, alpha=0.5)
    for r in rows:
        ax.plot(r["e"], r["n"], "o", color="red", ms=6)
        ax.annotate(r["id"], (r["e"], r["n"]), color="black", fontsize=7, xytext=(3, 3), textcoords="offset points")
    ax.set_xlim(pts[:, 0].min() - 100, pts[:, 0].max() + 100); ax.set_ylim(pts[:, 1].min() - 100, pts[:, 1].max() + 100)
    ax.set_title("%s: osnowa (czerwone, linie = widocznosc), kandydaci (zolte), siec drog (biale)" % key)
    Path(out).mkdir(parents=True, exist_ok=True)
    fig.savefig(Path(out) / (key + ".png"))
    plt.close(fig)


def header(all_rows, maps):
    o = ["// Wygenerowane przez assets/make_world_control.py - nie edytuj recznie.",
         "// Osnowa geodezyjna swiatow Gothic II (uklad lokalny wtyczki: E = x/100, N = z/100, H = y/100 w metrach; do dodania N0/E0).",
         "#pragma once", "", "struct WorldControlRow { const char* world; const char* id; const char* place; double e, n, h; };", "",
         "inline const WorldControlRow WORLD_CONTROL[] = {"]
    for key, rows in all_rows:
        for r in rows:
            o.append('  {"%s", "%s", "%s", %.3f, %.3f, %.3f},' % (key, r["id"], r["place"], r["e"], r["n"], r["u"]))
    o += ["};", "inline const int WORLD_CONTROL_COUNT = (int)(sizeof(WORLD_CONTROL) / sizeof(WORLD_CONTROL[0]));", "",
          "// Mapa swiata (tekstura LVL_MAP_<swiat>.TGA, polnoc u gory): lewy dolny rog (E0, N0) i bok kwadratu [m] w ukladzie lokalnym.",
          "struct WorldMapInfo { const char* world; double e0, n0, side; };", "inline const WorldMapInfo WORLD_MAPS[] = {"]
    for key, (e0, n0, side) in maps:
        o.append('  {"%s", %.1f, %.1f, %.1f},' % (key, e0, n0, side))
    o += ["};", "inline const int WORLD_MAPS_COUNT = (int)(sizeof(WORLD_MAPS) / sizeof(WORLD_MAPS[0]));", ""]
    return "\n".join(o)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dist")
    ap.add_argument("--preview")
    ap.add_argument("--only")
    ap.add_argument("--maps-dir", default=str(Path(__file__).resolve().parent / "worldmaps"))
    ap.add_argument("--out", default=str(Path(__file__).resolve().parent.parent / "src" / "calc" / "WorldControlData.hpp"))
    a = ap.parse_args()
    all_rows, maps = [], []
    for folder, key in WORLD_KEYS.items():
        if a.only and a.only != folder:
            continue
        key, rows, d, cand, area, nodes, edges = process(Path(a.dist) / folder, key)
        if rows:
            all_rows.append((key, rows))
            bounds = map_bounds(area)
            maps.append((key, bounds))
            if not a.only:
                render_map(d, area, nodes, edges, bounds, Path(a.maps_dir) / ("LVL_MAP_%s.TGA" % key))
        if a.preview and rows:
            preview(a.preview, key, rows, d, cand, area)
        for r in rows:
            print("  %s %-30s E %9.2f N %9.2f U %7.2f  zasl. %.1f%% przeszk. %.0f m nachyl. %.1f deg  poziom %d  widzi %d" %
                  (r["id"], r["place"], r["e"], r["n"], r["u"], r["blocked"], r["nearest"], r["slope"], r["tier"], r["vis"]))
    if not a.only:
        Path(a.out).write_text(header(all_rows, maps), encoding="utf-8")
        print("zapisano", a.out)


if __name__ == "__main__":
    main()
