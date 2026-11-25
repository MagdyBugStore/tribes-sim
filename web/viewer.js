// Tribes of the Valley: the live viewer. No frameworks, no build step.
// Data: /api/map once, /api/state every 500 ms, /api/schedules once per day
// (everyone's 24 hourly slots and where each happens), /api/people every 2 s.
// The day plays over seconds_per_day; people walk in straight lines between
// the stops of their schedule and stand still while working.
'use strict';

const $ = (id) => document.getElementById(id);
const canvas = $('view');
const ctx = canvas.getContext('2d');
const params = new URLSearchParams(location.search);
const FIXED_HOUR = params.has('hour') ? parseFloat(params.get('hour')) : null;  // debug: freeze the clock
const ALPHA = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const ALPHA_IDX = new Int8Array(128).fill(-1);
for (let i = 0; i < ALPHA.length; i++) ALPHA_IDX[ALPHA.charCodeAt(i)] = i;
const WALK = 0.4;               // an hour's walk takes the first 40% of the hour
const BASE_SPD = 24;            // 1x = 24 seconds per day
const SPEEDS = { '1': 24, '2': 12, '5': 4.8, '10': 2.4 };

// --- role groups -----------------------------------------------------------
const GROUPS = [
  { id: 'children', name: 'Children', color: '#ffe08a' },
  { id: 'farm', name: 'Farming and water', color: '#59b35a' },
  { id: 'herd', name: 'Herding and dairy', color: '#f0902e' },
  { id: 'craft', name: 'Crafts and labor', color: '#b37be0' },
  { id: 'trade', name: 'Trade and caravans', color: '#ef5b5b' },
  { id: 'civic', name: 'Council and arts', color: '#4a90e2' },
  { id: 'faith', name: 'Mosque and school', color: '#4fd1c5' },
  { id: 'guard', name: 'Guards and hunters', color: '#8a93a6' },
  { id: 'health', name: 'Healers', color: '#ff8fb8' },
  { id: 'elders', name: 'Elders', color: '#d9d9d9' },
];
const SECTOR_GROUP = {
  fields: 'farm', groves: 'farm', water: 'farm', labor: 'craft', herds: 'herd', dairy: 'herd',
  crafts: 'craft', salt: 'craft', firewood: 'craft', households: 'craft', services: 'trade',
  market: 'trade', caravans: 'trade', majlis: 'civic', bayt: 'civic', works: 'civic', arts: 'civic',
  mosque: 'faith', school: 'faith', watch: 'guard', guard: 'guard', hunting: 'guard',
  health: 'health', home: 'elders',
};
const GROUP_INDEX = {};
GROUPS.forEach((g, i) => { GROUP_INDEX[g.id] = i; });

// --- state -----------------------------------------------------------------
let map = null, mapPaths = null;
let state = null;
let sched = null;               // parsed schedules of schedDay
let schedDay = -1, loadingDay = -1;
let roleGroup = [];             // role index -> group index
const clock = { base: 0, at: 0, spd: BASE_SPD, paused: false, frozen: 0, valid: false };
const view = { scale: 1, x: 0, y: 0, w: 0, h: 0, dpr: 1, fit: 1 };
const filter = { tribe: -1, group: -1, act: '' };
let showLabels = true, showAnimals = true;
let lastDraw = 0, fpsFrames = 0, fpsAt = 0, fps = 0;

function setStatus(s) { $('status').textContent = s; }

async function getJSON(url) {
  const r = await fetch(url, { cache: 'no-store' });
  if (!r.ok) throw new Error(url + ' ' + r.status);
  return r.json();
}

// --- map -------------------------------------------------------------------
const KIND_STYLE = {
  home: ['#6b4a2a', 'rect'], well: ['#2f7fe0', 'circ'], field: ['#8fc46a', 'rect'],
  grove: ['#2e7d32', 'circ'], pasture: ['rgba(170,200,100,0.45)', 'circ'], suq: ['#e0a030', 'circ'],
  shop: ['#c0392b', 'circ'], mosque: ['#fafafa', 'circ'], majlis: ['#8e44ad', 'circ'],
  school: ['#3aa6a0', 'circ'], clinic: ['#ff8fb8', 'circ'], bayt_al_mal: ['#d4af37', 'circ'],
  mill: ['#7d6b57', 'circ'], oven: ['#c75b2a', 'circ'], smithy: ['#555', 'circ'],
  loom: ['#9b6bb5', 'circ'], tannery: ['#8a5a3a', 'circ'], dairy: ['#f2ecd0', 'circ'],
  salt_pan: ['#f7f7f2', 'circ'], gate: ['#222', 'circ'], caravanserai: ['#7a5a2c', 'circ'],
};

