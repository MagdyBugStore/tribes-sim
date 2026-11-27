// Terrain for world.html: heightfield from /api/map polygons (hills, dunes, wadi) with a seeded fallback.
// Same shape rules as terrain.html, but flat (h = 0) where no polygon applies, so buildings, roads and
// people (all at y = 0) never sink or float. Heights are in metres * S. Heights and the 12-unit
// dune/hill amplitudes are visual guesses: غير موثق.
import * as THREE from 'three';
export function buildTerrain(map, S, texture, CELL = 10) {
  const W = map.width, H = map.height, NX = Math.round(W / CELL), NY = Math.round(H / CELL);
  const seed = map.seed || 42;
  const rnd = s => () => { s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; };
  const R = rnd(seed), lat = new Float32Array(66 * 66).map(() => R());
  const noise = (x, y) => { const xi = Math.floor(x), yi = Math.floor(y), fx = x - xi, fy = y - yi, u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy), g = (a, b) => lat[((b & 63) * 66) + (a & 63)];
    return (g(xi, yi) * (1 - u) + g(xi + 1, yi) * u) * (1 - v) + (g(xi, yi + 1) * (1 - u) + g(xi + 1, yi + 1) * u) * v; };
  const fbm = (x, y) => noise(x, y) * 0.55 + noise(x * 2.1, y * 2.1) * 0.3 + noise(x * 4.3, y * 4.3) * 0.15;
  const inside = (pts, x, y) => { let c = false; for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) { const a = pts[i], b = pts[j]; if ((a[1] > y) !== (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]) c = !c; } return c; };
  const MX = Math.round(W / 10), MY = Math.round(H / 10), mask = { hills: new Float32Array(MX * MY), dunes: new Float32Array(MX * MY), wadi: new Float32Array(MX * MY) };
  if (map.terrain) { for (const t of map.terrain) if (mask[t.type]) for (let j = 0; j < MY; j++) for (let i = 0; i < MX; i++) if (inside(t.points, i * 10 + 5, j * 10 + 5)) mask[t.type][j * MX + i] = 1; }
  else for (let j = 0; j < MY; j++) for (let i = 0; i < MX; i++) { const x = i / MX, y = j / MY; if (x < 0.12 || x > 0.88 || y < 0.1) mask.hills[j * MX + i] = 1; if (y > 0.8) mask.dunes[j * MX + i] = 1; }
  for (const k in mask) for (let pass = 0; pass < 4; pass++) { const m = mask[k], o = new Float32Array(m.length);
    for (let j = 0; j < MY; j++) for (let i = 0; i < MX; i++) { let s = 0, n = 0; for (let dj = -2; dj <= 2; dj++) for (let di = -2; di <= 2; di++) { const a = i + di, b = j + dj; if (a >= 0 && b >= 0 && a < MX && b < MY) { s += m[b * MX + a]; n++; } } o[j * MX + i] = s / n; } mask[k] = o; }
  const samp = (m, x, y) => { const fx = Math.min(MX - 1.001, Math.max(0, x / 10 - 0.5)), fy = Math.min(MY - 1.001, Math.max(0, y / 10 - 0.5)), i = fx | 0, j = fy | 0, u = fx - i, v = fy - j;
    return (m[j * MX + i] * (1 - u) + m[j * MX + i + 1] * u) * (1 - v) + (m[(j + 1) * MX + i] * (1 - u) + m[(j + 1) * MX + i + 1] * u) * v; };
  const height = (x, y) => { const hills = samp(mask.hills, x, y), dunes = samp(mask.dunes, x, y);
    let h = hills * (40 + fbm(x / 90, y / 90) * 60) + dunes * (14 + Math.sin(x / 70 + fbm(x / 200, y / 200) * 6) * 9 + Math.sin((x + y) / 45) * 4);
    return Math.max(0, h) * S; };
  const pos = [], col = [], uv = [], idx = [], hs = new Float32Array((NX + 1) * (NY + 1));
  for (let j = 0; j <= NY; j++) for (let i = 0; i <= NX; i++) hs[j * (NX + 1) + i] = height(i * CELL, j * CELL);
  for (let j = 0; j <= NY; j++) for (let i = 0; i <= NX; i++) {
    const h = hs[j * (NX + 1) + i], x = i * CELL, y = j * CELL, hh = hs[Math.min(NY, j + 1) * (NX + 1) + i], hr = hs[j * (NX + 1) + Math.min(NX, i + 1)];
    const slope = Math.min(1, Math.hypot(hh - h, hr - h) / (CELL * S) / 0.9), dune = samp(mask.dunes, x, y), rock = Math.min(1, Math.max(slope * 1.2, h / (70 * S)));
    const c = new THREE.Color(1, 0.96, 0.85).lerp(new THREE.Color(0.55, 0.5, 0.46), rock).lerp(new THREE.Color(1.1, 0.9, 0.62), dune * 0.6);
    pos.push(x * S, h, y * S); uv.push(x / W * 40, y / H * 40); col.push(c.r, c.g, c.b);
  }
  for (let j = 0; j < NY; j++) for (let i = 0; i < NX; i++) { const a = j * (NX + 1) + i; idx.push(a, a + NX + 1, a + 1, a + 1, a + NX + 1, a + NX + 2); }
  const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3)); g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  g.setIndex(idx); g.computeVertexNormals();
  const mesh = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ map: texture, vertexColors: true }));
  let maxH = 0; for (const v of hs) if (v > maxH) maxH = v;
  return { mesh, maxH };
}
