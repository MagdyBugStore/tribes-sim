# Realism audit (issue #2): work in progress

بلال

Status: step 1 done (the `--diagnose` report). Steps 2 to 5 (sourced comparison, fixes, tests, README section) are NOT done yet. No model change is made in this PR.

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
