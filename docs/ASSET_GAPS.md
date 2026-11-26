# Asset gaps (what no verified open source covered in the first pass)

To be filled by procedural models or generated textures in the next issues (3D-3, 3D-5):

- Camel (the most important animal), goat, donkey, dog with a Bedouin look. Only horse, cow, sheep, llama, pig and a dog were found under CC0.
- Humans in Arabian dress: thobe, ghutra or keffiyeh, headscarf, abaya, and child and elder body types on the same rig. Plan: recolour or add simple procedural clothing meshes to the Quaternius rig.
- Animations: carry, dig, hoe, hammer, pray (bow, kneel), draw water, milk, ride. The pack has Idle, Walk, Run, SitDown, PickUp only.
- Buildings: goat-hair tent, mud-brick house, mosque with minaret, majlis hall, suq stalls, well, falaj channel. Plan: simple procedural geometry plus sand and mud textures.
- Date palm and acacia (the nature pack trees are temperate), jars, baskets, firewood, carts, caravan loads.
- Audio: wind, livestock, call to prayer. Not researched; the call to prayer needs a recording with clear rights.

## Crowd and animation (issue #14)
- People clips used: Idle, Walk, Walk_Carry, SitDown, PickUp (map in `web3d/anim_map.js`). No clip for sleeping, praying, praying/prostrating, milking, weaving, cooking.
- Sheep has only Idle and Jump, so sheep slide in Idle. No camel, goat or donkey models.
- Clip blending: done (0.3 s cross-fade in the vertex shader between the previous and current baked clip). Verified only by a counter (36 entities mid-blend) and a no-error render in headless Chrome with software GL; the smoothness itself was not judged by eye. Normals use the bind pose (not animated).
- Role looks are tint (role group) plus size (child 0.62, elder 0.94). No outfit meshes.
- Animal counts are not yet read from `/api/herds`; positions are not driven by the real schedules.

## World scene (issue #16)
`web3d/world.html` is a first pass: static placement from `/api/map` plus one dot per person from the current `/api/people` frame. Missing: people walking schedules over 24h, caravans/camels, events, new wells/fields animation, terrain bands, build animation, tent look (tribe 1 homes render as a messy cluster).

## Procedural livestock (issue #12, decided by the owner's coordinating agent: no CC BY assets)
`web3d/animals.js` builds camel, goat, donkey, horse, cattle, sheep and chicken from boxes (one merged geometry per species, tinted per instance). `world.html` now shows herds with these instead of grey boxes. They are static (no walk or graze animation) and low detail. Sizes were set from real height ratios against the 1.3-unit person capsule; the person capsules in this viewer look larger than that next to the animals, which was already the case. The CC0 wind loop (SFXMint) could not be added: its download endpoint refuses plain downloads without an account key, and I did not work around that. The wind audio is still open.

## Animated people in the real viewer (owner decision, 26 Nov 2026)
`web3d/world.html` now draws people with the Quaternius CC0 Casual_Male and Casual_Female models (baked to vertex-animation textures by `web3d/vat.js`, copied from crowd.html) instead of capsules. Clip per activity from `anim_map.js`, 0.3 s cross-fade, heading follows the walk direction, child 0.62 and elder 0.94 size, tribe tint, near and far level of detail. The old capsule mesh stays as an invisible proxy so clicking a person, follow-cam and the filters work unchanged (a click on a person was checked in headless Chrome). `?people3d=0` shows the old capsules. If `web3d/assets/` is missing (run `bash tools/fetch_assets.sh`) the viewer logs an error and keeps the capsules.
Limits: no Bedouin clothing (the pack has modern casual outfits, only tinted); only 5 clips, so pray, milk, weave and others use Idle, PickUp or SitDown; load time grows by the bake (several seconds on software GL); 1000 people checked only in headless Chrome with software GL, real-GPU fps not measured; Firefox and touch unchecked.
