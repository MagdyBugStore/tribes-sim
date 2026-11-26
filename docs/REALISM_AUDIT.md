# Realism audit (issue #2): work in progress

بلال

Status: step 1 done (the `--diagnose` report). Step 2 (sourced comparison) is partly done, below. Fixes, tests and the README section are NOT done yet. No model change is made.

Run: `./tribes --diagnose --people 1000 --seed 42 --days 360`. Full output: `docs/diagnose-seed42-1000.txt`. The report only counts; it changes nothing in the model. The sim year is 360 days.

## What the first year shows (seed 42, 1000 people)
| Item | Simulation value | Source for the real range | Verdict |
|---|---|---|---|
| Crude death rate | 465 per 1000 per year (391 deaths, pop 1000 to 681) | NOT YET VERIFIED. The issue gives about 25 to 45 as a hint | Looks far outside any normal year. Needs a source before being called an error |
| Crude birth rate | 86 per 1000 per year (72 births) | NOT YET VERIFIED (hint 30 to 50) | Above the hint. Cause not examined yet |
| Share of deaths under age 5 | 10% | NOT YET VERIFIED | Looks wrong: the issue expects infants and children to dominate; here working-age 15 to 44 is 41% |
| Death cause | 365 of 391 are "illness", 13 childhood illness, 12 old age, 1 childbirth | n/a | See mechanism below |
| Hunger | mean 122 hungry per day; poorest 4 deciles (pouch 0.0) about half hungry | n/a | Economic, see below |
| Wheat price | 1.14 on day 1, 3.4 by day 41 (about 3x) | n/a | Price spike, cause not examined yet |
| Money placement | pouches 125k on day 150 to 56k on day 360; Majlis 0.3k to 127k | n/a | Money drains from people to the Majlis; unexamined whether this is a modelling error |

## Mechanism found in code (read-only observation, no change)
- `src/demographics.cpp`: a sick person loses 0.6 health a day and, after 10 sick days, dies with 3% a day ("illness").
- `src/economy.cpp`: a person becomes sick when health < 35, and recovers only when health > 55. Health only rises (+2 a day) when hunger and thirst are both above 55, or by paying the hakim 5 dirhams (needs pouch > 15). A hungry, penniless person therefore cannot recover. So most "illness" deaths are probably malnutrition deaths labelled as illness, and the root cause is food access, not disease.
- `person.age()` divides by 365 while the sim year is 360 days, so ages run about 1.4% slow (minor, class a candidate).
- Flat 0.1% a day sickness onset gives about 30% of people sick per year, independent of age.

## Not done yet
Sourced comparison table (no source has been verified in this PR), classification (a/b/c) with fixes, the realism tests replacing the Gini test, the README "Realism and sources" section, seeds 2 and 3, and the first-90-days income per role (the model has no per-role income figure, so it needs a wage query).

