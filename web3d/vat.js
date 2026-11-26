// Vertex-animation-texture helpers shared with crowd.html (copied from it): bake a skinned FBX to a float texture, cluster a far LOD, VAT material.
import * as THREE from 'three';
import { FBXLoader } from './vendor/three/addons/loaders/FBXLoader.js';
export const FR = 12, TEXW = 2048, uTime = { value: 0 }; const loader = new FBXLoader();
export async function bake(url, wanted, targetH) {
  const o = await loader.loadAsync(url);
  let sm = null; o.traverse((m) => { if (m.isSkinnedMesh) sm = m; });
  const g = sm.geometry, V = g.attributes.position.count, R = Math.ceil(V / TEXW);
  const clips = wanted.map((w) => o.animations.find((a) => a.name.endsWith('|' + w)) || o.animations.find((a) => /idle/i.test(a.name)));
  const mixer = new THREE.AnimationMixer(o), data = new Float32Array(TEXW * R * FR * clips.length * 4), v = new THREE.Vector3();
  clips.forEach((clip, ci) => {
    const act = mixer.clipAction(clip); act.play();
    for (let f = 0; f < FR; f++) {
      mixer.setTime(clip.duration * f / FR); o.updateMatrixWorld(true);
      const base = (ci * FR + f) * R * TEXW * 4;
      for (let i = 0; i < V; i++) { sm.getVertexPosition(i, v); v.applyMatrix4(sm.matrixWorld); const k = base + ((i % TEXW) + Math.floor(i / TEXW) * TEXW) * 4; data[k] = v.x; data[k + 1] = v.y; data[k + 2] = v.z; }
    }
    act.stop();
  });
  const tex = new THREE.DataTexture(data, TEXW, R * FR * clips.length, THREE.RGBAFormat, THREE.FloatType); tex.needsUpdate = true;
  const sc = targetH / (clips.length ? 1 : 1);
  // bind-pose height to scale to world units
  let y0 = 1e9, y1 = -1e9; for (let i = 0; i < V; i++) { const y = data[((i % TEXW) + Math.floor(i / TEXW) * TEXW) * 4 + 1]; if (y < y0) y0 = y; if (y > y1) y1 = y; }
  const h = y1 - y0;
  const bind = new Float32Array(V * 3); for (let i = 0; i < V; i++) { const k = ((i % TEXW) + Math.floor(i / TEXW) * TEXW) * 4; bind[i * 3] = data[k]; bind[i * 3 + 1] = data[k + 1]; bind[i * 3 + 2] = data[k + 2]; }
  const g2 = g.clone(); g2.setAttribute('position', new THREE.BufferAttribute(bind, 3)); g2.deleteAttribute('normal'); g2.computeVertexNormals();
  return { y0, hGeom: h, g: g2, V, R, tex, map: [].concat(sm.material).map((m) => m.map).find(Boolean) || null, nmat: [].concat(sm.material).length, scale: targetH / h, clips, dur: clips.map((c) => c.duration), data, sm };
}
// far LOD: cluster vertices on a grid in bind pose, average their baked positions per frame
export function decimate(b, div) {
  const g = b.g, pos = g.attributes.position, V = b.V; let ymin = 1e9, ymax = -1e9; for (let i = 0; i < V; i++) { const y = pos.getY(i); if (y < ymin) ymin = y; if (y > ymax) ymax = y; } const cell = (ymax - ymin) / div,  key = new Map(), cl = new Int32Array(V); let n = 0;
  for (let i = 0; i < V; i++) { const k = Math.round(pos.getX(i) / cell) + ',' + Math.round(pos.getY(i) / cell) + ',' + Math.round(pos.getZ(i) / cell); let c = key.get(k); if (c === undefined) { c = n++; key.set(k, c); } cl[i] = c; }
  const idx = [], seen = new Set(); for (let t = 0; t < V; t += 3) { const a = cl[t], c2 = cl[t + 1], d = cl[t + 2]; if (a === c2 || c2 === d || a === d) continue; const s = [a, c2, d].sort((x, y) => x - y).join(); if (seen.has(s)) continue; seen.add(s); idx.push(a, c2, d); }
  const R2 = Math.ceil(n / TEXW), frames = b.clips.length * FR, data = new Float32Array(TEXW * R2 * frames * 4), cnt = new Float32Array(n);
  for (let i = 0; i < V; i++) cnt[cl[i]]++;
  for (let f = 0; f < frames; f++) { const sb = f * b.R * TEXW * 4, db = f * R2 * TEXW * 4; for (let i = 0; i < V; i++) { const sk = sb + ((i % TEXW) + Math.floor(i / TEXW) * TEXW) * 4, c = cl[i], dk = db + ((c % TEXW) + Math.floor(c / TEXW) * TEXW) * 4, w = 1 / cnt[c]; data[dk] += b.data[sk] * w; data[dk + 1] += b.data[sk + 1] * w; data[dk + 2] += b.data[sk + 2] * w; } }
  const bp = new Float32Array(n * 3); for (let i = 0; i < V; i++) { const c = cl[i]; bp[c * 3] += pos.getX(i) / cnt[c]; bp[c * 3 + 1] += pos.getY(i) / cnt[c]; bp[c * 3 + 2] += pos.getZ(i) / cnt[c]; }
  const fg = new THREE.BufferGeometry(); fg.setAttribute('position', new THREE.BufferAttribute(bp, 3)); fg.setAttribute('vid', new THREE.BufferAttribute(Float32Array.from({ length: n }, (_, i) => i), 1)); fg.setIndex(idx);
  const tex = new THREE.DataTexture(data, TEXW, R2 * frames, THREE.RGBAFormat, THREE.FloatType); tex.needsUpdate = true; fg.computeVertexNormals();
  return { g: fg, R: R2, tex, V: n, tris: idx.length / 3 };
}
export function vatMaterial(tex, R, map, far, garb) {
  if (map) { if (map.channel === undefined) map.channel = 0; map.colorSpace = THREE.SRGBColorSpace; }
  const m = new THREE.MeshLambertMaterial({ map: far ? null : map, color: 0xffffff });
  m.customProgramCacheKey = () => (far ? 'vatF' : 'vatN') + (map ? 'm' : '') + R + (garb ? 'g' + garb.kind : '');
  m.onBeforeCompile = (s) => {
    if (garb) { s.uniforms.uHY = { value: new THREE.Vector2(garb.y0, garb.h) }; s.uniforms.uFace = { value: garb.face }; }
    s.uniforms.uVat = { value: tex }; s.uniforms.uTime = uTime; s.uniforms.uR = { value: R };
    s.vertexShader = (garb ? 'varying vec3 vBind;\n' : '') + 'attribute float vid; attribute vec4 iAnim; attribute vec4 iPrev; uniform sampler2D uVat; uniform float uTime; uniform float uR;\n' + s.vertexShader.replace('#include <begin_vertex>', `
      ${garb ? 'vBind = position;' : ''}
      float ph = fract(uTime * iAnim.z + iAnim.w) * float(${FR}); float f0 = floor(ph); float f1 = mod(f0 + 1.0, float(${FR})); float fr = ph - f0;
      float vx = mod(vid, ${TEXW}.0), vy = floor(vid / ${TEXW}.0);
      vec3 p0 = texelFetch(uVat, ivec2(int(vx), int((iAnim.x + f0) * uR + vy)), 0).xyz; vec3 p1 = texelFetch(uVat, ivec2(int(vx), int((iAnim.x + f1) * uR + vy)), 0).xyz;
      vec3 transformed = mix(p0, p1, fr);
      if (iPrev.w > 0.0) { float ph2 = fract(uTime * iPrev.y + iPrev.z) * float(${FR}); float g0 = floor(ph2); float g1 = mod(g0 + 1.0, float(${FR})); float gr = ph2 - g0;
        vec3 q0 = texelFetch(uVat, ivec2(int(vx), int((iPrev.x + g0) * uR + vy)), 0).xyz; vec3 q1 = texelFetch(uVat, ivec2(int(vx), int((iPrev.x + g1) * uR + vy)), 0).xyz;
        transformed = mix(transformed, mix(q0, q1, gr), iPrev.w); }`);
    if (garb) s.fragmentShader = 'varying vec3 vBind; uniform vec2 uHY; uniform float uFace;\n' + s.fragmentShader.replace('#include <color_fragment>', `#include <color_fragment>
      { float hh = (vBind.y - uHY.x) / uHY.y; float fz = vBind.z * uFace / uHY.y; vec3 tint = mix(vec3(1.0), vColor.rgb, 0.5);
        vec3 baseC = ${map && !far ? 'diffuseColor.rgb' : 'vec3(0.8, 0.62, 0.5) * tint'}; float lum = dot(baseC, vec3(0.3, 0.59, 0.11));
        bool head = hh > 0.84, front = fz > 0.0; vec3 cloth = vec3(0.0); float use = 0.0;
        ${garb.kind === 'f' ? `
        if (hh > 0.05 && hh <= 0.84) { cloth = vec3(0.10, 0.09, 0.11) * (0.6 + 0.8 * lum); use = 1.0; }
        if (head) { bool faceZone = front && hh < 0.955 && abs(vBind.x) / uHY.y < 0.075; if (!faceZone) { cloth = vec3(0.09, 0.08, 0.10) * (0.6 + 0.8 * lum); use = 1.0; } }` : `
        if (hh > 0.05 && hh <= 0.84) { cloth = vec3(0.93, 0.90, 0.82) * tint * (0.65 + 0.55 * lum); use = 1.0; }
        if (head) { bool faceZone = front && hh < 0.955 && abs(vBind.x) / uHY.y < 0.075; if (!faceZone) { cloth = (hh > 0.945 && hh < 0.965 ? vec3(0.08) : vec3(0.97, 0.96, 0.93) * tint) * (0.75 + 0.35 * lum); use = 1.0; } }`}
        diffuseColor.rgb = mix(baseC, cloth, use); }`);
  };
  return m;
}
