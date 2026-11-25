# Asset catalogue (3D-2, partial)

Rule: only assets whose license was read on the source page **and** in the file shipped inside the pack are listed as "chosen".
Everything is CC0 1.0 unless noted. Download and hash check: `bash tools/fetch_assets.sh` (unpacks to `web3d/assets/`, git-ignored).
Playground: `python3 -m http.server 8000 --directory web3d`, then http://localhost:8000/playground.html (3 people, a horse, a cow and a sheep walking on flat ground, FBX loaded with the vendored FBXLoader).

## Chosen and verified

| Category | Asset | Source | License evidence | Format | Content | Fit |
|---|---|---|---|---|---|---|
| Humans | Ultimate Animated Character Pack (Quaternius) | https://opengameart.org/content/animated-characters-pack | page says CC0; Quaternius page says CC0 | FBX, OBJ, Blend | 52 FBX, shared rig, animations include Idle, Walk, Run, SitDown, PickUp, Victory, Defeat, Shoot_OneHanded | low. Modern, fantasy and soldier characters, no thobe or ghutra. Usable as a base with recolour |
| Humans | LowPoly RPG Characters (Quaternius) | https://opengameart.org/content/lowpoly-rpg-characters | page says CC0; `License.txt` in the zip says CC0 1.0 | FBX | 12 fantasy characters | low |
| Animals | Farm Animals (Quaternius) | https://opengameart.org/content/lowpoly-animated-farm-animal-pack | page CC0; `License.txt` in zip CC0 1.0 | FBX, OBJ, Blend | Cow, Horse, Llama, Pig, Pug, Sheep, Zebra. Clips: Walk, WalkSlow, Run, Idle, Jump, Death | medium. Horse, cow, sheep usable now |
| Animals | Animal Pack Vol.2 (Quaternius) | https://opengameart.org/content/animated-animales-low-poly | page CC0 | FBX | Cat, Dog, Eagle, Piranha, Wolf | dog only |
| Animals | Animals Pack (Quaternius) | https://opengameart.org/content/5-low-poly-animals | page CC0 | FBX | bird, Chick, Fish, Red Fox, Whale | chick only |
| Nature | Nature Pack Vol.3 (Quaternius) | https://opengameart.org/content/lowpoly-nature-pack | page CC0 | FBX | Bush, Grass, Rock, Tree (13 models) | rocks yes; trees are not date palms |
| Crops | LowPoly Crops Pack (Quaternius) | https://opengameart.org/content/lowpoly-crops-pack | page CC0 | FBX | 102 models, growth stages (the pack lists Apple, Bamboo, Beet ...; wheat and date palm not checked) | medium |
| Buildings | LowPoly Medieval Village Pack (Quaternius) | https://opengameart.org/content/lowpoly-medieval-village-pack | page CC0; `License.txt` in zip CC0 1.0 | FBX | 44 models: House 1-4, Mill, Blacksmith, Stable, Inn, Bell Tower, props (Bag, Bags ...) | low for look (European) but fine as placeholders: mill, smithy, stable, props |
| Ground textures | ambientCG Ground061 "Lightly Wet Sand" | https://ambientcg.com/view?id=Ground061 | https://docs.ambientcg.com/license/ says all assets CC0 | JPG PBR maps, 1K zip | sand | good |
| Ground textures | Poly Haven sand_01, rock_01, coast_sand_rocks_02 | https://polyhaven.com/a/sand_01 | https://polyhaven.com/license says all assets CC0 | JPG/EXR via API | sand, rock | good (not downloaded yet) |
| General | Kenney game assets | https://kenney.nl/support | support page: all assets on the asset pages are CC0 | glTF, OBJ, FBX | not browsed for desert items yet | unknown |

## Seen but NOT used (license not fully verified or not CC0)

- Poly Pizza camel by jeremy and by Poly by Google (https://poly.pizza/m/9mu4MbU4QtJ): listed as Creative Commons Attribution, so it would need credit. Version of the CC-BY license not read. Not used.
- Camel models on pixor.art, codelove.in, innerscene.com and similar sites advertise CC0, but I only saw search snippets and did not verify the original author or the license text. Not used.
- three.js sample characters (Soldier.glb, Xbot.glb): one report says the Soldier is a Mixamo model credited as CC0, so the license is disputed. Not used. https://github.com/sceneview/sceneview/issues/4586
- KayKit packs (https://kaylousberg.com/game-assets/characters-adventurers): license text not read. Not used.

## What is not done (acceptance criteria not met)

- Fewer than 3 verified options in humans (2, neither Arabian), animals (no camel, goat, donkey; chicken only as a chick), buildings (no tent, mosque, majlis, suq stall, well or falaj), props (jars, baskets, carts not found), audio (not researched at all).
- No polygon counts recorded, no per-asset fit rating beyond the short note above.
- Terrain code (heightmap, chunked LOD): not catalogued. three.js itself is MIT and its examples include noise helpers, but I did not read them.
- The playground shows characters without their textures (FBX texture links are not resolved), so they look dark. Scale per species is normalised to a fixed height, so the horse looks small next to the cow.
- Only Chrome with software GL was tested.

## Generated textures (3D-3, partial)

`python3 tools/gen_textures.py --seed 1 --size 512` writes 13 seamless PNGs to `web3d/textures/` (sand light and dark, packed earth, gravel, rock, dry grass, field soil, cracked mud, mud-brick, plaster, wood planks, goat-hair stripes, rug). Standard library only, no external images, same seed gives the same bytes. `python3 tests/test_textures.py` checks that opposite edges match and that output is reproducible. Contact sheet: `docs/textures-sheet.png`.
