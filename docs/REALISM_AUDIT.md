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

## Decision and change: per-tribe council customs (PR for #2)
Owner decision (user, via the coordinating agent, 26 Nov): take the decision from historical reality; each tribe may have a different system.

Full texts read this time:
- S4: Fletcher, "The Amarat, their Sheikh, and the Colonial State" (Warwick WRAP, full PDF): https://wrap.warwick.ac.uk/id/eprint/93322/1/WRAP-Amarat-Sheikh-Colonial-State-Fletcher.pdf . "In a culture in which generosity conferred power and prestige", a British visitor reported up to forty guests regularly dining in the sheikh's tent. A sheikh was expected to extend hospitality and mediate disputes. Khuwwa (protection payment to the sheikh) was real income.
- S5: Tapper, "The organization of nomadic communities in pastoral societies of the Middle East" (1979, full PDF): https://www.nomadsed.de/fileadmin/user_upload/redakteure/Dateien_Intern/Archiv_AG_1/Tapper_The_organization_of_nomadic_communities_in_pastoral_societies_of_the_Middle_East__1979_.pdf . Leadership differed by tribe: Murrah lineages roughly equal, no leader or council of elders at lineage level; Shammar subsections led informally by an old man as shaykh; Basseri camps led by unofficial elders but the tribe under a chief with appointed subchiefs.
- Shryock 2019 (Balga of Jordan, abstract only, see above) on hoarding by dominant families.

| Fact | Source | Simulation | Verdict | Action |
|---|---|---|---|---|
| Sheikhly generosity and open tents were a source of authority | S4 | Majlis never paid relief | (a) model gap for tribes of this type | Even-numbered tribes: their share of the treasury above 5000 dirhams pays 4 dirhams a day to each hungry member |
| Leadership and wealth concentration differed between tribes | S5, Shryock | One custom for all tribes | (a) | Odd-numbered tribes keep the old behaviour (retain surplus) |

Limits, stated plainly: the sources show both customs existed, not which of this simulation's tribes follows which. The even/odd split is a modelling choice. The treasury is still one shared account, so an open-tent tribe spends from the common purse in proportion to its population. Quantities (reserve 5000, relief 4 dirhams) are not sourced.

Result (diagnose, 1000 people, 360 days): seed 42 death rate 465 to 401 per 1000; seed 2 635 to 404; seed 3 178 to 352 (worse). Single runs are very noisy: any change moves the global RNG stream, so these do not show an effect. All are still about 10 times the 35 to 40 per 1000 of S1. This change does not fix the death rate and no claim is made that it does. The main cause (hungry penniless people dying of "illness") is still open.

## Children's education: kuttab / maktab (not a modern school)
Owner's request (user message, relayed by the coordinating agent, 26 Nov): research how children were taught then and build the code on it.

Sources read in full:
- S6: Encyclopaedia Iranica, "Education iii. The traditional elementary school": https://www.iranicaonline.org/articles/education-iii/ . Persia (maktab), pre-20th century. Children studied under one teacher (akhund/mulla/mu'allim); curriculum up to about age ten: alphabet, reading and writing, rudiments of arithmetic, above all the Quran and prayer books. No age grades or levels: the teacher called each child in turn, older pupils (khalifa) helped teach younger ones; pupils recited aloud. Hours "from early morning until sunset" for those who stayed, but maktabs for commoners "closed at noon to allow children to spend the rest of the day helping with the family trade or household chores". Pupils brought lunch and gave a share to the teacher; families gave a gift at milestones. Corporal punishment was common. Many teachers were semi-literate and taught recitation only; talented pupils went on to the madrasa. Maktabs served relatively more girls than boys with rudimentary education.
- S7: Re-Evaluating Early Memorization of the Quran in Medieval Muslim Cultures (MDPI Religions 13(2), 2022): https://www.mdpi.com/2077-1444/13/2/179 . In kuttabs the chief emphasis was learning the Quran by heart, with literacy and arithmetic secondary. Memorization at an early age was given priority across the sources; some parents also used it as a way to keep young children occupied.

| Fact | Source | Simulation before | Verdict | Action |
|---|---|---|---|---|
| Teaching starts early morning | S6 | study 8:00-12:00 | (b) small gap | study 6:00-12:00 except the 7:00 breakfast |
| Commoners' school closes at noon; afternoon is family work | S6 | afternoon play, chores 16-17 | (a) | afternoon chores 15:00-17:00 (family trade/household) |
| Girls attended too | S6 | all children study | matches | none |
| Quran memorization is the core | S7, S6 | one "study" activity | not modelled | no literacy or memorization state exists |
| Mixed ages, individual turns, older pupils teach younger | S6 | none | not modelled | not implemented |
| Fees in kind and gifts to teacher; teacher paid by families | S6 | Muallim paid by the Majlis (roles.csv) | (b) candidate | not changed, needs a source on the Arabian/Egyptian setting |

Limits, stated plainly: S6 describes Persia, S7 the medieval Islamic world in general. Neither describes a Najdi Bedouin camp, where formal schooling for nomad children was probably rare; that is unverified. The change is only a schedule change. Not implemented: literacy or memorization progress, pupil teachers, in-kind fees, play/hours variation by class.

## Food chain and livestock: what the sources say (docs only, no model change)
Owner's question (26 Nov, relayed): is the animal-to-meat-to-market chain complete, and where did food and money historically come from?

