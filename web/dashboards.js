// Tribes of the Valley: dashboards (economy, market, herds, caravans, alerts)
// and the event ticker. Data: /api/dash (rolling daily history kept by the
// server, so a page reload gets the last 365 days back), /api/caravans for
// the live caravans drawn on the map. Shares globals with viewer.js.
'use strict';

const DASH_MAX = 365;
const D = { items: [], units: [], base: [], livestock: [], tribes: [], days: [], caravans: [], events: [], last: -1, car: 0, ev: 0 };
let liveCaravans = [];
let dashOpen = false, dashTab = 'econ', dashVersion = 0, drawnVersion = -1, drawnTab = '';
let selItem = -1, selHerd = 0;
const dbody = $('dash-body');
const PAL = ['#59b35a', '#4a90e2', '#f0902e', '#ef5b5b', '#b37be0', '#4fd1c5', '#e8c86a', '#ff8fb8', '#8a93a6'];
const fmt0 = (v) => Math.round(v).toLocaleString('en-US');
const fmt2 = (v) => v.toFixed(2);

// --- data -------------------------------------------------------------------------------
async function pollDash() {
  try {
    const d = await getJSON(`/api/dash?since=${D.last}&car=${D.car}&ev=${D.ev}`);
    if (d.last_day >= 0 && d.last_day < D.last) {           // the simulation restarted
      D.days = []; D.caravans = []; D.events = []; D.last = -1; D.car = 0; D.ev = 0;
      return pollDash();
    }
    D.items = d.items; D.units = d.units; D.base = d.base; D.livestock = d.livestock; D.tribes = d.tribes;
    let changed = false;
    for (const r of d.days) { D.days.push(r); D.last = r.d; changed = true; }
    if (D.days.length > DASH_MAX) D.days.splice(0, D.days.length - DASH_MAX);
    for (const c of d.caravans) { D.car = Math.max(D.car, c.seq); D.caravans.push(c); changed = true; }
    if (d.days.length && dashOpen && dashTab === 'caravans') refreshCaravanLog();   // open visits change in place
    if (D.caravans.length > 200) D.caravans.splice(0, D.caravans.length - 200);
    for (const e of d.events) { D.ev = Math.max(D.ev, e.seq); D.events.push(e); changed = true; tickerAdd(e); }
    if (D.events.length > 400) D.events.splice(0, D.events.length - 400);
    if (d.days.length) { famineCheck(d.days[d.days.length - 1]); changed = true; }
    if (changed) dashVersion++;
  } catch (e) { /* the state poll reports connection trouble */ }
}
// The server only sends caravan visits once; refresh open ones through a full read when needed.
async function refreshCaravanLog() {
  try {
    const d = await getJSON('/api/dash?since=1000000000&car=0&ev=1000000000');
    D.caravans = d.caravans; D.car = D.caravans.length ? D.caravans[D.caravans.length - 1].seq : 0;
    dashVersion++;
  } catch (e) { /* ignore */ }
}

// --- alerts and ticker --------------------------------------------------------------------
const ALERTS = [];            // {day, kind, text, hot}
function classify(text) {
  const t = text.toLowerCase();
  if (t.includes('ruined') || t.includes('failed')) return ['caravan', true];
  if (t.includes('caravan')) return ['caravan', false];
  if (t.includes('well')) return ['well', false];
  if (t.includes('eid')) return ['eid', false];
  if (t.includes('wedding') || t.includes('marries') || t.includes('married')) return ['wedding', false];
  if (t.includes('dies') || t.includes('died')) return ['death', false];
  if (t.includes('born') || t.includes('birth')) return ['birth', false];
  if (t.includes('could not pay')) return ['money', false];
  if (t.includes('famine') || t.includes('hunger')) return ['famine', true];
  return ['event', false];
}
const tickerEl = $('ticker');
let tickerTimer = 0;
function pushAlert(day, kind, text, hot) {
  ALERTS.push({ day, kind, text, hot });
  if (ALERTS.length > 500) ALERTS.splice(0, ALERTS.length - 500);
  dashVersion++;
}
function tickerAdd(e) {
  const [kind, hot] = classify(e.text);
  const day = e.day;
  // routine births and deaths of ordinary people would flood the ticker: keep notable kinds in the ticker
  pushAlert(day, kind, e.text, hot);
  if (kind === 'birth' || kind === 'money' || (kind === 'death' && !/Sheikh|Imam|Qadi|Hakim|Elder/i.test(e.text))) return;
  showTicker(e.text, hot);
}
function showTicker(text, hot) {
  tickerEl.hidden = false; tickerEl.textContent = ar(text); tickerEl.classList.toggle('hot', !!hot);
  clearTimeout(tickerTimer);
  tickerTimer = setTimeout(() => { tickerEl.hidden = true; }, 9000);
}
let famine = false, lastJumuah = -1;
function famineCheck(r) {
  const share = r.hungry / Math.max(1, r.pop);
  if (share > 0.05 && !famine) {
    famine = true;
    const text = `اليوم ${r.d}: خطر مجاعة، ${r.hungry} شخصًا (${Math.round(share * 100)}%) جاعوا اليوم`;
    pushAlert(r.d, 'famine', text, true); showTicker(text, true);
  } else if (share <= 0.03) famine = false;
  if (r.d % 7 === 5 && r.d !== lastJumuah) { lastJumuah = r.d; pushAlert(r.d, 'jumuah', `اليوم ${r.d}: الجمعة، صلاة الجمعة في المسجد`, false); }
}

