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
- No clip blending between states. Normals use the bind pose (not animated).
- Role looks are tint (role group) plus size (child 0.62, elder 0.94). No outfit meshes.
- Animal counts are not yet read from `/api/herds`; positions are not driven by the real schedules.