function poly(pts) {
  const p = new Path2D();
  pts.forEach((q, i) => (i ? p.lineTo(q[0], q[1]) : p.moveTo(q[0], q[1])));
  p.closePath();
  return p;
}

function buildMap(m) {
  const terrainFill = { hills: '#a39a8a', dunes: '#f0c978', wadi: '#cdb98a' };
  const terrain = m.terrain.map((t) => ({ path: poly(t.points), fill: terrainFill[t.type] || '#ccc' }));
  const road = new Path2D();
  m.road.forEach((q, i) => (i ? road.lineTo(q[0], q[1]) : road.moveTo(q[0], q[1])));
  const byKind = {};
  for (const o of m.objects) {
    const k = o[0];
    if (!byKind[k]) byKind[k] = new Path2D();
    const [x, y, r] = [o[3], o[4], o[5]];
    const style = KIND_STYLE[k] || ['#444', 'circ'];
    if (style[1] === 'rect') byKind[k].rect(x - r, y - r, r * 2, r * 2);
    else { byKind[k].moveTo(x + r, y); byKind[k].arc(x, y, r, 0, Math.PI * 2); }
  }
  const settle = new Path2D();
  m.settlements.forEach((s) => { settle.moveTo(s.x + s.radius, s.y); settle.arc(s.x, s.y, s.radius, 0, Math.PI * 2); });
  const named = m.objects.filter((o) => ['mosque', 'majlis', 'suq', 'caravanserai', 'school', 'clinic', 'mill', 'oven', 'smithy'].includes(o[0]));
  return { terrain, road, byKind, settle, named };
}

async function loadMap() {
  map = await getJSON('/api/map');
  mapPaths = buildMap(map);
  const sel = $('f-tribe');
  map.settlements.forEach((s) => {
    const o = document.createElement('option');
    o.value = s.tribe; o.textContent = s.name; sel.appendChild(o);
  });
  fitView();
}

// --- schedules ---------------------------------------------------------------
function parseSched(d) {
  const rows = d.people, n = rows.length;
  const s = {
    day: d.day, n, acts: d.acts, ids: new Int32Array(n), tribe: new Uint8Array(n), gender: new Uint8Array(n),
    age: new Uint8Array(n), group: new Uint8Array(n), off: new Int32Array(n),
    seq: new Uint8Array(n * 24), act: new Uint8Array(n * 24), stops: null,
    startX: new Float32Array(n), startY: new Float32Array(n), cx: new Float32Array(n), cy: new Float32Array(n),
    hash: new Float32Array(n),
  };
  let total = 0;
  for (const r of rows) total += r[5].length;
  s.stops = new Float32Array(total);
  let off = 0;
  for (let i = 0; i < n; i++) {
    const r = rows[i];
    s.ids[i] = r[0]; s.tribe[i] = r[1]; s.gender[i] = r[2]; s.age[i] = Math.min(255, r[3]);
    const rg = roleGroup[r[4]];
    s.group[i] = r[3] < 16 ? GROUP_INDEX.children : (r[3] >= 60 && rg === GROUP_INDEX.elders ? GROUP_INDEX.elders : rg);
    s.off[i] = off;
    const st = r[5];
    for (let k = 0; k < st.length; k++) s.stops[off + k] = st[k];
    off += st.length;
    const sq = r[6], ac = r[7];
    for (let h = 0; h < 24; h++) {
      s.seq[i * 24 + h] = ALPHA_IDX[sq.charCodeAt(h)];
      s.act[i * 24 + h] = ALPHA_IDX[ac.charCodeAt(h)];
    }
    s.hash[i] = ((r[0] * 2654435761) >>> 0) / 4294967296;
  }
  s.bucket = GROUPS.map(() => new Int32Array(n));
  s.bcount = new Int32Array(GROUPS.length);
  s.fem = new Int32Array(n);
  s.vis = new Uint8Array(n);
  s.animG = new Int32Array(0);
  s.animC = new Int32Array(0);
  return s;
}

