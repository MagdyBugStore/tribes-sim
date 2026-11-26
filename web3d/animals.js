// Procedural low-poly livestock (issue #12 gaps: camel, goat, donkey; also horse, cattle, sheep, chicken).
// Each species is one merged geometry built from boxes, facing +x, feet on y=0, body length about 1 unit.
// Same visual language as buildings.js: flat boxes, colour comes from the instance tint. No external assets.
import * as THREE from 'three';
function part(w, h, d, x, y, z, rx = 0, rz = 0) { const g = new THREE.BoxGeometry(w, h, d); if (rx) g.rotateX(rx); if (rz) g.rotateZ(rz); g.translate(x, y, z); return g.toNonIndexed(); }
function merge(parts) { let n = 0; parts.forEach(p => n += p.attributes.position.count); const pos = new Float32Array(n * 3), nor = new Float32Array(n * 3); let o = 0; parts.forEach(p => { pos.set(p.attributes.position.array, o * 3); nor.set(p.attributes.normal.array, o * 3); o += p.attributes.position.count; }); const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(pos, 3)); g.setAttribute('normal', new THREE.BufferAttribute(nor, 3)); return g; }
function quad(bodyL, bodyH, bodyW, legH, legT, x0 = 0) { const p = [part(bodyL, bodyH, bodyW, x0, legH + bodyH / 2, 0)]; for (const sx of [-1, 1]) for (const sz of [-1, 1]) p.push(part(legT, legH, legT, x0 + sx * bodyL * 0.36, legH / 2, sz * bodyW * 0.32)); return p; }
export const SHAPES = {
  camel() { const p = quad(0.9, 0.34, 0.26, 0.5, 0.07); p.push(part(0.2, 0.2, 0.2, -0.05, 1.05, 0));                 // hump
    p.push(part(0.1, 0.55, 0.1, 0.5, 1.15, 0, 0, -0.35));                                                              // neck, leaning forward
    p.push(part(0.28, 0.1, 0.1, 0.72, 1.46, 0)); p.push(part(0.04, 0.04, 0.12, -0.46, 0.85, 0)); return merge(p); },
  horse() { const p = quad(0.8, 0.3, 0.22, 0.45, 0.06); p.push(part(0.1, 0.4, 0.1, 0.42, 1.0, 0, 0, -0.5)); p.push(part(0.24, 0.1, 0.1, 0.6, 1.2, 0)); p.push(part(0.04, 0.25, 0.04, -0.42, 0.75, 0, 0, 0.3)); return merge(p); },
  donkey() { const p = quad(0.7, 0.28, 0.2, 0.32, 0.06); p.push(part(0.09, 0.28, 0.09, 0.36, 0.78, 0, 0, -0.5)); p.push(part(0.2, 0.11, 0.1, 0.5, 0.92, 0)); p.push(part(0.03, 0.14, 0.03, 0.48, 1.04, 0.04)); p.push(part(0.03, 0.14, 0.03, 0.48, 1.04, -0.04)); return merge(p); },
  cattle() { const p = quad(0.8, 0.36, 0.3, 0.3, 0.08); p.push(part(0.22, 0.2, 0.2, 0.5, 0.62, 0)); p.push(part(0.03, 0.1, 0.03, 0.55, 0.78, 0.09)); p.push(part(0.03, 0.1, 0.03, 0.55, 0.78, -0.09)); return merge(p); },
  sheep() { const p = quad(0.6, 0.3, 0.28, 0.2, 0.05); p.push(part(0.16, 0.16, 0.14, 0.36, 0.5, 0)); return merge(p); },
  goat() { const p = quad(0.55, 0.22, 0.17, 0.25, 0.04); p.push(part(0.14, 0.15, 0.1, 0.33, 0.55, 0)); p.push(part(0.02, 0.1, 0.02, 0.32, 0.68, 0.03)); p.push(part(0.02, 0.1, 0.02, 0.32, 0.68, -0.03)); return merge(p); },
  chicken() { const p = [part(0.5, 0.4, 0.3, 0, 0.35, 0), part(0.12, 0.3, 0.12, 0.3, 0.6, 0), part(0.16, 0.06, 0.06, 0.42, 0.7, 0), part(0.04, 0.2, 0.04, 0, 0.1, 0.07), part(0.04, 0.2, 0.04, 0, 0.1, -0.07)]; return merge(p); },
};
export const SIZE = { camel: 1.15, horse: 1.0, donkey: 0.9, cattle: 1.5, sheep: 1.1, goat: 0.8, chicken: 0.45 }; // scaled so a camel is about 1.3x a person (people are capsules 1.3 units tall)
