// Procedural low-poly buildings and props (issue #15). All geometry is generated in code
// (source: "procedural", licence: project code, textures from tools/gen_textures.py).
import * as THREE from 'three';
const tl = new THREE.TextureLoader();
const texCache = {};
export function tex(name, rep = 1) {
  const k = name + rep;
  if (!texCache[k]) { const t = tl.load('./textures/' + name + '.png'); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rep, rep); t.colorSpace = THREE.SRGBColorSpace; texCache[k] = t; }
  return texCache[k];
}
const mats = {};
export function mat(name, tx, rep = 1, color = 0xffffff) {
  const k = name + tx + rep + color;
  return mats[k] || (mats[k] = new THREE.MeshLambertMaterial({ map: tx ? tex(tx, rep) : null, color }));
}
const box = (w, h, d, m, x = 0, y = 0, z = 0) => { const o = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m); o.position.set(x, y + h / 2, z); return o; };
const cyl = (r1, r2, h, m, x = 0, y = 0, z = 0, seg = 12) => { const o = new THREE.Mesh(new THREE.CylinderGeometry(r2, r1, h, seg), m); o.position.set(x, y + h / 2, z); return o; };
const G = (...c) => { const g = new THREE.Group(); c.forEach(x => g.add(x)); return g; };
const rnd = (s) => { let a = s * 9301 + 49297; return () => ((a = (a * 9301 + 49297) % 233280) / 233280); };

export const SOURCES = {}; // filled below: name -> {src, lic}
const PROC = { src: 'procedural (three.js geometry + generated textures)', lic: 'project code / CC0-style generated textures' };