// --- charts ---------------------------------------------------------------------------------
function setupCanvas(c) {
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  const w = c.clientWidth, h = c.clientHeight;
  if (c.width !== Math.round(w * dpr) || c.height !== Math.round(h * dpr)) { c.width = Math.round(w * dpr); c.height = Math.round(h * dpr); }
  const g = c.getContext('2d');
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, w, h);
  return { g, w, h };
}
function niceMax(v) {
  if (v <= 0) return 1;
  const p = Math.pow(10, Math.floor(Math.log10(v)));
  for (const m of [1, 1.5, 2, 3, 5, 7.5, 10]) if (v <= m * p) return m * p;
  return 10 * p;
}
// series: [{label, color, data:[...], dash?}], xs: day numbers; opts {min, max, fmt}
function lineChart(canvas, xs, series, opts = {}) {
  const { g, w, h } = setupCanvas(canvas);
  const L = 46, R = 8, T = 6, B = 16;
  let lo = opts.min !== undefined ? opts.min : Infinity, hi = opts.max !== undefined ? opts.max : -Infinity;
  for (const s of series) for (const v of s.data) { if (opts.min === undefined && v < lo) lo = v; if (opts.max === undefined && v > hi) hi = v; }
  if (!isFinite(lo)) { lo = 0; hi = 1; }
  if (opts.max === undefined) hi = lo === 0 || opts.zero ? niceMax(hi) : hi + (hi - lo) * 0.08;
  if (opts.min === undefined && !opts.zero) lo = lo - (hi - lo) * 0.08;
  if (hi - lo < 1e-9) { hi = lo + 1; }
  const f = opts.fmt || fmt0;
  g.font = '10px system-ui, sans-serif'; g.fillStyle = '#8a8a96'; g.strokeStyle = '#34343c'; g.lineWidth = 1;
  g.textAlign = 'right';
  for (let k = 0; k <= 4; k++) {
    const v = lo + (hi - lo) * k / 4, y = T + (h - T - B) * (1 - k / 4);
    g.beginPath(); g.moveTo(L, y); g.lineTo(w - R, y); g.stroke();
    g.fillText(f(v), L - 4, y + 3);
  }
  const n = xs.length;
  if (n < 1) return;
  const px = (i) => L + (n === 1 ? 0 : (w - L - R) * i / (n - 1));
  const py = (v) => T + (h - T - B) * (1 - (v - lo) / (hi - lo));
  g.textAlign = 'center';
  g.fillText('يوم ' + xs[0], L + 12, h - 3);
  g.fillText('يوم ' + xs[n - 1], w - R - 18, h - 3);
  for (const s of series) {
    g.strokeStyle = s.color; g.lineWidth = s.width || 1.6; g.setLineDash(s.dash || []);
    g.beginPath();
    s.data.forEach((v, i) => (i ? g.lineTo(px(i), py(v)) : g.moveTo(px(i), py(v))));
    g.stroke();
  }
  g.setLineDash([]);
}
function chartBox(title, series, id) {
  return `<div class="chart"><h6>${ar(title)}</h6><div class="legendrow">${series.map((s) => `<span><i style="background:${s.color}"></i>${ar(s.label)}</span>`).join('')}</div><canvas id="${id}"></canvas></div>`;
}