function actIndexByName(s, name) {
  return s.acts.findIndex((a) => a[0] === name);
}

async function loadSchedule(day) {
  if (loadingDay === day) return;
  loadingDay = day;
  try {
    const d = await getJSON('/api/schedules?day=' + day);
    if (!roleGroup.length) {
      roleGroup = d.roles.map((r) => GROUP_INDEX[SECTOR_GROUP[r[1]] || 'elders']);
    } else if (roleGroup.length !== d.roles.length) {
      roleGroup = d.roles.map((r) => GROUP_INDEX[SECTOR_GROUP[r[1]] || 'elders']);
    }
    const next = parseSched(d);
    // People continue from where they are: carry the displayed position over.
    if (sched) {
      const at = new Map();
      for (let i = 0; i < sched.n; i++) at.set(sched.ids[i], i);
      for (let i = 0; i < next.n; i++) {
        const j = at.get(next.ids[i]);
        if (j !== undefined) { next.startX[i] = sched.cx[j]; next.startY[i] = sched.cy[j]; }
        else { next.startX[i] = next.stops[next.off[i] + 2 * next.seq[i * 24]]; next.startY[i] = next.stops[next.off[i] + 2 * next.seq[i * 24] + 1]; }
      }
    } else {
      for (let i = 0; i < next.n; i++) {
        const k = next.seq[i * 24];
        next.startX[i] = next.stops[next.off[i] + 2 * k]; next.startY[i] = next.stops[next.off[i] + 2 * k + 1];
      }
    }
    // activity filter options
    const sel = $('f-act'), keep = filter.act;
    sel.length = 1;
    d.acts.slice().sort((a, b) => a[1].localeCompare(b[1])).forEach((a) => {
      const o = document.createElement('option');
      o.value = a[0]; o.textContent = a[1]; sel.appendChild(o);
    });
    sel.value = keep;
    sched = next;
    schedDay = d.day;
  } catch (e) {
    setStatus('schedules unavailable');
  } finally {
    loadingDay = -1;
  }
}

// --- polling -------------------------------------------------------------------
function estFrac(now) {
  return clock.base + (now - clock.at) / 1000 / clock.spd;
}

async function pollState() {
  try {
    const s = await getJSON('/api/state');
    const now = performance.now();
    const spd = s.seconds_per_day > 0 ? s.seconds_per_day : BASE_SPD;
    const serverFrac = Math.min(0.9999, (s.since_step_ms || 0) / 1000 / spd);
    if (s.paused && !clock.paused) clock.frozen = clock.valid ? Math.min(0.9999, estFrac(now)) : serverFrac;
    if (!clock.valid || state === null || s.day !== state.day) {
      clock.base = serverFrac; clock.at = now;
    } else {
      const est = estFrac(now);
      const diff = serverFrac - est;
      clock.base = Math.abs(diff) < 0.08 ? est + diff * 0.3 : serverFrac;
      clock.at = now;
    }
    clock.spd = spd; clock.paused = s.paused; clock.valid = true;
    state = s;
    $('day').textContent = s.day;
    $('season').textContent = s.season;
    $('pop').textContent = s.population;
    updateSpeedButtons(s);
    if (schedDay !== s.day) loadSchedule(s.day);
    setStatus(fps ? fps + ' fps' : 'live');
  } catch (e) {
    setStatus('waiting for the simulation...');
  }
}

async function pollPeople() {
  try {
    const p = await getJSON('/api/people');
    const iSick = p.fields.indexOf('sick');
    let sick = 0;
    for (const r of p.rows) sick += r[iSick];
    $('pop').title = sick + ' sick';
  } catch (e) { /* the state poll reports connection trouble */ }
}

function post(action, value) {
  const body = value === undefined ? { action } : { action, value };
  return fetch('/api/control', { method: 'POST', body: JSON.stringify(body) }).then(pollState).catch(() => {});
}

function updateSpeedButtons(s) {
  document.querySelectorAll('#speed button').forEach((b) => {
    const k = b.dataset.s;
    let on = false;
    if (k === 'pause') on = s.paused;
    else on = !s.paused && Math.abs(s.seconds_per_day - SPEEDS[k]) < 0.05;
    b.classList.toggle('on', on);
  });
}

