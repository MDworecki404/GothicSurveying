"""Jednorazowe narzędzie: konwertuje kask i kamizelkę z .glb (Sketchfab, CC-BY 4.0) na lekkie .3DS + .TGA do assets/imported/.
Użycie: python import_glb.py <construction_hat.glb> <safety_vest.glb>     (wymaga numpy i Pillow)
Kask: scalane siatki, uproszczone przez klastrowanie wierzchołków (~3000 trójkątów), żółty kolor z atlasu LVL_HIVIS.
Kamizelka: oryginalna siatka (~4400 trójkątów) i tekstura bazowa zmniejszona do 512x512.
Układ wyjściowy jak w make_models.py: Z w górę, przód = +Y, jednostki cm, kask od rondem w z=0, kamizelka wyśrodkowana."""
import json
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).parent))
import make_models as mm

OUT = Path(__file__).parent / "imported"
HAT_WIDTH_CM, VEST_HEIGHT_CM = 22.0, 60.0
HAT_TRIS = 3000
VEST_XY_SCALE, VEST_UP_CM = 1.2, 6.0      # kamizelka szersza i grubsza od tułowia (inaczej ciało ją ucina), wyżej na klatce


def load_glb(path):
    d = Path(path).read_bytes()
    off, js, binc = 12, None, None
    while off < len(d):
        cl, ct = struct.unpack("<I4s", d[off:off + 8])
        body = d[off + 8:off + 8 + cl]
        if ct == b"JSON": js = json.loads(body)
        elif ct.startswith(b"BIN"): binc = body
        off += 8 + cl
    return js, binc


def accessor(js, binc, idx):
    a = js["accessors"][idx]; bv = js["bufferViews"][a["bufferView"]]
    comp = {5126: np.float32, 5125: np.uint32, 5123: np.uint16}[a["componentType"]]
    n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
    start = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    arr = np.frombuffer(binc, dtype=comp, count=a["count"] * n, offset=start)
    return arr.reshape(-1, n) if n > 1 else arr


def node_matrix(node):
    if "matrix" in node: return np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T   # glTF: kolumnami
    return np.eye(4)


def world_meshes(js, binc):
    """Zwraca listę (pozycje Nx3 w układzie świata glTF Y-up, uv Nx2 lub None, trójkąty Mx3)."""
    out = []
    def walk(i, parent):
        node = js["nodes"][i]; m = parent @ node_matrix(node)
        if "mesh" in node:
            for prim in js["meshes"][node["mesh"]]["primitives"]:
                p = accessor(js, binc, prim["attributes"]["POSITION"]).astype(np.float64)
                p = (np.c_[p, np.ones(len(p))] @ m.T)[:, :3]
                uv = accessor(js, binc, prim["attributes"]["TEXCOORD_0"]).astype(np.float64) if "TEXCOORD_0" in prim["attributes"] else None
                out.append((p, uv, accessor(js, binc, prim["indices"]).reshape(-1, 3).astype(np.int64)))
        for c in node.get("children", []): walk(c, m)
    for r in js["scenes"][0]["nodes"]: walk(r, np.eye(4))
    return out


def to_file_axes(p, front_sign=1.0):
    """glTF (x w prawo, y w górę, z przód) -> plik (x, y = przód, z = góra); front_sign = -1 odwraca przód."""
    return np.c_[-p[:, 0], front_sign * p[:, 2], p[:, 1]]


def cluster(P, F, cell):
    q = np.floor(P / cell).astype(np.int64)
    _, inv, cnt = np.unique(q, axis=0, return_inverse=True, return_counts=True)
    inv = inv.reshape(-1)
    newP = np.zeros((len(cnt), 3))
    for k in range(3): newP[:, k] = np.bincount(inv, weights=P[:, k], minlength=len(cnt)) / cnt
    G = inv[F]
    G = G[(G[:, 0] != G[:, 1]) & (G[:, 1] != G[:, 2]) & (G[:, 0] != G[:, 2])]
    G = np.unique(np.sort(G, axis=1), axis=0)
    return newP, G


def used_only(P, F):
    keep, inv = np.unique(F.reshape(-1), return_inverse=True)
    return P[keep], inv.reshape(-1, 3)


