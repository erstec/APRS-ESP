#!/usr/bin/env python3
"""
Download OSM map tiles for offline use in APRS-ESP.
Covers the Baltic region (lat 50-61 N, lon 14-31 E) at zoom levels 5 and 6.

Run once after cloning:
    python3 tools/download_tiles.py

Tiles are saved to data/static/tiles/{z}/{x}/{y}.png and served by the device
web server. Existing tiles are skipped (safe to re-run).

Tile data © OpenStreetMap contributors (ODbL) — https://www.openstreetmap.org/copyright
"""
import math, os, time, sys, subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(SCRIPT_DIR, '..', 'data', 'static', 'tiles')
USER_AGENT = 'APRS-ESP/2.0 tile-prefetch (+https://github.com/erstec/APRS-ESP)'

BOUNDS = dict(lat_min=50.0, lat_max=61.0, lon_min=14.0, lon_max=31.0)
ZOOMS  = [5]  # z5 only — 4 tiles, covers Northern Europe, fits both ESP32 and ESP32-S3


def lon2x(lon, z):
    return int((lon + 180) / 360 * 2**z)

def lat2y(lat, z):
    r = math.radians(lat)
    return int((1 - math.log(math.tan(r) + 1 / math.cos(r)) / math.pi) / 2 * 2**z)

def tile_range(z):
    x0 = lon2x(BOUNDS['lon_min'], z)
    x1 = lon2x(BOUNDS['lon_max'], z)
    y0 = lat2y(BOUNDS['lat_max'], z)   # north → smaller y
    y1 = lat2y(BOUNDS['lat_min'], z)   # south → larger y
    return x0, x1, y0, y1

def fetch(z, x, y):
    path = os.path.join(OUTPUT_DIR, str(z), str(x), f'{y}.png')
    if os.path.exists(path):
        return 'skip'
    os.makedirs(os.path.dirname(path), exist_ok=True)
    url = f'https://tile.openstreetmap.org/{z}/{x}/{y}.png'
    try:
        r = subprocess.run(
            ['curl', '-sfL', '--user-agent', USER_AGENT, '-o', path, url],
            timeout=20, capture_output=True)
        if r.returncode == 0 and os.path.getsize(path) > 0:
            return 'ok'
        os.remove(path)
        return f'FAIL: curl exit {r.returncode}'
    except Exception as e:
        return f'FAIL: {e}'

total = skipped = failed = 0
for z in ZOOMS:
    x0, x1, y0, y1 = tile_range(z)
    count = (x1 - x0 + 1) * (y1 - y0 + 1)
    print(f'zoom {z}: x={x0}-{x1}, y={y0}-{y1}  ({count} tiles)')
    for x in range(x0, x1 + 1):
        for y in range(y0, y1 + 1):
            result = fetch(z, x, y)
            total += 1
            if result == 'skip':
                skipped += 1
                print(f'  skip {z}/{x}/{y}.png')
            elif result == 'ok':
                size = os.path.getsize(
                    os.path.join(OUTPUT_DIR, str(z), str(x), f'{y}.png'))
                print(f'  {z}/{x}/{y}.png  ({size//1024}KB)')
            else:
                failed += 1
                print(f'  {z}/{x}/{y}.png  {result}', file=sys.stderr)
            if result == 'ok':
                time.sleep(1)   # be polite to OSM tile servers

print(f'\nDone: {total} tiles, {skipped} skipped, {failed} failed')
total_bytes = sum(
    os.path.getsize(os.path.join(dp, f))
    for dp, _, files in os.walk(OUTPUT_DIR)
    for f in files if f.endswith('.png')
)
print(f'Total on disk: {total_bytes // 1024} KB')