export function tent(v = 0) { // goat-hair tent, sizes 0..2
  const w = [3, 4.5, 6][v % 3], d = w * 0.8, h = w * 0.5;
  const hair = mat('hair', 'goat_hair', 1);
  const roof = new THREE.Mesh(new THREE.ConeGeometry(d * 0.7, h, 8), hair);
  roof.scale.set(w / d, 1, 1); roof.position.y = h / 2;
  return G(roof, box(w * 0.4, 0.1, d * 0.4, mat('rug', 'rug', 1), 0, 0, d * 0.45));
}
export function house(v = 0) { // mud-brick flat-roof house
  const m = mat('mb', 'mudbrick', 2), w = [4, 5, 6][v % 3], d = [4, 4.5, 5][v % 3], h = 2.6 + (v % 2) * 0.6;
  return G(box(w, h, d, m), box(w + 0.3, 0.2, d + 0.3, mat('p', 'packed_earth', 1), 0, h, 0), box(0.9, 1.7, 0.1, mat('wood', 'wood_planks'), 0, 0, d / 2 + 0.02));
}
export function mosque() {
  const pl = mat('pl', 'plaster', 2), g = new THREE.Group();
  g.add(box(9, 4, 9, pl));
  const dome = new THREE.Mesh(new THREE.SphereGeometry(3, 16, 10, 0, Math.PI * 2, 0, Math.PI / 2), mat('dome', 'plaster', 1, 0xe8dcc0)); dome.position.y = 4; g.add(dome);
  g.add(cyl(0.9, 0.7, 9, pl, 5.5, 0, 5.5, 10)); g.add(box(1.6, 0.3, 1.6, pl, 5.5, 9, 5.5));
  const tip = new THREE.Mesh(new THREE.ConeGeometry(0.7, 1.6, 10), pl); tip.position.set(5.5, 10.1, 5.5); g.add(tip); return g;
}
export function majlis() {
  const m = mat('mb', 'mudbrick', 3); const g = G(box(12, 3.2, 6, m), box(12.4, 0.25, 6.4, mat('p', 'packed_earth'), 0, 3.2, 0));
  for (let i = -2; i <= 2; i++) g.add(cyl(0.2, 0.2, 3, mat('wood', 'wood_planks'), i * 2.4, 0, 3.1, 6)); return g;
}
export function stall(v = 0, stock = 1) { // stock 0..1 => goods count
  const g = G(box(3, 0.9, 1.4, mat('wood', 'wood_planks', 1), 0, 0, 0));
  const aw = new THREE.Mesh(new THREE.PlaneGeometry(3.4, 1.8), new THREE.MeshLambertMaterial({ color: [0xc0392b, 0x2e86c1, 0xd4a017][v % 3], side: THREE.DoubleSide }));
  aw.rotation.x = -Math.PI / 2.6; aw.position.set(0, 2.5, -0.1); g.add(aw);
  for (const x of [-1.5, 1.5]) g.add(cyl(0.05, 0.05, 2.5, mat('wood', 'wood_planks'), x, 0, 0.6, 5));
  const n = Math.round(stock * 12), r = rnd(v + 3);
  for (let i = 0; i < n; i++) g.add(box(0.22, 0.22, 0.22, mat('g' + (i % 3), null, 1, [0xd2a24c, 0x7a9a3a, 0xa0522d][i % 3]), -1.2 + (i % 6) * 0.48, 0.9, -0.3 + Math.floor(i / 6) * 0.5 + r() * 0.1));
  g.userData.stock = stock; return g;
}
export function well(level = 1) {
  const g = G(cyl(1.0, 1.0, 0.9, mat('rock', 'rock', 2), 0, 0, 0, 12));
  const w = cyl(0.8, 0.8, 0.02, new THREE.MeshLambertMaterial({ color: 0x3b7fc4 }), 0, 0.15 + level * 0.6, 0, 12); g.add(w);
  g.add(box(0.1, 1.8, 0.1, mat('wood', 'wood_planks'), -1, 0, 0), box(0.1, 1.8, 0.1, mat('wood', 'wood_planks'), 1, 0, 0), box(2.3, 0.1, 0.1, mat('wood', 'wood_planks'), 0, 1.8, 0));
  g.add(cyl(0.01, 0.01, 0.9, new THREE.MeshLambertMaterial({ color: 0x8b6b3d }), 0, 0.9, 0, 4)); g.add(cyl(0.15, 0.12, 0.25, mat('wood', 'wood_planks'), 0, 0.65, 0, 8)); return g;
}
export function channel(len = 10) {
  const g = G(box(len, 0.3, 1.2, mat('rock', 'rock', 2)));
  const water = new THREE.Mesh(new THREE.PlaneGeometry(len, 0.8), new THREE.MeshLambertMaterial({ color: 0x3b7fc4, map: tex('sand_dark', 4) }));
  water.rotation.x = -Math.PI / 2; water.position.y = 0.31; water.name = 'water'; g.add(water); return g;
}
export function mill() { const g = G(cyl(2, 1.5, 4, mat('mb', 'mudbrick', 2), 0, 0, 0, 10)); const bl = new THREE.Group(); bl.name = 'blades'; for (let i = 0; i < 4; i++) { const b = box(0.4, 3.2, 0.05, mat('wood', 'wood_planks')); b.position.y = 1.6; const p = new THREE.Group(); p.add(b); p.rotation.z = i * Math.PI / 2; bl.add(p); } bl.position.set(0, 3.6, 1.7); g.add(bl); return g; }
export function oven() { const g = G(cyl(1, 0.8, 1.4, mat('mb', 'mudbrick', 1), 0, 0, 0, 10)); g.add(box(0.6, 0.5, 0.2, new THREE.MeshLambertMaterial({ color: 0x111111 }), 0, 0.3, 0.85)); const s = new THREE.Group(); s.name = 'smoke'; g.add(s); g.userData.smokeAt = new THREE.Vector3(0, 1.5, 0); return g; }
export function smithy() { const g = G(box(4, 2.4, 3, mat('rock', 'rock', 2)), box(4.4, 0.2, 3.4, mat('wood', 'wood_planks'), 0, 2.4, 0), box(0.8, 0.7, 0.4, new THREE.MeshLambertMaterial({ color: 0x333333 }), 1.0, 0, 2.2)); return g; }
export function saltPan(fill = 1) { const g = G(); for (let i = 0; i < 4; i++) { const p = box(2.2, 0.15, 2.2, mat('s', 'sand_light', 1, i / 4 < fill ? 0xffffff : 0xb09a70), (i % 2) * 2.4, 0, Math.floor(i / 2) * 2.4); g.add(p); } return g; }
export function field(growth = 0.5) { // growth 0..1 rows; <0 = failed/dry
  const g = G(box(8, 0.1, 6, mat('soil', 'field_soil', 3)));
  const dry = growth < 0, h = dry ? 0.12 : 0.1 + growth * 0.9;
  for (let r = 0; r < 6; r++) g.add(box(7.4, h, 0.35, new THREE.MeshLambertMaterial({ color: dry ? 0x9a8650 : new THREE.Color().setHSL(0.25 - growth * 0.1, 0.5, 0.3 + growth * 0.1) }), 0, 0.1, -2.5 + r * 1.0));
  return g;
}
export function palm(v = 0) { const g = G(cyl(0.3, 0.2, 5 + v, mat('wood', 'wood_planks'), 0, 0, 0, 6)); const lm = new THREE.MeshLambertMaterial({ color: 0x3f7a2a, side: THREE.DoubleSide }); for (let i = 0; i < 7; i++) { const l = new THREE.Mesh(new THREE.PlaneGeometry(0.7, 3), lm); l.position.set(Math.cos(i) * 1.1, 5 + v, Math.sin(i) * 1.1); l.rotation.set(-Math.PI / 2.6, 0, 0); l.rotation.order = 'YXZ'; l.rotation.y = -i; g.add(l); } return g; }
export function acacia(v = 0) { const g = G(cyl(0.25, 0.15, 3 + v * 0.5, mat('wood', 'wood_planks'), 0, 0, 0, 6)); const c = new THREE.Mesh(new THREE.SphereGeometry(2.2, 8, 5), new THREE.MeshLambertMaterial({ color: 0x6b8e23 })); c.scale.y = 0.35; c.position.y = 3.4 + v * 0.5; g.add(c); return g; }
export function granary(level = 0.5) { const g = G(cyl(1.6, 1.4, 3, mat('mb', 'mudbrick', 2), 0, 0, 0, 12)); const cone = new THREE.Mesh(new THREE.ConeGeometry(1.9, 1.2, 12), mat('hair', 'goat_hair', 2)); cone.position.y = 3.6; g.add(cone); g.add(box(0.15, 2.5 * level + 0.05, 0.15, new THREE.MeshLambertMaterial({ color: 0xd2a24c }), 1.7, 0, 0)); return g; }
export function roadStones() { const g = G(box(30, 0.05, 3, mat('gr', 'gravel', 6))); const r = rnd(5); for (let i = 0; i < 25; i++) g.add(box(0.2 + r() * 0.3, 0.2, 0.2 + r() * 0.3, mat('rock', 'rock'), -14 + r() * 28, 0, (r() - 0.5) * 3.6)); return g; }
export function flag(c = 0xc0392b) { const g = G(cyl(0.04, 0.04, 4, mat('wood', 'wood_planks'), 0, 0, 0, 5)); const f = new THREE.Mesh(new THREE.PlaneGeometry(1, 0.6), new THREE.MeshLambertMaterial({ color: c, side: THREE.DoubleSide })); f.position.set(0.5, 3.6, 0); f.name = 'flag'; g.add(f); return g; }
export function lantern() { const g = G(cyl(0.04, 0.04, 2, mat('wood', 'wood_planks'), 0, 0, 0, 5)); const l = new THREE.Mesh(new THREE.SphereGeometry(0.15, 8, 6), new THREE.MeshBasicMaterial({ color: 0xffc860 })); l.position.y = 2.1; l.name = 'lamp'; g.add(l); return g; }