// --- clock and sky ----------------------------------------------------------------
function hourNow() {
  if (FIXED_HOUR !== null) return FIXED_HOUR;
  if (!clock.valid) return 0;
  let frac = clock.paused ? clock.frozen : estFrac(performance.now());
  if (state && schedDay !== state.day) frac = Math.min(frac, 0.9999);   // wait for the new day's schedules
  return Math.max(0, Math.min(0.9999, frac)) * 24;
}

function nightAlpha(h) {
  if (h < 5 || h >= 19.5) return 0.55;
  if (h < 7) return 0.55 * (7 - h) / 2;
  if (h >= 17.5) return 0.55 * (h - 17.5) / 2;
  return 0;
}

function drawClock(h) {
  const hh = Math.floor(h), mm = Math.floor((h - hh) * 60);
  $('clock').textContent = String(hh).padStart(2, '0') + ':' + String(mm).padStart(2, '0');
  $('sky').textContent = h >= 6 && h < 18 ? '\u2600' : '\u263E';
}

// --- view ---------------------------------------------------------------------------
function resize() {
  const r = canvas.getBoundingClientRect();
  view.dpr = Math.min(2, window.devicePixelRatio || 1);
  view.w = r.width; view.h = r.height;
  canvas.width = Math.max(1, Math.round(r.width * view.dpr));
  canvas.height = Math.max(1, Math.round(r.height * view.dpr));
  view.fit = Math.min(view.w / 2000, view.h / 1400);
}

function fitView() {
  resize();
  view.scale = view.fit;
  view.x = (view.w - 2000 * view.scale) / 2;
  view.y = (view.h - 1400 * view.scale) / 2;
}

function zoomAt(px, py, k) {
  const ns = Math.max(view.fit * 0.6, Math.min(view.fit * 12, view.scale * k));
  const wx = (px - view.x) / view.scale, wy = (py - view.y) / view.scale;
  view.scale = ns; view.x = px - wx * ns; view.y = py - wy * ns;
}

let drag = null;
canvas.addEventListener('pointerdown', (e) => { drag = { x: e.clientX, y: e.clientY }; canvas.setPointerCapture(e.pointerId); canvas.classList.add('drag'); });
canvas.addEventListener('pointermove', (e) => {
  if (!drag) return;
  view.x += e.clientX - drag.x; view.y += e.clientY - drag.y;
  drag.x = e.clientX; drag.y = e.clientY;
});
const endDrag = () => { drag = null; canvas.classList.remove('drag'); };
canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', endDrag);
canvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  const r = canvas.getBoundingClientRect();
  zoomAt(e.clientX - r.left, e.clientY - r.top, e.deltaY < 0 ? 1.18 : 1 / 1.18);
}, { passive: false });
$('z-in').onclick = () => zoomAt(view.w / 2, view.h / 2, 1.4);
$('z-out').onclick = () => zoomAt(view.w / 2, view.h / 2, 1 / 1.4);
$('z-fit').onclick = fitView;
window.addEventListener('resize', () => { const f = view.fit; resize(); if (Math.abs(view.scale - f) < 1e-6) fitView(); });

document.querySelectorAll('#speed button').forEach((b) => {
  b.onclick = () => {
    const k = b.dataset.s;
    if (k === 'pause') post('pause');
    else post('speed', SPEEDS[k]).then(() => post('resume'));
  };
});
$('f-tribe').onchange = (e) => { filter.tribe = parseInt(e.target.value, 10); };
$('f-group').onchange = (e) => { filter.group = parseInt(e.target.value, 10); };
$('f-act').onchange = (e) => { filter.act = e.target.value; };
$('labels').onchange = (e) => { showLabels = e.target.checked; };
$('animals').onchange = (e) => { showAnimals = e.target.checked; };
GROUPS.forEach((g, i) => {
  const o = document.createElement('option');
  o.value = i; o.textContent = g.name; $('f-group').appendChild(o);
});

// --- legend ----------------------------------------------------------------------------
(function buildLegend() {
  let h = '<h4>People</h4>';
  GROUPS.forEach((g) => { h += '<div class="row"><span class="dot" style="background:' + g.color + '"></span>' + g.name + '</div>'; });
  h += '<div class="row"><span class="ring"></span>Ring: women and girls</div>';
  h += '<div class="row"><span class="dot small" style="background:#ffe08a"></span>Small dot: children</div>';
  h += '<div class="row"><span class="dot small" style="background:#e8e0c8"></span>Tiny dots: herd animals</div><hr><h4>Valley</h4>';
  [['#2f7fe0', 'Well'], ['#8fc46a', 'Field'], ['#2e7d32', 'Palm grove'], ['rgba(170,200,100,.7)', 'Pasture'],
   ['#e0a030', 'Suq'], ['#fafafa', 'Mosque'], ['#8e44ad', 'Majlis'], ['#7a5a2c', 'Caravan road']]
    .forEach((x) => { h += '<div class="row"><span class="dot" style="background:' + x[0] + '"></span>' + x[1] + '</div>'; });
  $('legend').innerHTML = h;
})();

