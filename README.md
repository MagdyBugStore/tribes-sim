# Tribes of the Valley - a desert-tribes trade simulation (C++17)

A deterministic, multi-file C++17 simulation of a small valley of Bedouin
tribes: people are born, learn a trade, marry, work production chains, buy
and sell in tribal markets, keep herds, receive sadaqah when they fall on
hard times, grow old, are cared for by their children, and die. Money and
every physical good are conserved to the last fils and the last gram, and
the books are audited at the end of every simulated day.

## Build and run

```sh
g++ -O2 -std=c++17 -pthread src/*.cpp -o tribes

# interactive console (one tick = one day, every --tick-interval seconds)
./tribes --people 300 --days 365 --seed 1

# headless batch run with a CSV export of the daily numbers
./tribes --people 3000 --days 365 --seed 1 --headless --csv run.csv

# the test suite (plain asserts, no framework; ~2 seconds)
g++ -O2 -std=c++17 -pthread -Isrc tests/run_tests.cpp $(ls src/*.cpp | grep -v main.cpp) -o tests/run_tests
./tests/run_tests

# smoke-test the HTTP API of a running server (curl + python3 only)
bash tests/smoke_api.sh 8080
```

Flags: `--people N` (all CSV quantities scale linearly from their
per-1000-people values), `--days N`, `--seed N` (mt19937_64; the same seed
reproduces a run bit-for-bit), `--headless`, `--tick-interval S`,
`--csv FILE`.

Live HTTP API (read-only snapshots, same origin, 127.0.0.1 only):

```sh
./tribes --serve 8080 --people 1000 --days 0     # --days 0 = run until quit
curl -s localhost:8080/api/state
```

`--serve PORT` starts a one-thread blocking HTTP server beside the simulation
(`--web DIR` sets the static folder, default `web/`; `--days 0` means no day
limit). The console still works; with no console (stdin closed) the run
continues until the simulation ends. Endpoints (JSON, UTF-8, no CORS headers):