// --- tabs ---------------------------------------------------------------------------------------
function econ() {
  const days = D.days, xs = days.map((r) => r.d);
  const last = days[days.length - 1];
  if (drawnTab !== 'econ') {
    const money = [{ label: 'total (constant)', color: '#ffffff', dash: [4, 3] }, { label: 'pouches', color: PAL[0] }, { label: 'Bayt al-Mal', color: PAL[1] }, { label: 'shops', color: PAL[2] }, { label: 'caravans', color: PAL[3] }, { label: 'Majlis + mint', color: PAL[4] }];
    dbody.innerHTML = `<div class="summary" id="sum"></div><div class="grid2">
      ${chartBox('Money supply and where it sits (dirhams)', money, 'c-money')}
      ${chartBox('Gini of wealth', [{ label: 'gini', color: PAL[6] }], 'c-gini')}
      ${chartBox('Hungry, thirsty and sick people', [{ label: 'hungry', color: PAL[2] }, { label: 'thirsty', color: PAL[1] }, { label: 'sick', color: PAL[3] }], 'c-need')}
      ${chartBox('Births and deaths per week', [{ label: 'births', color: PAL[0] }, { label: 'deaths', color: PAL[3] }], 'c-bd')}</div>`;
  }
  if (!last) return;
  // same wording and numbers as the console's `world` command
  $('sum').textContent = `اليوم ${last.d} | السكان ${last.pop} | الدراهم ${fmt0(last.money)} (ثابتة سليمة) | الأكياس ${fmt0(last.pouches)} / بيت المال ${fmt0(last.bayt)} | جيني ${last.gini.toFixed(3)} | جائعون ${last.hungry} عطشى ${last.thirsty} | مرضى ${last.sick} | آبار ${last.wells} | قوافل ${last.arrived} (${last.failed} فاشلة)`;
  lineChart($('c-money'), xs, [
    { color: '#ffffff', data: days.map((r) => r.money), dash: [4, 3] },
    { color: PAL[0], data: days.map((r) => r.pouches) }, { color: PAL[1], data: days.map((r) => r.bayt) },
    { color: PAL[2], data: days.map((r) => r.tills) }, { color: PAL[3], data: days.map((r) => r.purses) },
    { color: PAL[4], data: days.map((r) => r.other) }], { min: 0, zero: true });
  lineChart($('c-gini'), xs, [{ color: PAL[6], data: days.map((r) => r.gini) }], { min: 0, max: 1, fmt: fmt2 });
  lineChart($('c-need'), xs, [
    { color: PAL[2], data: days.map((r) => r.hungry) }, { color: PAL[1], data: days.map((r) => r.thirsty) },
    { color: PAL[3], data: days.map((r) => r.sick) }], { min: 0, zero: true });
  const wk = (key) => days.map((r, i) => { let s = 0; for (let k = Math.max(0, i - 6); k <= i; k++) s += days[k][key]; return s; });
  lineChart($('c-bd'), xs, [{ color: PAL[0], data: wk('births') }, { color: PAL[3], data: wk('deaths') }], { min: 0, zero: true });
}

function trend(now, before) {
  if (!(before > 0) || !(now > 0)) return '=';
  const r = now / before;
  return r > 1.03 ? '<span class="up">&#9650;</span>' : r < 0.97 ? '<span class="down">&#9660;</span>' : '=';
}
function dayStock(i) {   // days of stock left in shops at the recent consumption rate
  const days = D.days, n = days.length;
  if (!n) return null;
  let use = 0, k = 0;
  for (let q = Math.max(0, n - 7); q < n; q++) { use += days[q].use[i]; k++; }
  use /= Math.max(1, k);
  if (use <= 0.01) return null;
  return days[n - 1].stock[i] / use;
}
function market() {
  const days = D.days, n = days.length;
  if (!n) { dbody.textContent = 'بانتظار اليوم الأول...'; return; }
  const last = days[n - 1], prev = days[Math.max(0, n - 8)];
  if (drawnTab !== 'market') {
    let rows = '';
    D.items.forEach((nm, i) => { rows += `<tr data-i="${i}"><td>${esc(nm)}</td><td></td><td></td><td></td><td></td><td></td></tr>`; });
    dbody.innerHTML = `<div class="split"><div><table class="t" id="mt"><thead><tr><th>السلعة</th><th>السعر</th><th>مقابل الأساس</th><th>7 أيام</th><th>مخزون الدكاكين</th><th>أيام المخزون</th></tr></thead><tbody>${rows}</tbody></table></div>
      <div class="chart"><h6 id="mh">اضغط على سلعة لعرض تاريخ سعرها (آخر 90 يومًا)</h6><canvas id="c-price" style="height:200px"></canvas></div></div>`;
    $('mt').querySelectorAll('tbody tr').forEach((tr) => { tr.onclick = () => { selItem = parseInt(tr.dataset.i, 10); drawnVersion = -1; render(); }; });
  }
  const trs = $('mt').querySelectorAll('tbody tr');
  D.items.forEach((nm, i) => {
    const tr = trs[i], td = tr.children;
    const p = last.price[i];
    const none = p <= 0 && last.stock[i] <= 0;
    td[1].textContent = p > 0 ? fmt2(p) + ' ' + ar(D.units[i]) : '-';
    td[2].textContent = p > 0 && D.base[i] > 0 ? Math.round(p / D.base[i] * 100) + '%' : '-';
    td[3].innerHTML = trend(p, prev.price[i]);
    td[4].textContent = none ? '-' : fmt0(last.stock[i]);
    const ds = dayStock(i);
    td[5].textContent = ds === null ? '-' : ds >= 100 ? '99+' : ds.toFixed(1);
    tr.classList.toggle('low', ds !== null && ds < 7);
    tr.classList.toggle('sel', i === selItem);
  });
  if (selItem >= 0) {
    const m = days.slice(-90);
    $('mh').textContent = `${ar(D.items[selItem])}: متوسط السعر في آخر ${m.length} يومًا`;
    lineChart($('c-price'), m.map((r) => r.d), [{ color: PAL[2], data: m.map((r) => r.price[selItem]) }, { color: '#8a93a6', dash: [4, 3], data: m.map(() => D.base[selItem]) }], { fmt: fmt2 });
  }
}