export const CATALOG = [
  ['Goat-hair tent S/M/L', i => tent(i), 3, 'wide'], ['Mud-brick house (3 variants)', i => house(i), 3],
  ['Mosque (dome + minaret)', () => mosque(), 1], ['Majlis hall', () => majlis(), 1],
  ['Suq stall: empty / half / full', i => stall(i, [0.1, 0.5, 1][i]), 3], ['Well: low / mid / full', i => well([0.1, 0.5, 1][i]), 3],
  ['Water channel', () => channel(), 1], ['Mill', () => mill(), 1], ['Oven (smoke)', () => oven(), 1], ['Smithy', () => smithy(), 1],
  ['Salt pan: 25/50/100%', i => saltPan([0.25, 0.5, 1][i]), 3], ['Field: failed / young / grown', i => field([-1, 0.3, 1][i]), 3],
  ['Palm (3 variants)', i => palm(i), 3], ['Acacia (3 variants)', i => acacia(i), 3], ['Granary: low / full', i => granary([0.15, 1][i]), 2],
  ['Caravan road + stones', () => roadStones(), 1], ['Flag', () => flag(), 1], ['Lantern (night)', () => lantern(), 1],
];
export function animate(root, t, night) {
  root.traverse(o => {
    if (o.name === 'blades') o.rotation.z = t * 0.8;
    if (o.name === 'flag') o.rotation.y = Math.sin(t * 3) * 0.4;
    if (o.name === 'water') { o.material.map.offset.x = -t * 0.1; }
    if (o.name === 'lamp') o.material.color.setHex(night ? 0xffc860 : 0x665533);
    if (o.name === 'smoke') { if (!o.children.length) for (let i = 0; i < 6; i++) { const s = new THREE.Mesh(new THREE.SphereGeometry(0.2, 6, 4), new THREE.MeshBasicMaterial({ color: 0x999999, transparent: true })); o.add(s); } o.children.forEach((s, i) => { const p = ((t * 0.4 + i / 6) % 1); s.position.set(Math.sin(i + t) * 0.15, 1.5 + p * 2.5, 0); s.scale.setScalar(0.6 + p * 1.5); s.material.opacity = 0.6 * (1 - p); }); }
  });
}
