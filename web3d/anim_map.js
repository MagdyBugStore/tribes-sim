// Simulation activity -> animation state -> baked clip. Used by crowd.html and later scenes.
// Activity keys come from the server's activity table (acts[i][0]); unknown keys fall back to 'idle'.
// Clips are the ones in the Quaternius Ultimate Animated Character Pack: Idle, Walk, Walk_Carry, SitDown, PickUp.
export const CLIPS = ['Idle', 'Walk', 'Walk_Carry', 'SitDown', 'PickUp'];
export const STATE_CLIP = {
  idle: 'Idle', walk: 'Walk', run: 'Walk', carry: 'Walk_Carry', sit: 'SitDown', rest: 'SitDown',
  work: 'PickUp', herd: 'Walk', trade: 'Idle', pray: 'Idle', study: 'SitDown',
};
// Exact clip missing in the open assets (mapped to the closest one):
export const GAPS = {
  run: 'no run for people in the pack (uses Walk)', 'work-field': 'no hoe or dig (uses PickUp)', 'draw-water': 'no draw water (PickUp)',
  milk: 'none (Idle)', bake: 'none (PickUp)', smith: 'none (PickUp)', weave: 'none (SitDown)', pray: 'no bow or kneel (Idle)',
  sleep: 'person is hidden inside the home, so no clip needed',
};
export function stateFor(activity) {
  const a = String(activity || '');
  if (/sleep/.test(a)) return 'hidden';
  if (/pray|jumuah/.test(a)) return 'pray';
  if (/sit|majlis|compose|study|school|rest/.test(a)) return 'sit';
  if (/carry|fetch|haul|deliver|travel|caravan/.test(a)) return 'carry';
  if (/herd|graze|shepherd/.test(a)) return 'herd';
  if (/walk|go_|visit|commute/.test(a)) return 'walk';
  if (/trade|buy|sell|market|suq/.test(a)) return 'trade';
  if (/field|grove|water|well|milk|bake|smith|weave|tan|mill|dig|work|craft|gather|hunt/.test(a)) return 'work';
  return 'idle';
}
