# Viewer vs console count check (#16)

Checked on 26 Nov by starting `./tribes --serve 8181 --people 300 --days 0 --seed S --web web3d` and comparing the console's founding line ("Valley founded: ...") with the counts the 3D viewer draws from `/api/map` and `/api/state`.

| Seed | Console: shops / wells / fields | /api/map: shop / well / field | Homes in /api/map | Population in /api/state |
|---|---|---|---|---|
| 1 | 8 / 2 / 19 | 8 / 2 / 19 | 168 | 300 |
| 5 | 8 / 2 / 19 | 8 / 2 / 19 | 168 | 300 |
| 42 | 8 / 2 / 18 | 8 / 2 / 18 | 168 | 300 |

Result: the numbers match at founding for all three seeds.

Not checked: a comparison against the console's live `world` command (daily summary) later in a run, and homes against a console number (the founding line has no home count). The viewer HUD takes its counts from the same `/api/map` data, so it follows the table above.
