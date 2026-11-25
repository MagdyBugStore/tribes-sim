#!/usr/bin/env python3
"""Procedural, seamless, reproducible ground and material textures (standard library only).

Usage: python3 tools/gen_textures.py [--seed N] [--size 512] [--out web3d/textures]
Same seed gives byte-identical files. Every texture tiles: noise is periodic in x and y.
"""
import argparse, math, os, random, struct, zlib


def write_png(path, w, h, rgb):
    raw = b''.join(b'\x00' + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


class Noise:
    """Periodic value noise: lattice of `period` cells wraps, so the image tiles."""
    def __init__(self, rng, period):
        self.p = period
        self.g = [[rng.random() for _ in range(period)] for _ in range(period)]

    def at(self, u, v, freq):           # u, v in [0,1)
        n = self.p                      # the lattice has exactly `freq` cells, so it wraps
        assert n == freq
        x, y = u * freq, v * freq
        x0, y0 = int(math.floor(x)), int(math.floor(y))
        fx, fy = x - x0, y - y0
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        g = self.g
        a = g[y0 % n][x0 % n]; b = g[y0 % n][(x0 + 1) % n]
        c = g[(y0 + 1) % n][x0 % n]; d = g[(y0 + 1) % n][(x0 + 1) % n]
        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy


def fbm(noises, u, v, base, octaves):
    s, amp, tot, f = 0.0, 1.0, 0.0, base
    for o in range(octaves):
        s += noises[o].at(u, v, f) * amp; tot += amp; amp *= 0.5; f *= 2
    return s / tot


def lerp(c0, c1, t):
    return tuple(c0[i] + (c1[i] - c0[i]) * t for i in range(3))


# name: (colour a, colour b, base frequency, octaves, contrast, stripes)
RECIPES = {
    'sand_light':   ((226, 196, 132), (240, 216, 160), 4, 4, 1.0, None),
    'sand_dark':    ((196, 160, 100), (214, 178, 118), 4, 4, 1.0, None),
    'packed_earth': ((150, 118, 84), (176, 140, 100), 8, 5, 1.2, None),
    'gravel':       ((120, 110, 98), (186, 174, 158), 32, 3, 1.6, None),
    'rock':         ((92, 84, 76), (158, 146, 130), 6, 6, 1.5, None),
    'dry_grass':    ((150, 140, 78), (196, 178, 104), 16, 4, 1.3, None),
    'field_soil':   ((72, 54, 38), (110, 82, 56), 10, 4, 1.2, None),
    'cracked_mud':  ((150, 120, 90), (196, 164, 126), 6, 3, 2.2, None),
    'mudbrick':     ((160, 124, 88), (190, 152, 112), 12, 3, 1.2, 'brick'),
    'plaster':      ((226, 214, 190), (240, 230, 208), 6, 4, 0.6, None),
    'wood_planks':  ((120, 82, 50), (160, 112, 70), 3, 4, 1.0, 'planks'),
    'goat_hair':    ((30, 26, 24), (232, 222, 200), 20, 2, 0.5, 'tent'),
    'rug':          ((140, 30, 30), (230, 190, 90), 16, 2, 0.4, 'rug'),
}


def gen(name, size, seed):
    ca, cb, base, octs, contrast, mode = RECIPES[name]
    rng = random.Random(f'{seed}:{name}')
    noises = [Noise(rng, base * 2 ** o) for o in range(octs)]
    out = bytearray()
    for y in range(size):
        v = y / size
        for x in range(size):
            u = x / size
            n = fbm(noises, u, v, base, octs)
            t = max(0.0, min(1.0, 0.5 + (n - 0.5) * contrast))
            col = lerp(ca, cb, t)
            if mode == 'brick':
                row = int(v * 8); off = 0.5 if row % 2 else 0.0
                bx = ((u * 4 + off) % 1.0); by = (v * 8) % 1.0
                if abs(bx - 0.5) < 0.02 or abs(by - 0.5) < 0.03: col = lerp(col, (110, 92, 72), 0.7)
            elif mode == 'planks':
                if abs((v * 6) % 1.0 - 0.5) < 0.02: col = lerp(col, (50, 32, 20), 0.8)
            elif mode == 'tent':
                col = (cb if int(u * 12) % 2 else ca) if t > 0.0 else col
                col = lerp(col, lerp(ca, cb, t), 0.25)
            elif mode == 'rug':
                k = int(u * 8 + v * 0) % 2
                col = lerp(ca, cb, 0.15 + 0.7 * ((int(v * 16) + k) % 2)) if abs((u * 8) % 1 - 0.5) < 0.35 else (40, 30, 70)
                col = lerp(col, lerp(ca, cb, t), 0.2)
            out += bytes(int(max(0, min(255, c))) for c in col)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--size', type=int, default=256)
    ap.add_argument('--out', default='web3d/textures')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for name in RECIPES:
        write_png(os.path.join(a.out, name + '.png'), a.size, a.size, gen(name, a.size, a.seed))
        print('wrote', name)


if __name__ == '__main__':
    main()