// --- drawing ----------------------------------------------------------------------------
function drawWorld() {
  const d = view.dpr;
  ctx.setTransform(d, 0, 0, d, 0, 0);
  ctx.fillStyle = '#e8d9b0';
  ctx.fillRect(0, 0, view.w, view.h);
  if (!map) return;
  ctx.setTransform(view.scale * d, 0, 0, view.scale * d, view.x * d, view.y * d);
  for (const t of mapPaths.terrain) { ctx.fillStyle = t.fill; ctx.fill(t.path); }
  ctx.lineWidth = 6; ctx.strokeStyle = '#7a5a2c'; ctx.setLineDash([18, 10]);
  ctx.stroke(mapPaths.road);
  ctx.setLineDash([]);
  ctx.fillStyle = 'rgba(217,194,143,0.9)'; ctx.fill(mapPaths.settle);
  ctx.lineWidth = 1.5; ctx.strokeStyle = 'rgba(138,109,59,0.7)'; ctx.stroke(mapPaths.settle);
  for (const k in mapPaths.byKind) {
    ctx.fillStyle = KIND_STYLE[k] ? KIND_STYLE[k][0] : '#444';
    ctx.fill(mapPaths.byKind[k]);
  }
}

function drawLabels() {
  if (!showLabels || !map) return;
  const d = view.dpr;
  ctx.setTransform(d, 0, 0, d, 0, 0);
  ctx.textAlign = 'center';
  ctx.fillStyle = '#222';
  ctx.font = '600 ' + Math.max(11, Math.min(22, 13 * view.scale / view.fit)) + 'px system-ui, sans-serif';
  for (const s of map.settlements) ctx.fillText(s.name, s.x * view.scale + view.x, (s.y - s.radius - 8) * view.scale + view.y);
  if (view.scale / view.fit > 1.4) {
    ctx.font = '11px system-ui, sans-serif';
    for (const o of mapPaths.named) ctx.fillText(o[0].replace('_', ' '), o[3] * view.scale + view.x, (o[4] - o[5] - 4) * view.scale + view.y);
  }
}