Source read in full:
- S8: "Arabian Traveller's Observations on Bedouin Food" (compiled from Doughty, Thomas, Thesiger and others): http://enhg.org/alain/phil/bedouin/bedouin.htm . Staples: dates, wheat/barley flour and bread, rice, samn (clarified butter), milk (camel preferred), laban/dried yoghurt, coffee. One meal a day, or none, was common; "creeping hunger" is recorded. Boiled mutton or camel calf with rice (mansaf) "would normally only be a festival or major family event dish". A beast was often slaughtered for a guest's first meal as a show of wealth and honour; hosting could deplete the host's flock. Camels "would rarely be slaughtered for meat": only surplus bull calves or injured or sick beasts. Goats and sheep were kept for milk, samn and meat. Game (gazelle, hunted with salukis and hawks) was irregular. Samn was a major commercial product (a caravan of 30 tonnes from Aneyza). Cattle were kept in oases but of poor quality.

| Fact | Source | Simulation (seed 42, 1000 people, 360 days) | Verdict | Action |
|---|---|---|---|---|
| Meat is a feast and hospitality food, not daily | S8 | Meat is 2.6% of spending (89k of the ledger); item note says "meat is a feast food" | matches | none |
| Camels rarely slaughtered; only surplus bull calves, injured or sick | S8 | `slaughter-camel` recipe: any camel the butcher can buy, capacity 0.2 runs a day, 180 kg meat | (b) candidate: the code does not limit camel slaughter to surplus males or sick animals | open; needs herd sex data check before changing |
| Samn (clarified butter) is a staple and a major trade good | S8 | no samn item; there is laban and cheese | (a) missing good | open: add as a dairy product after checking quantities |
| Rice a staple of festival meals | S8 | not in the item list | (b) minor | open |
| Hospitality slaughter for the first meal, costly to the host | S8 | none | not modelled | open |
| Chickens, eggs | none found | chickens exist as livestock only; no slaughter recipe, no eggs | unverified: the source does not mention poultry in the Bedouin diet | no change; not adding eggs without a source |

How food reaches people in the code today: farming (wheat) to miller to baker to shop; date groves to shops; milking to dairyman (cheese, laban); herds born and dying; the butcher buys sheep, goat and camel from herders and sells meat at the suq; hunters and falconers add meat; caravans import and export. Money historically: S8 describes a mixed economy of herd products (samn, animals) sold in towns, dates and grain bought with them, and caravan trade. Wages and price ratios remain unverified.

Prayer attendance by women: Women in the Mosque: Historical Perspectives on Segregation (AJISS, https://www.ajis.org/index.php/ajiss/article/download/504/2268/4998), read in full. Early traditions show women prayed in the Prophet's mosque, and other traditions say prayer at home is better for them; the sources are contested. Simulation: women pray at home. Verdict: contested, left unchanged.

## Food fixes implemented (source S8, see above)
Approved by the owner through the coordinating agent (26 Nov).
| Fact (S8) | Change | Check |
|---|---|---|
| Camels were rarely slaughtered, only surplus bulls, injured or sick | `slaughter-camel` capacity cut from 0.2 to 0.03 runs per butcher per day (about 11 camels a year per butcher) | Unsourced number. The herds do not track sex, so "surplus males only" and "injured or sick" are NOT modelled; the cut only makes camel slaughter rare. |
| Samn (clarified butter) is a staple and a major trade good, churned from goat or sheep milk | New item `samn` (30 dirhams/kg, 600 kg at start, 30 kg exported per caravan, eaten from the pantry or suq), and a Dairyman recipe `samn-making` (25 L milk to 1 kg) | The 25 L per kg ratio and the 30 dirham price are my assumptions, not from S8. Seed 42: samn is stocked in two shops by day 200 at a falling price (30 to 18). |
| Hospitality slaughter for guests | Not implemented | The sim has no guest-hosting mechanism for travellers; it would need one first. |
| Chickens, eggs, rice | Not changed | No source found. |
Seed 42 (1000 people, 360 days): death rate 329 per 1000, birth rate 99; meat share of spending 2.4% (was 2.6%). Single runs are noisy; still about 9 times the sourced 35 to 40 per 1000. No claim that this fixes mortality.

## Mortality: free treatment for the penniless sick
Sourced fact (S9, Encyclopaedia Iranica "Bimarestan", https://www.iranicaonline.org/articles/bimarestan-hospital/): medieval Islamic hospitals were paid for by endowments and bequests; physicians' pay, treatment, medicine and patients' food came from those funds.

Diagnosis (seed 42, before the change): 263 of 295 deaths were illness deaths. All 263 had under 3 dirhams in the pouch, 132 were hungry (under 30), mean 35 sick days. The hakim was only reachable for people with more than 15 dirhams, so a penniless sick person could never be treated.

### Assumptions (غير موثق) - not sourced, my reasoned estimates under the 5:09 AM instruction
| Assumption | Value |
|---|---|
| The endowment-funded treatment custom (S9, cities) also applied to a desert tribe's hakim | Bayt al-Mal pays the tribe's hakim 5 dirhams per penniless sick person |
| Limit per tribe | 15 patients a day, only while the Bayt has more than 200 dirhams of its own funds |
| Effect of a visit | same as a paid visit (+18 health, -2 sick days): also an assumption |

### Result (1000 people, 360 days, crude death rate per 1000)
| Seed | Before | After |
|---|---|---|
| 42 | 329 | 127 |
| 7 | 397 | 77 |
| 99 | 242 | 71 |
Single runs are noisy. Still above typical pre-modern rates; no tuning toward a target was done. Hunger (about half of illness deaths were hungry) and wages/prices are still open.