def fill_mesh(P, uv, F):
    m = mm.Mesh()
    m.v = [tuple(float(c) for c in p) for p in P]
    m.uv = [tuple(float(c) for c in u) for u in uv]
    for a, b, c in F:
        m.f += [(int(a), int(b), int(c)), (int(a), int(c), int(b))]; m.fm += [0, 0]
    return m


def convert_hat(path):
    js, binc = load_glb(path)
    parts = world_meshes(js, binc)
    P = np.vstack([p for p, _, _ in parts]); base = 0; Fs = []
    for p, _, f in parts: Fs.append(f + base); base += len(p)
    F = np.vstack(Fs)
    # przód = strona, na którą wystaje daszek: środek bbox leży dalej od czubka kopuły niż jego środek ciężkości
    top = P[P[:, 1] > np.percentile(P[:, 1], 90)]
    front = 1.0 if (P[:, 2].min() + P[:, 2].max()) / 2 > top[:, 2].mean() else -1.0
    Q = to_file_axes(P, front)
    Q[:, 0] -= (Q[:, 0].min() + Q[:, 0].max()) / 2
    topq = Q[Q[:, 2] > np.percentile(Q[:, 2], 90)]
    Q[:, 1] -= topq[:, 1].mean(); Q[:, 2] -= Q[:, 2].min()
    Q *= HAT_WIDTH_CM / (Q[:, 0].max() - Q[:, 0].min())
    lo, hi = 0.05, 5.0
    for _ in range(25):                                   # dobierz komórkę siatki do ~HAT_TRIS trójkątów
        cell = (lo * hi) ** 0.5
        P2, G = cluster(Q, F, cell)
        if len(G) > HAT_TRIS: lo = cell
        else: hi = cell
    P2, G = cluster(Q, F, hi); P2, G = used_only(P2, G)
    print(f"kask: {len(F)} -> {len(G)} trojkatow, {len(P2)} wierzcholkow, przod = {'+' if front > 0 else '-'}Z glTF, wymiary cm {np.ptp(P2, axis=0).round(1)}")
    uv = np.tile([0.125, 0.5], (len(P2), 1))            # blok 0 atlasu LVL_HIVIS (żółty)
    (OUT / "LVL_HELMET.3DS").write_bytes(mm.to_3ds("LVL_HELMET", fill_mesh(P2, uv, G), [("LVL_HIVIS", "LVL_HIVIS.TGA")]))


def convert_vest(path):
    js, binc = load_glb(path)
    (p, uv, F), = world_meshes(js, binc)
    Q = to_file_axes(p)
    ext = np.ptp(Q, axis=0); print("kamizelka: wymiary przed skala", ext.round(3))
    Q -= (Q.min(axis=0) + Q.max(axis=0)) / 2
    Q *= VEST_HEIGHT_CM / ext[2]
    Q[:, :2] *= VEST_XY_SCALE; Q[:, 2] += VEST_UP_CM
    uv2 = np.c_[uv[:, 0], 1.0 - uv[:, 1]]                # glTF v od góry -> 3DS/TGA v od dołu
    print(f"kamizelka: {len(F)} trojkatow, wymiary cm {np.ptp(Q, axis=0).round(1)}")
    (OUT / "LVL_VEST.3DS").write_bytes(mm.to_3ds("LVL_VEST", fill_mesh(Q, uv2, F), [("LVL_VESTC", "LVL_VESTC.TGA")]))
    bv = js["bufferViews"][js["images"][js["textures"][js["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"]]["source"]]["bufferView"]]
    import io
    img = Image.open(io.BytesIO(binc[bv.get("byteOffset", 0):bv.get("byteOffset", 0) + bv["byteLength"]])).convert("RGB").resize((512, 512), Image.LANCZOS)
    mm.tga(OUT / "LVL_VESTC.TGA", 512, 512, lambda x, y: img.getpixel((x, 511 - y)))   # wiersze od dołu, jak w tga()


if __name__ == "__main__":
    if len(sys.argv) < 3: sys.exit(__doc__)
    OUT.mkdir(exist_ok=True)
    convert_hat(sys.argv[1]); convert_vest(sys.argv[2])