| endpoint | returns |
|---|---|
| `GET /api/state` | day, season, seconds per day, paused flag, population, births, deaths, money supply, Gini, tribes |
| `GET /api/people?since=<day>` | living people as `{fields:[...], rows:[[...]]}`; `since` keeps people born on or after that day |
| `GET /api/person/<id>` | full detail, family (with names), last 60 history lines, today's 24 slots with a one-line reason each (`today`: name, English, Arabic, zone, zone English, zone Arabic, reason), current `activity`/`zone`/`hour`, goods and animals owned (`goods`) |
| `GET /api/schedules?day=<d>` | every living person's 24 hourly slots with map positions for one day (about 280 KB for 3,000 people, built once per day and cached; `/api/people` carries no positions) |
| `GET /api/dash?since=<day>&car=<seq>&ev=<seq>` | rolling history for the dashboards: one row per day for the last 365 days (population, money split, Gini, hungry/thirsty/sick, births, deaths, wells, caravans, every item's average price, shop stock and daily use, herd totals per animal and per tribe, births and slaughters), the caravan visit log and the event log. Only rows newer than the given day and sequence numbers are returned. Recorded once per day under the world lock, served without it (`src/dashboard.cpp`) |
| `GET /api/market`, `/api/herds`, `/api/caravans`, `/api/map` | shop prices per good, herd counts per tribe, caravan states, settlements and their zones |
| `POST /api/control` | `{"action":"pause"}`, `{"action":"resume"}`, `{"action":"speed","value":seconds}` |
| `GET /` and `/<file>` | static files from `web/` (`..` and escapes get 403/404) |

`activity` and `zone` per person are `null` until the activity layer (issue 4)
exists. Requests are checked for a loopback `Host` and a same-origin `Origin`.
`src/snapshot.cpp` copies the world under `World::lock` (about 0.8 ms for
3,000 people) and builds all JSON outside the lock.

Activity layer (`src/activity.cpp`): `schedule_for(world, person, day)` derives
24 hourly slots (zone, machine name, English and Arabic label) from role, age,
gender, season, needs, sickness, pregnancy and caravan days. It is read-only,
never touches the simulation RNG (variations hash person id, day and hour) and
leaves the state hash unchanged. Day 0 is a Sunday (Friday is `day % 7 == 5`);
needs and caravans apply only to the current day. `./tribes --schedule <id>`
prints one person's day; `./tribes --role-table` prints the role-by-activity
table.

Valley map (`src/map.cpp`): a read-only, deterministic 2000 x 1400 layout from
the seed (own hash generator, never the simulation RNG): wadi floor, rocky
hills, dunes, the suq with mosque, majlis, workshops and shops, one settlement
per tribe with a home slot per household, wells, fields, groves, pastures,
salt pans, gates, and the caravan road east to west. Counts match the
simulation, objects never overlap, and a newly dug well appears without
moving anything else. `position_of(world, map, person, zone)` gives a stable
spot (own home, own field, tribe well...). `GET /api/map` returns the layout
once (cached). `./tribes --print-map FILE.svg` draws it; see
`docs/valley-map.svg` (1,000 people, seed 42).

Viewer (`web/index.html`, `style.css`, `viewer.js`, no dependencies): open
`http://127.0.0.1:PORT/` while `--serve` runs. A 2D canvas draws the valley
map and every person as a dot coloured by role group (ring = women and girls,
small dot = child, tiny dots = herd animals). People move between the hourly
stops of their schedule; the sky darkens at night. Controls: pause, 1x / 2x /
5x / 10x (24 / 12 / 4.8 / 2.4 seconds per day), tribe, role and activity
filters, labels and animals toggles, legend, mouse-wheel zoom and drag pan.
The viewer polls `/api/state` every 0.5 s, `/api/people` every 2 s and
`/api/schedules` once per day. `?hour=H` freezes the clock at hour H (used
for the screenshots in `docs/viewer-*.png`). Click a person (or use the search box, by name or id) to open the side panel: header, what they do now and why (English and Arabic, right-to-left), need bars, money and goods, a 24-hour strip with the current hour marked, family links that switch the panel, the last 60 history lines and a Follow button (`?select=ID&follow=1` opens it from the URL). Tested in headless Chrome only;
Firefox was not available.

Dashboards (button "Dashboards", top-left of the map): Economy (money supply and where it sits: pouches, Bayt al-Mal, shop tills, caravan purses, Majlis and mint; Gini; hungry, thirsty and sick; births and deaths per week; the header line uses the same wording and numbers as the console `world` command), Market (price, percent of base price, 7-day trend arrow, shop stock and days of stock left at the last 7 days' use; under 7 days is highlighted; click an item for its 90-day price chart), Herds (per animal and per tribe, born and slaughtered in 30 days, click for the head-count chart), Caravans (the log of visits, and the caravan in the valley is drawn walking the road with a label) and Alerts (also a ticker above the map: caravans, wells, Eid, weddings, notable deaths, famine risk when over 5% of people went hungry, Jumu'ah). The server keeps the history, so a page reload gets the last 365 days back. Charts redraw once per simulated day, tables update cell by cell.

Console commands while running: `person <id>`, `family <id>`,
`status`, `history <id>`, `schedule <id> [day]`, `activity [hour]`, `world`, `top`, `economy`, `tribes`, `market`,
`trade <good>`, `chain <good>`, `pause`, `resume`, `speed <s>`, `quit`.

## Running the viewer

```sh
g++ -O2 -std=c++17 -pthread src/*.cpp -o tribes
./tribes --serve 8080 --people 3000 --days 0 --tick-interval 24 < /dev/null
# then open http://127.0.0.1:8080/ in a browser
```

`--tick-interval 24` is one simulated day per 24 real seconds (the viewer's 1x);
the speed buttons change it live. `< /dev/null` is for running without a
terminal; with a terminal you can leave it out and use the console commands.
If port 8080 is taken, use another port in both places. The page is a 2D
map of the valley with every person as a dot, a person side panel (click a
dot), and dashboards. See the viewer, activity layer and dashboards notes in
the API section above. CI (`.github/workflows/ci.yml` (kept as `docs/ci.yml` until a token with the workflow scope can add it)) builds, runs
`tests/run_tests`, starts a 300-person server and runs `tests/smoke_api.sh`
on every push and pull request.

## Architecture

```
                 +--------------------------- tribes (one process) ----------------------------+
                 |                                                                              |
  simulation     |  World::step() once per day  --->  World (people, shops, herds, caravans)    |
  thread         |        |  holds World::lock while stepping                                   |
                 |        +--> Dashboard::record(): one row per day (own mutex, 365 days)      |
                 |                                                                              |
  HTTP thread    |  http_server: GET /api/*  ---> take_snapshot / take_person / json_schedules  |
                 |        |       (copy under World::lock, build JSON outside it)               |
                 |        +--> /api/dash is served from the Dashboard copy, no World lock       |
                 |        +--> GET /, /<file>: static files from web/                           |
                 +------------------------------------------------------------------------------+
                                         |   JSON over HTTP, same origin, 127.0.0.1 only
                                         v
  browser (web/):  viewer.js  = map, people dots, clock, person panel, search
                   dashboards.js = economy, market, herds, caravans, alerts, ticker
  read-only: the viewer never changes the simulation except pause/resume/speed
  through POST /api/control.
```

Layers inside the simulation, bottom to top: `world`, `economy`, `herds`,
`caravan`, `institutions` and `demographics` (the model and its audited
books); `activity` (what each person does each hour, read-only); `map`
(where things are, read-only); `snapshot`, `schedules_api`, `dashboard` and
`http_server` (the viewer's read side).

## How a day is built (the activity layer)

`schedule_for(world, person, day)` in `src/activity.cpp` returns 24 hourly
slots for one person. It never touches the simulation's random numbers
(variations come from a hash of person id, day and hour), so it cannot
change the simulation. The steps, in order:

1. Children (under 16): sleep at night, play, eat at fixed hours, school
   from 8 to 12 on weekdays for ages 7 to 15, housework in the late
   afternoon. Prayer starts at the prayer age.
2. Adults start from the role's work block (a 24-letter mask in
   `activity.cpp`: sleep, rest, main work, second work). Elders sit in the
   majlis or visit neighbours in the morning and late afternoon. Unknown
   roles work as day laborers.
3. On a caravan day, roles with a caravan job (trade, guard, caravan work)
   switch to it.
4. Pregnancy late in term moves heavy work to housework. In summer, outdoor
   work from 11 to 14 becomes a rest in the heat. On Friday teachers rest.
5. Prayers: Fajr, Dhuhr, Asr, Maghrib and Isha are inserted (men at the
   mosque, women at home); on Friday the noon hours become Jumu'ah at the
   mosque.
6. Sickness: everything except sleep, prayer and meals becomes rest at home.
7. Needs (only for the current day): thirst under 30 adds a trip to the
   well, hunger under 30 adds a meal at midday.

Each slot has a machine name, English and Arabic labels, and a zone. The map
module turns a zone into a position for that person (own home, own field,
tribe well, the suq, the mosque). `/api/schedules` packs everyone's 24 slots
and stops once per day; `/api/person/<id>` adds a one-line reason for each
slot (for example "thirsty (thirst 10 of 100): going to the well").
`./tribes --schedule <id>` prints one person's day on the console.

## How it is organized

| file | what it owns |
|---|---|
| `src/main.cpp` | flags, headless loop, CSV export |
| `src/world.cpp` | founding the valley, the one-day tick, startup balance checks, money/stock audits |
| `src/economy.cpp` | wells, groves, fields, production chains, shopping, shop day-end (prices, spoilage, wages) |
| `src/demographics.cpp` | aging, marriage (mahr, the wife joins her husband's tribe), pregnancy (270 days), births, nursing (no conception while a mother nurses), infant/child and maternal mortality at the historical rates, deaths, fara'id inheritance, coming of age at 16, retirement at 60, orphan care, elder care |
| `src/herds.cpp` | grazing, breeding with real gestation, milking, shearing |
| `src/institutions.cpp` | the Majlis (council payroll, suq tolls), the Bayt al-Mal (ushr, sadaqah, wadiah deposits, qard hassan loans, deyn - never any riba), the sheikh's-tent almsgiving |
| `src/caravan.cpp` | the caravans: the only source of goods the valley cannot make |
| `src/money.cpp` | `transfer()` - the ONLY way money ever moves |
| `src/stock.cpp` | `move_stock()` plus named `stock_create()`/`stock_destroy()` - the ONLY ways goods ever move or change |
| `src/report.cpp`, `src/console.cpp` | reports and the interactive console |
| `src/map.cpp` | the 2D valley layout and `position_of()` |
| `src/activity.cpp` | the hourly schedule of every person, derived read-only |
| `src/snapshot.cpp`, `src/http_server.cpp` | lock-short world snapshot + JSON, and the local HTTP server |
| `data/resources.csv` | every good: price, floor/ceiling, starting stock and its distribution, consumption rates, caravan flows, and a one-line reason for the number |
| `data/recipes.csv` | every production chain stage |
| `data/roles.csv` | every trade: sector, payer, wage, founding share |

Design rules enforced by the code, not by convention: every payment goes
through `transfer()`; the dirham supply is fixed and checked every day
(`total == initial + minted`); every physical thing is a counted stock with
exactly one owner, changed only by `move_stock()` or a named create/destroy
process, and reconciled every day
(`initial + imported + produced - exported - consumed - spoiled - used ==
sum of all holdings`); herds grow only through breeding; there is no raid
code at all - wealth moves by trade, work, gifts, inheritance and charity.

## Key parameters and why

| parameter | value | why |
|---|---|---|
| water | 3.5 L/adult/day, wells yield 7000 L/day each | desert reality; the startup check requires 60 days of cover |
| bread | 2 loaves/adult/day; 100 kg wheat -> 75 kg flour -> ~52 loaves per 10 kg flour | the staple chain; milling 2 runs/day, baking 3 runs/day per worker |
| dates | 0.30 kg/adult/day, 75 kg/palm/year, 3600 palms per 1000 people | the grove harvest (days 200-290) is the valley's anchor crop |
| wheat | 55,000 kg start per 1000 people, 12,000 kg per caravan | small local fields (900 kg/ha, 30 ha) plus the caravans carry the bulk - see the grain note below |
| suq toll | 6% of each sale to the Majlis | funds the public payroll (well keepers, guards, the muezzin...) |
| ushr | 5% of producer sales to the Bayt al-Mal | funds sadaqah, orphan stipends and qard hassan |
| sadaqah | 5 dirhams/day when a purse falls under 8, +1 for orphans and elders | the floor under the poorest |
| wadiah | deposits only from purses over 250, capped at 150/person, up to 80/day on withdrawal | a safe-keeping trust, returned on demand; deposits are never spent |
| qard hassan | interest-free loan when hunger presses; repaid when the purse recovers | no riba anywhere in the sim |
| prices | drift up to 10% (local) / 18% (imports) per day toward scarcity, clamped to the CSV floor/ceiling | supply and demand with no runaway spirals |
| mahr | paid from the groom's pouch at marriage | a real transfer, part of the wealth flow |
| elder care | work capacity fades 100% -> 25% between 55 and 75; children, then neighbors, support the old | craftsmen do not vanish at retirement - they slow down and are cared for |
| fertility | a married woman bears a child, nurses him (~13 months, no conception - lactational amenorrhea), then conceives again: one child every ~2-2.5 years at the fast end, 5-8 over a lifetime | the historical rhythm of pre-modern societies (sources below) |
| infant and child mortality | ~18% of newborns in year one, ~6%/yr ages 1-4, ~1.6%/yr ages 5-14 - about half of all children before fifteen, on top of famine losses | the pre-industrial record (sources below) |
| maternal mortality | 0.7% of births | 500-1000 per 100,000 births in the 19th-century record (sources below) |
| nursing babies | children under two live on their mother's milk: no shopping, no famine death while she is fed; her hunger dries the milk | infants were breastfed; they did not buy bread in the suq |

## Demographic sources

The birth/death regime is calibrated to the documented pre-industrial
pattern - high births, high deaths, near-zero net growth over the long run:

- Total fertility 5-8 children per woman (the US in 1800 averaged ~7 per
  white woman; Russia peaked near 7 in the mid-1920s) and birth/death rates
  often exceeding 40 per 1000 per year in pre-industrial societies:
  <https://en.wikipedia.org/wiki/Demographic_transition>
- For most of human history about 1 in 2 newborns died before 15 (Volk &
  Atkinson's historical-civilization data; UN IGME):
  <https://ourworldindata.org/child-mortality>
- Maternal mortality 500-1,000 per 100,000 births in the 19th century
  (Loudon, *Death in Childbirth*):
  <https://ourworldindata.org/maternal-mortality>
- Life expectancy at birth under 40 everywhere in 1800 (Riley 2005):
  <https://ourworldindata.org/life-expectancy>
- Lactational amenorrhea - intensive nursing suppresses conception, up to
  about two years postpartum (!Kung and Gainj studies):
  <https://en.wikipedia.org/wiki/Lactational_amenorrhea>

## Numbers that changed during testing, and why

These started from the Python reference's values and were adjusted against
measured demand until the startup balance checks passed and a society
persisted. All values live in `data/*.csv` with a reason on every row.

- `pottery` start 500 -> 200 and `silk` start 40 -> 15: the "caravans are
  truly needed for imports" check failed - the founding stock alone covered
  far more than 60 days of demand, so the caravans were decorative.
- `wheat` start 40,000 -> 55,000 kg and import 9,000 -> 12,000 kg/caravan:
  measured bread-chain demand at scale needed a deeper buffer between
  caravan arrivals.
- `dates` start 130,000 kg and `palm` 2,400 -> 3,600: the day-200 harvest
  window has to carry the valley for a full year.
- milling capacity 2.0, baking capacity 3.0 runs/worker/day: a miller and a
  baker per the roles table must feed their share of the valley.
- `caravan purse` 20,000 -> 30,000 (x scale): tills ran dry mid-season, so
  caravans could not actually buy the valley's surplus.

The startup checks themselves were also fixed during testing: chain inputs
(wheat for mills, flour for bakeries) were previously divided by a zero
"eat rate" and printed absurd cover times; demand is now propagated
through the recipe graph to a fixed point (at 300 people: water 61 days,
wheat 118 days, flour 143 days of cover).

## Design decisions from the user's steering

Two explicit decisions by the user shape this build, and they override the
original spec text where the two disagree:

1. **Realism over stability.** The spec's "Gini 0.35-0.55 after two years"
   target was dropped on the user's instruction: he wants the wealth spread
   to emerge on its own, the way the desert really graded people between
   poverty and riches. The Gini is still measured and reported (in the
   daily line, the `economy` report and the test suite) - it is just not a
   target the sim steers toward. Measured values: 0.59-0.72 after two years
   (seeds 1-3), higher mid-run.
2. **Hunger waves and debt spirals are realistic.** The user asked that
   these NOT be engineered away: lean seasons and debt gaps belong to the
   setting. The remaining bar he set is that the sim runs its full duration
   and a society persists in a typical seed - natural shrinking and
   struggle are expected outcomes, and a seed that collapses entirely is a
   reported outcome, not a defect. The conservation checks stay strict:
   they are about integrity (no money or goods created from nothing), not
   about stability.

## A note on grain (spec section 4.3)

The spec says goods the valley cannot make arrive only by caravan, and also
describes local wheat fields. This build reads the two together: small
irrigated fields (30 ha per 1000 people, ~900 kg/ha) give a modest local
harvest, and the caravans carry the bulk of the wheat. `wheat` is therefore
`local=1` with a large `import_per_caravan`. The chain command shows both
sources feeding the mills.

## Known limits

- The valley is harsh. At 300 people a lean wave still cuts deep in year
  one (the suq's bread chain runs at the edge at that scale), and the
  community that comes out of it is small; births now roughly double the
  old build's, and more of the valley survives. This is the accepted
  realism stance above, not a bug.
- All tribes share one price-discovery mechanism per market; there is no
  bargaining between individuals.
- Herds are tracked as aggregate head counts per owner per species (with
  real gestation), not as individual animals.
- The `wadiah` cap (150/person) is a deliberate simplification so the Bayt
  cannot become a fractional-reserve bank.
- Caravans are abstracted to "a company arrives every 10-20 days with cargo
  and a purse"; individual travelers and routes are not modeled.
- The viewer is a read-only 2D view. It does not edit the world, has no
  accounts, and listens only on 127.0.0.1 (no CORS, loopback Host and
  same-origin checks).
- People are drawn walking in straight lines between their hourly stops;
  there is no path-finding, and positions inside a settlement are
  deterministic spots, not simulated movement.
- Schedules are derived from role, age, season and needs, not stored in the
  simulation; a person's slots for past days are recomputed from today's
  state, and needs only affect the current day.
- The dashboards keep a rolling 365-day history in the server's memory
  only; it is lost when the process stops. Births and slaughters of animals
  come from the stock ledger and are zero in the first months because
  breeding takes long.
- The 2D viewer was tested in Chrome only. At 3,000 people it holds 30 fps
  at 10x speed on the test machine; much older machines may be slower.
- Deaths from hunger and thirst lag the shortage by design (hunger ->
  sickness -> death), so every lean wave echoes for about a month.

## Measured results (this exact code, this machine)

Machine: x86-64 Linux, g++ 11.4.0, `-O2 -std=c++17 -pthread`.

**Startup balance checks** (every run, before day 1): 19/19 PASS at 300
people - 60-day cover for water/dates/wheat/flour/salt, bread-chain cover,
milk and meat production, caravans truly needed for all 8 imported goods,
wells sustainable, no household starts at zero, richest <= 15x median.

**Test suite** (`tests/run_tests.cpp`): 22 checks, all PASS in ~2 s:
money supply constant to the fils across 3 seeds x 2 sizes; the day-end
stock formula reconciles for every item over 60 days; `move_stock` and
`transfer()` refuse negative and overdrawn amounts and change nothing;
animal counts never go negative and livestock is created only by `birth`
(the stock books keep a per-process audit trail to prove it); prices rise
when stock is cut (2.00 -> 2.19) and fall when it is doubled (2.00 -> 1.80);
no need ever leaves [0,100]; family links are symmetric among the living;
no same-gender marriage occurs; no money flow, stock process or event ever
mentions raiding; non-local goods are never produced locally; every price
stays within its floor/ceiling; determinism by state hash.

**Two-year runs** (300 people, 730 days, seeds 1-3), before -> after the
fertility work:

| seed | alive (of 300) | births | deaths |
|---|---|---|---|
| 1 | 84 -> 113 | 20 -> 37 | 236 -> 224 |
| 2 | 106 -> 88 | 31 -> 54 | 225 -> 266 |
| 3 | 89 -> 111 | 27 -> 38 | 238 -> 227 |

Money supply constant to the fils on every run, as before.

**One year at full scale** (3,000 people, seed 1), before -> after: day
365 population 2,929 -> 2,835 of 3,000; births 140 -> 283 against deaths
211 -> 448 (the added deaths are the historical infant, child and maternal
mortality the old build lacked, plus the lean wave); money supply constant
every day.

**Determinism**: two headless runs of 200 days at seed 7 produce
byte-identical output; a different seed produces a different history.