## Sourced comparison (step 2, partial)
Sources I opened and read:
- S1: UN, Population Studies No. 49, "The World Population Situation in 1970" (UN New York 1971): https://www.un.org/development/desa/pd/sites/www.un.org.development.desa.pd/files/files/documents/2020/Jan/un_1970_the_world_population_situation_in_1970_0.pdf . Says pre-modern death rates below 30 per 1000 were probably rare and averaged 35 to 40 even without calamities; birth rates were rarely below 35 and 40 to 50 were typical. Table 3: less developed regions 1750-1800 birth 41, death 37.
- S2: Our World in Data, "Global Health" (child mortality section): https://ourworldindata.org/health-inequality . Pre-modern child mortality (before age 5) ranged between 30% and 50%.
- S3: Our World in Data, "The mortality of children over the long run": https://ourworldindata.org/child-mortality-in-the-past . About 40% of children died before 15 in Sweden 1750-1780, about 45% in France, about half in Bavaria.
Not verified: Bedouin diet and calories, wage and price ratios, caravan facts, wealth inequality and zakat redistribution (a search found papers on pastoral inequality, e.g. https://doi.org/10.1086/648561, but I did not read them, so nothing is cited from them).

| Fact | Source | Simulation value (seed 42, 1000 people, 360 days) | Verdict | Action |
|---|---|---|---|---|
| Death rate 35 to 40 per 1000 typical, rarely below 30 | S1 | 465 | (a) or (b): about 12 times above even a bad average year. Not a named catastrophe in the model | Investigate the cause (below), do not tune |
| Birth rate 35 to 50 per 1000 | S1 | 86 | (b) candidate: about twice the top of the range. Cause not examined | Investigate |
| Child mortality before 5 is 30 to 50%, before 15 about 40 to 50% | S2, S3 | Baseline hazard in `demographics.cpp` gives about 18% first year, 6% a year ages 1 to 4, 1.6% a year ages 5 to 14: cumulative about 45% before 15, in line with S3 | (c) for the hazard | Leave |
| Infants and children should dominate deaths | S3 (implied by the above) | Only 10% under 5 and 32% under 15; 41% are 15 to 44 | Mixed: the founding population has few infants (a founding age structure effect, unverified), and adults die of "illness" from hunger | Check the founding age pyramid |

## Current causal chain (numbers from the diagnose report, not final)
Wheat price rises from 1.14 to about 3.4 within 40 days, bread follows; the poorest four deciles hold 0.0 dirhams and about half of them are hungry; sickness starts when health falls below 35 and clears only when fed or after a paid hakim visit, so hungry penniless people die of "illness" (365 of 391 deaths). Meanwhile the Majlis balance grows from about 0.3k (day 150) to 127k (day 360) while pouches fall from 125k to 56k. Whether the Majlis accumulation or unpaid market-earning roles (Baker, Dairyman, SaltDigger, CamelDriver, Weaver all average about 1 dirham) is a modelling error still needs checking in the code.

## Finding: the Majlis accumulates money (open decision, no change made)
Code: `src/institutions.cpp`, `majlis_daily`. The comment says "The council does not hoard: a surplus goes back into the valley", but the only outflows are salaries, well digging (only while the valley's water yield is below 130% of need) and a 2 dirham Eid gift on two days a year. Tolls, the grove share and similar inflows have no matching outflow, so the Majlis holds about 127k of the 198k dirham supply by day 360 (pouches 56k) while the poorest deciles hold 0.0 and about half are hungry.
Evidence on whether this is realistic is mixed. Reading only the abstracts: Shryock (2019, Balga of Jordan, https://www.tandfonline.com/doi/full/10.1080/02757206.2019.1623793) says dominant shaykh families are often known for taking and storing wealth, so elite accumulation is documented. Other sources I saw (a modern study of tribal shaykh duties, https://spaef.org/file.php?id=625, and the Bedouin generosity literature, https://doi.org/10.4324/9781315823614-2) stress that the shaykh's tent is open to those in need. I did not read the full texts.
Classification: unresolved. Either (a) a model error because the code contradicts its own stated rule, or (c) plausible elite hoarding. Not tuned. Needs the owner's decision: should the Majlis redistribute its surplus to the hungry (hospitality or sadaqah style), and with what rule?

## Founding age structure and fertility (measured, not yet sourced)
Founding 1000 people (seed 42), age in 360-day years: 0: 20, 1-4: 73, 5-14: 178, 15-29: 215, 30-44: 251, 45-59: 186, 60-74: 72, 75+: 5. So 27% are under 15 and 26% are 45 or older. I have NOT verified a source for a pre-modern age pyramid, so no verdict. It is a plausible reason why under-5 deaths are only 10% of deaths: few infants exist at the start.
Births: 72 births in the year to 239 women aged 15 to 44, about 0.30 births per fertile woman per year. The crude birth rate of 86 per 1000 is partly because the population shrinks and 24% of it is fertile women. No source verified for per-woman fertility either.
Early-year check (first 60 days, crafter roles Baker, Weaver, Dairyman): mean pouch rises from about 10 to about 100 and nobody among them is hungry. So the near-zero pouches of those roles at day 360 come later in the year and are a result of the price and money-drain chain above, not a day-one margin error. Bakery margin at base prices is positive (inputs about 25 dirhams for 52 loaves at 0.6, about 31).