function herds() {
  const days = D.days, n = days.length;
  if (!n) { dbody.textContent = 'بانتظار اليوم الأول...'; return; }
  const last = days[n - 1], span = days.slice(-30);
  if (drawnTab !== 'herds') {
    dbody.innerHTML = `<div class="split"><div><table class="t" id="ht"><thead><tr><th>الحيوان</th><th>المجموع</th>${D.tribes.map((t) => `<th>${esc(t)}</th>`).join('')}<th>مواليد 30 يومًا</th><th>مذبوح 30 يومًا</th></tr></thead><tbody>${D.livestock.map((a, k) => `<tr data-k="${k}"><td>${esc(a)}</td>${'<td></td>'.repeat(D.tribes.length + 3)}</tr>`).join('')}</tbody></table></div>
      <div class="chart"><h6 id="hh"></h6><canvas id="c-herd" style="height:200px"></canvas></div></div>`;
    $('ht').querySelectorAll('tbody tr').forEach((tr) => { tr.onclick = () => { selHerd = parseInt(tr.dataset.k, 10); drawnVersion = -1; render(); }; });
  }
  const trs = $('ht').querySelectorAll('tbody tr');
  D.livestock.forEach((a, k) => {
    const td = trs[k].children;
    td[1].textContent = fmt0(last.ht[k]);
    D.tribes.forEach((_, t) => { td[2 + t].textContent = fmt0(last.hx[t][k]); });
    let b = 0, s = 0;
    for (const r of span) { b += r.hb[k]; s += r.hs[k]; }
    td[2 + D.tribes.length].textContent = fmt0(b);
    td[3 + D.tribes.length].textContent = fmt0(s);
    trs[k].classList.toggle('sel', k === selHerd);
  });
  $('hh').textContent = `${ar(D.livestock[selHerd])}: عدد الرؤوس خلال آخر ${days.length} يومًا`;
  lineChart($('c-herd'), days.map((r) => r.d), [{ color: PAL[0], data: days.map((r) => r.ht[selHerd]) }], { zero: true });
}

function caravansTab() {
  if (drawnTab !== 'caravans') {
    dbody.innerHTML = `<div class="summary" id="cnow"></div><table class="t"><thead><tr><th>الوصول</th><th>من</th><th>المكث</th><th>وحدات الحمولة</th><th>أكبر حمولة</th><th>بيع للأهالي</th><th>شراء من الأهالي</th><th>النتيجة</th></tr></thead><tbody id="clog"></tbody></table>`;
  }
  const act = liveCaravans.filter((c) => c.state === 'in_valley');
  $('cnow').textContent = act.length ? 'في الوادي الآن: ' + act.map((c) => `${ar(c.origin)} (اليوم ${c.days_in_valley + 1}، الكيس ${fmt0(c.purse)})`).join('؛ ')
    : 'لا قافلة في الوادي. التالية: ' + (liveCaravans.length ? liveCaravans.map((c) => `${ar(c.origin)} في اليوم ${c.next_arrival_day}`).sort().slice(0, 3).join(', ') : '-');
  const rows = D.caravans.slice().reverse().map((c) => `<tr><td>اليوم ${c.day}</td><td>${esc(c.origin)}</td><td>${c.stay} أيام</td><td>${fmt0(c.cargo_units)}</td><td>${esc(c.top)}</td><td>${fmt0(c.sold)}</td><td>${fmt0(c.bought)}</td><td>${c.failed ? '<span class="up">فشلت: ضاعت الحمولة</span>' : 'سليمة'}</td></tr>`).join('');
  $('clog').innerHTML = rows || '<tr><td colspan="8">لم تصل قافلة بعد</td></tr>';
}