function drawPeople(h) {
  const s = sched;
  if (!s) return;
  const d = view.dpr;
  const hi = Math.min(23, Math.floor(h)), f = h - hi;
  const sc = view.scale, vx = view.x, vy = view.y;
  const x0 = -vx / sc - 20, x1 = (view.w - vx) / sc + 20, y0 = -vy / sc - 20, y1 = (view.h - vy) / sc + 20;
  const bc = s.bcount; bc.fill(0);
  let nf = 0;
  const walkT = Math.min(1, f / WALK), ease = walkT * walkT * (3 - 2 * walkT);
  const actSel = filter.act ? actIndexByName(s, filter.act) : -1;
  if (filter.act && actSel < 0) { /* no one does it today */ }
  const herdA = actIndexByName(s, 'herd'), herdC = actIndexByName(s, 'herd_camels');
  let nAG = 0, nAC = 0;
  if (s.animG.length < s.n) { s.animG = new Int32Array(s.n); s.animC = new Int32Array(s.n); }
  const animalsG = s.animG, animalsC = s.animC;
  for (let i = 0; i < s.n; i++) {
    const o = i * 24, so = s.off[i];
    const ti = so + 2 * s.seq[o + hi];
    const tx = s.stops[ti], ty = s.stops[ti + 1];
    let fx, fy;
    if (hi > 0) { const fi = so + 2 * s.seq[o + hi - 1]; fx = s.stops[fi]; fy = s.stops[fi + 1]; }
    else { fx = s.startX[i]; fy = s.startY[i]; }
    let x = tx, y = ty, moving = false;
    if (fx !== tx || fy !== ty) { x = fx + (tx - fx) * ease; y = fy + (ty - fy) * ease; moving = walkT < 1; }
    s.cx[i] = x; s.cy[i] = y;
    if (x < x0 || x > x1 || y < y0 || y > y1) { s.vis[i] = 0; continue; }
    if (filter.tribe >= 0 && s.tribe[i] !== filter.tribe) { s.vis[i] = 0; continue; }
    if (filter.group >= 0 && s.group[i] !== filter.group) { s.vis[i] = 0; continue; }
    if (filter.act && s.act[o + hi] !== actSel) { s.vis[i] = 0; continue; }
    s.vis[i] = 1;
    const g = s.group[i];
    s.bucket[g][bc[g]++] = i;
    if (s.gender[i] === 1) s.fem[nf++] = i;
    if (showAnimals && !moving) {
      const a = s.act[o + hi];
      if (a === herdA) animalsG[nAG++] = i;
      else if (a === herdC) animalsC[nAC++] = i;
    }
  }
  ctx.setTransform(d, 0, 0, d, 0, 0);
  const rA = Math.max(1.6, 2.7 * Math.pow(sc / 0.7, 0.6));
  // herd animals first (under the herders)
  if (nAG || nAC) {
    const ra = Math.max(0.9, rA * 0.45);
    const drawAnimals = (list, len, color, count, spread) => {
      ctx.fillStyle = color; ctx.beginPath();
      for (let q = 0; q < len; q++) {
        const i = list[q];
        for (let k = 0; k < count; k++) {
          const a = (s.hash[i] * 6.283) + k * (6.283 / count) + Math.sin(h * 0.7 + k + s.hash[i] * 9) * 0.3;
          const r = spread * (0.7 + 0.5 * ((k * 0.37 + s.hash[i] * 3) % 1));
          const px = (s.cx[i] + Math.cos(a) * r) * sc + vx, py = (s.cy[i] + Math.sin(a) * r) * sc + vy;
          ctx.moveTo(px + ra, py); ctx.arc(px, py, ra, 0, 6.283);
        }
      }
      ctx.fill();
    };
    drawAnimals(animalsG, nAG, '#f3ecd2', 5, 14);
    drawAnimals(animalsC, nAC, '#8b5a2b', 3, 18);
  }
  for (let g = 0; g < GROUPS.length; g++) {
    const n = bc[g];
    if (!n) continue;
    ctx.fillStyle = GROUPS[g].color;
    ctx.beginPath();
    const r = g === 0 ? rA * 0.7 : rA, list = s.bucket[g];
    for (let k = 0; k < n; k++) {
      const i = list[k];
      const px = s.cx[i] * sc + vx, py = s.cy[i] * sc + vy;
      ctx.moveTo(px + r, py); ctx.arc(px, py, r, 0, 6.283);
    }
    ctx.fill();
  }
  if (nf) {   // women and girls get a light ring
    ctx.strokeStyle = 'rgba(255,255,255,0.95)'; ctx.lineWidth = Math.max(0.8, rA * 0.3);
    ctx.beginPath();
    for (let k = 0; k < nf; k++) {
      const i = s.fem[k];
      const r = s.group[i] === 0 ? rA * 0.7 : rA;
      const px = s.cx[i] * sc + vx, py = s.cy[i] * sc + vy;
      ctx.moveTo(px + r, py); ctx.arc(px, py, r, 0, 6.283);
    }
    ctx.stroke();
  }
}

function drawNight(h) {
  const a = nightAlpha(h);
  if (a <= 0) return;
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.fillStyle = 'rgba(8,14,52,' + a.toFixed(3) + ')';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
}

function frame(t) {
  requestAnimationFrame(frame);
  if (t - lastDraw < 30) return;      // about 30 fps
  lastDraw = t;
  const h = hourNow();
  drawWorld();
  drawNight(h);
  drawLabels();
  drawPeople(h);
  drawClock(h);
  fpsFrames++;
  if (t - fpsAt > 2000) { fps = Math.round(fpsFrames * 1000 / (t - fpsAt)); fpsFrames = 0; fpsAt = t; }
}

// --- start -------------------------------------------------------------------------------
resize();
loadMap().catch(() => setStatus('map unavailable'));
pollState();
setInterval(pollState, 500);
setInterval(pollPeople, 2000);
requestAnimationFrame(frame);
window.__viewer = { get fps() { return fps; }, get day() { return schedDay; }, get people() { return sched ? sched.n : 0; } };
