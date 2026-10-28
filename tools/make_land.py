#!/usr/bin/env python3
"""Generate Source/Land.h: Natural Earth land and lake polygons for the map and the Earth view (public domain).

Two levels of detail:
  fine   (lod 0): ne_10m_land, Douglas-Peucker 0.003 deg, lakes from ne_10m_lakes larger than ~30 km2, cut into 5-degree tiles
  coarse (lod 1): ne_50m_land, 0.02 deg, only big lakes, cut into 10-degree tiles
Tiles keep every piece small, so the plug-in only touches the land near the place. Points are stored as int16 steps of 0.001 deg
after an absolute first point (int32), which keeps the header small.
Usage: python3 tools/make_land.py <dir with ne_10m_land.geojson ne_50m_land.geojson ne_10m_lakes.geojson>
GeoJSON from https://github.com/nvkelso/natural-earth-vector (geojson/).
"""
import json, math, sys, os

SRC = sys.argv[1] if len(sys.argv) > 1 else '.'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Source', 'Land.h')

def rings_of(geom):
    t = geom['type']; c = geom['coordinates']
    if t == 'Polygon': return c
    if t == 'MultiPolygon': return [r for poly in c for r in poly]
    return []

def dp_open(pts, tol):
    if len(pts) < 3: return pts
    keep = [False] * len(pts); keep[0] = keep[-1] = True; stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop(); ax, ay = pts[a]; bx, by = pts[b]; dx, dy = bx - ax, by - ay; L = math.hypot(dx, dy) or 1e-12
        best, bi = -1.0, -1
        for i in range(a + 1, b):
            px, py = pts[i]; d = abs(dy * px - dx * py + bx * ay - by * ax) / L
            if d > best: best, bi = d, i
        if best > tol: keep[bi] = True; stack += [(a, bi), (bi, b)]
    return [p for p, k in zip(pts, keep) if k]

def dp(pts, tol):
    if len(pts) > 3 and pts[0] == pts[-1]:
        i = max(range(len(pts)), key=lambda k: (pts[k][0] - pts[0][0]) ** 2 + (pts[k][1] - pts[0][1]) ** 2)
        if 0 < i < len(pts) - 1: return dp_open(pts[:i + 1], tol) + dp_open(pts[i:], tol)[1:]
    return dp_open(pts, tol)

def clip(poly, x0, y0, x1, y1):
    def cl(pts, inside, inter):
        out = []
        for i in range(len(pts)):
            cur, prev = pts[i], pts[i - 1]
            if inside(cur):
                if not inside(prev): out.append(inter(prev, cur))
                out.append(cur)
            elif inside(prev): out.append(inter(prev, cur))
        return out
    def ix(x): return lambda p, q: (x, p[1] + (q[1] - p[1]) * (x - p[0]) / (q[0] - p[0]))
    def iy(y): return lambda p, q: (p[0] + (q[0] - p[0]) * (y - p[1]) / (q[1] - p[1]), y)
    pts = poly
    for ins, it in ((lambda p: p[0] >= x0, ix(x0)), (lambda p: p[0] <= x1, ix(x1)), (lambda p: p[1] >= y0, iy(y0)), (lambda p: p[1] <= y1, iy(y1))):
        if not pts: break
        pts = cl(pts, ins, it)
    return pts

def ring_area_km2(r):
    a = 0.0; lat0 = sum(p[1] for p in r) / len(r); k = 111.32 ** 2 * math.cos(math.radians(lat0))
    for i in range(len(r)): x1, y1 = r[i - 1]; x2, y2 = r[i]; a += x1 * y2 - x2 * y1
    return abs(a) * 0.5 * k

def pieces(path, tol, tile, kind, lod, min_km2=0.0):
    gj = json.load(open(os.path.join(SRC, path))); out = []
    for f in gj['features']:
        for r in rings_of(f['geometry']):
            r = [tuple(p[:2]) for p in r]
            if len(r) < 4: continue
            if min_km2 and ring_area_km2(r) < min_km2: continue
            r = dp(r, tol)
            if len(r) < 4: continue
            if r[0] == r[-1]: r = r[:-1]
            xs = [p[0] for p in r]; ys = [p[1] for p in r]
            for tx in range(int(math.floor(min(xs) / tile)) * tile, int(math.ceil(max(xs) / tile)) * tile, tile):
                for ty in range(int(math.floor(min(ys) / tile)) * tile, int(math.ceil(max(ys) / tile)) * tile, tile):
                    c = clip(r, tx, ty, tx + tile, ty + tile)
                    if len(c) < 3: continue
                    q = [(int(round(y * 1000)), int(round(x * 1000))) for x, y in c]     # (lat, lon) in 0.001 deg
                    dd = [q[0]] + [p for i, p in enumerate(q[1:], 1) if p != q[i - 1]]
                    if len(dd) >= 3: out.append((kind, lod, tile, dd))
    return out

allp = []
allp += pieces('ne_10m_land.geojson', 0.003, 5, 0, 0)
allp += pieces('ne_10m_lakes.geojson', 0.003, 5, 1, 0, 30.0)
allp += pieces('ne_50m_land.geojson', 0.02, 10, 0, 1)
allp += pieces('ne_10m_lakes.geojson', 0.02, 10, 1, 1, 1500.0)

deltas = []; index = []
for kind, lod, tile, q in allp:
    lats = [p[0] for p in q]; lons = [p[1] for p in q]
    off = len(deltas)
    for i in range(1, len(q)):
        deltas.append(q[i][0] - q[i - 1][0]); deltas.append(q[i][1] - q[i - 1][1])
    assert all(-32768 <= d <= 32767 for d in deltas[off:])
    index.append((q[0][0], q[0][1], min(lats) // 10, max(lats) // 10 + 1, min(lons) // 10, max(lons) // 10 + 1, off, len(q), kind, lod, tile))

with open(OUT, 'w') as f:
    f.write('// Aeola - land and lakes for the map and the Earth view.\n// Source: Natural Earth 1:10m / 1:50m land and lakes (public domain, naturalearthdata.com), simplified and cut into tiles\n'
            '// by tools/make_land.py. Generated file: do not edit.\n#pragma once\n#include <cstdint>\n\nnamespace aeola\n{\n')
    f.write('// lat0/lon0: first point in 0.001 deg; bbox in 0.01 deg; then n-1 steps (dLat, dLon) in 0.001 deg from kLandSteps[off]\n')
    f.write('struct LandPiece { int32_t lat0, lon0; int16_t latMin, latMax, lonMin, lonMax; int32_t off, n; uint8_t kind, lod, tile; };   // kind 0 land 1 lake, lod 0 fine 1 coarse, tile in degrees\n')
    f.write('static const LandPiece kLandPieces[] = {\n')
    for e in index: f.write('{%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d},\n' % e)
    f.write('};\nstatic const int kLandPieceCount = %d;\nstatic const int16_t kLandSteps[] = {\n' % len(index))
    for i in range(0, len(deltas), 40): f.write(','.join(str(d) for d in deltas[i:i + 40]) + ',\n')
    f.write('};\n} // namespace aeola\n')
pts = sum(e[7] for e in index)
print('pieces', len(index), 'points', pts, 'steps', len(deltas), 'header bytes', os.path.getsize(OUT),
      'lod0', sum(1 for e in index if e[9] == 0), 'lod1', sum(1 for e in index if e[9] == 1))