function alertsTab() {
  if (drawnTab !== 'alerts') dbody.innerHTML = '<div id="alist"></div>';
  $('alist').innerHTML = ALERTS.slice(-150).reverse().map((a) => `<div class="evrow ${a.hot ? 'hot' : ''}"><span class="k">${ar(a.kind)}</span>${esc(a.text)}</div>`).join('') || 'لا شيء بعد';
}

function render() {
  if (!dashOpen) return;
  if (drawnVersion === dashVersion && drawnTab === dashTab) return;
  const fn = { econ, market, herds, caravans: caravansTab, alerts: alertsTab }[dashTab];
  fn();
  drawnTab = dashTab; drawnVersion = dashVersion;
  $('dash-note').textContent = D.days.length ? `سجل ${D.days.length} يومًا` : '';
}
function openDash(v) {
  dashOpen = v; $('dash').hidden = !v;
  $('dash-btn').classList.toggle('on', v);
  if (v) { drawnTab = ''; drawnVersion = -1; render(); }
}
$('dash-btn').onclick = () => openDash(!dashOpen);
$('dash-x').onclick = () => openDash(false);
document.querySelectorAll('#tabs button[data-t]').forEach((b) => {
  b.onclick = () => {
    dashTab = b.dataset.t;
    document.querySelectorAll('#tabs button[data-t]').forEach((x) => x.classList.toggle('on', x === b));
    if (dashTab === 'caravans') refreshCaravanLog();
    render();
  };
});
window.addEventListener('resize', () => { drawnVersion = -1; render(); });

// --- the caravan on the map ---------------------------------------------------------------------
async function pollCaravans() {
  try { liveCaravans = (await getJSON('/api/caravans')).caravans; } catch (e) { /* ignore */ }
}
function roadPoint(t) {
  const r = map && map.road;
  if (!r || r.length < 2) return null;
  const f = Math.max(0, Math.min(1, t)) * (r.length - 1), i = Math.min(r.length - 2, Math.floor(f)), u = f - i;
  return [r[i][0] + (r[i + 1][0] - r[i][0]) * u, r[i][1] + (r[i + 1][1] - r[i][1]) * u];
}
window.onFrame = function (h) {
  if (!liveCaravans.length || !map) return;
  let k = 0;
  for (const c of liveCaravans) {
    if (c.state !== 'in_valley') continue;
    // two days in the valley: it enters from the east, trades at the suq, leaves west
    const p = roadPoint((c.days_in_valley + h / 24) / 2);
    if (!p) continue;
    const x = p[0] * view.scale + view.x, y = p[1] * view.scale + view.y + k * 18;
    ctx.save();
    ctx.fillStyle = '#7a4a1e'; ctx.strokeStyle = '#fff'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(x, y, 7, 0, 6.283); ctx.fill(); ctx.stroke();
    ctx.font = '600 12px system-ui, sans-serif'; ctx.textAlign = 'center';
    const label = 'Caravan: ' + c.origin;
    ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(255,255,255,.9)'; ctx.strokeText(label, x, y - 12);
    ctx.fillStyle = '#4a2a0e'; ctx.fillText(label, x, y - 12);
    ctx.restore();
    k++;
  }
};

// --- start ------------------------------------------------------------------------------------------
pollDash(); pollCaravans();
setInterval(pollDash, 2000);
setInterval(pollCaravans, 2000);
setInterval(render, 1000);
window.__dash = { D, ALERTS, get version() { return dashVersion; }, open: openDash, tab: (t) => document.querySelector(`#tabs button[data-t="${t}"]`).click(), pick: (i) => { selItem = i; selHerd = Math.max(0, i); drawnVersion = -1; render(); } };
if (params.has('dash')) { dashTab = params.get('dash') || 'econ'; document.querySelectorAll('#tabs button[data-t]').forEach((x) => x.classList.toggle('on', x.dataset.t === dashTab)); openDash(true); }
