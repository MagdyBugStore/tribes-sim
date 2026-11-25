#!/usr/bin/env python3
"""Textures must tile and be reproducible. Run: python3 tests/test_textures.py"""
import os, struct, sys, tempfile, zlib, hashlib
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import gen_textures as G

SIZE = 128

def px(name, seed):
    return G.gen(name, SIZE, seed)

def col(d, x): return [d[(y * SIZE + x) * 3 + c] for y in range(SIZE) for c in range(3)]
def row(d, y): return list(d[y * SIZE * 3:(y + 1) * SIZE * 3])
def mad(a, b): return sum(abs(p - q) for p, q in zip(a, b)) / len(a)

fail = 0
for name in G.RECIPES:
    d = px(name, 7)
    # seam: last column vs first column should be as close as any two neighbouring columns
    inner = max(mad(col(d, x), col(d, x + 1)) for x in range(0, SIZE - 1, 9))
    seam_x = mad(col(d, SIZE - 1), col(d, 0))
    inner_y = max(mad(row(d, y), row(d, y + 1)) for y in range(0, SIZE - 1, 9))
    seam_y = mad(row(d, SIZE - 1), row(d, 0))
    ok = seam_x <= inner * 1.5 + 2 and seam_y <= inner_y * 1.5 + 2
    same = hashlib.sha256(px(name, 7)).digest() == hashlib.sha256(d).digest()
    other = hashlib.sha256(px(name, 8)).digest() != hashlib.sha256(d).digest()
    print(('ok  ' if ok and same and other else 'FAIL'), name, f'seam {seam_x:.1f}/{seam_y:.1f} vs inner {inner:.1f}/{inner_y:.1f}')
    fail += not (ok and same and other)
print('ALL TEXTURE TESTS PASSED' if not fail else f'{fail} FAILED')
sys.exit(1 if fail else 0)
