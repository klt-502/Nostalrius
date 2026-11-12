# NPC purchase and loot classification audit

Historical audit: the current collector/courier architecture, batch policy and
qualified-witness lifetime are documented in [BROKER_LOGISTICS.md](BROKER_LOGISTICS.md).
The older single-actor trip/floor proposals below are retained as audit history.


Inspected `zedas-easy-mode` at `328bf7b`. This report precedes catalog/policy changes.

The extractor scans all **337 NPC files**, recursively expands `.ndb` includes, and finds **2598** single-unit item/price/topic offers. **762** reach a matching `Count(Type)>=Amount -> Delete(Type), CreateMoney` confirmation. These are evidence candidates, not automatically approved offers. It distinguishes purchases from `DeleteMoney, Create(Type)` sales to players. Bulk-only/unusual/gated rules are not inferred into unit prices. All ammunition references were also searched, including bundles and quest hand-ins.

The original purchase-evidence audit reviewed **58 identities** (33 added to the Sam baseline).
Current broker gameplay approval is **52 non-food identities**: meat 3577, salmon
3579, ham 3582, cherry 3590, bread 3600 and cheese 3607 are now excluded. Willie
is removed from the automated route. Their valid NPC purchase evidence below is
retained as historical evidence, not current broker approval. See
[BROKER_PICKUP_POLICY.md](BROKER_PICKUP_POLICY.md). Buyer selection uses a fixed reviewed route: retain Sam prices, then Shanar, Kroox, Timur, Uzgod, Al Dee, Willard, Willie and Zaidal for uncovered identities. It is never a maximum-price search. For example plate shield stays at Shanar 31 GP rather than maximizing to another merchant. Each entry below is an exact literal unit offer with Topic 2, an unconditional greeting route and a matching item-deletion/money-creation confirmation. NPC file fingerprints will bind the default catalog to this review.

## Reviewed ordinary-town purchase evidence (food now excluded)

| ID | Exact name | Buyer | Town | GP | Offer source |
| --- | --- | --- | --- | ---: | --- |
| 3044 | elephant tusk | Zaidal | Port Hope | 100 | `data/npc/zaidal.npc:58` |
| 3264 | sword | Sam | Thais | 25 | `data/npc/sam.npc:121` |
| 3265 | two handed sword | Sam | Thais | 450 | `data/npc/sam.npc:108` |
| 3266 | battle axe | Sam | Thais | 80 | `data/npc/sam.npc:109` |
| 3267 | dagger | Sam | Thais | 2 | `data/npc/sam.npc:110` |
| 3268 | hand axe | Sam | Thais | 4 | `data/npc/sam.npc:111` |
| 3269 | halberd | Sam | Thais | 400 | `data/npc/sam.npc:112` |
| 3270 | club | Shanar | Ab'Dendriel | 1 | `data/npc/shanar.npc:113` |
| 3272 | rapier | Sam | Thais | 5 | `data/npc/sam.npc:113` |
| 3273 | sabre | Sam | Thais | 12 | `data/npc/sam.npc:114` |
| 3274 | axe | Sam | Thais | 7 | `data/npc/sam.npc:115` |
| 3275 | double axe | Uzgod | Kazordoon | 260 | `data/npc/uzgod.npc:76` |
| 3276 | hatchet | Timur | Fibula | 25 | `data/npc/timur.npc:75` |
| 3277 | spear | Sam | Thais | 3 | `data/npc/sam.npc:116` |
| 3282 | morning star | Sam | Thais | 90 | `data/npc/sam.npc:117` |
| 3283 | carlin sword | Uzgod | Kazordoon | 118 | `data/npc/uzgod.npc:65` |
| 3285 | longsword | Shanar | Ab'Dendriel | 51 | `data/npc/shanar.npc:111` |
| 3286 | mace | Sam | Thais | 30 | `data/npc/sam.npc:118` |
| 3294 | short sword | Sam | Thais | 10 | `data/npc/sam.npc:119` |
| 3305 | battle hammer | Sam | Thais | 120 | `data/npc/sam.npc:120` |
| 3311 | clerical mace | Willard | Edron | 170 | `data/npc/willard.npc:116` |
| 3317 | barbarian axe | Willard | Edron | 185 | `data/npc/willard.npc:117` |
| 3349 | crossbow | Timur | Fibula | 160 | `data/npc/timur.npc:72` |
| 3350 | bow | Timur | Fibula | 130 | `data/npc/timur.npc:71` |
| 3351 | steel helmet | Sam | Thais | 190 | `data/npc/sam.npc:130` |
| 3352 | chain helmet | Sam | Thais | 17 | `data/npc/sam.npc:129` |
| 3353 | iron helmet | Shanar | Ab'Dendriel | 145 | `data/npc/shanar.npc:63` |
| 3354 | brass helmet | Shanar | Ab'Dendriel | 30 | `data/npc/shanar.npc:61` |
| 3355 | leather helmet | Sam | Thais | 4 | `data/npc/sam.npc:128` |
| 3357 | plate armor | Sam | Thais | 400 | `data/npc/sam.npc:126` |
| 3358 | chain armor | Sam | Thais | 70 | `data/npc/sam.npc:124` |
| 3359 | brass armor | Sam | Thais | 150 | `data/npc/sam.npc:125` |
| 3361 | leather armor | Sam | Thais | 12 | `data/npc/sam.npc:123` |
| 3367 | viking helmet | Shanar | Ab'Dendriel | 66 | `data/npc/shanar.npc:62` |
| 3372 | brass legs | Shanar | Ab'Dendriel | 49 | `data/npc/shanar.npc:68` |
| 3376 | studded helmet | Al Dee | Rookgaard | 20 | `data/npc/aldee.npc:124` |
| 3379 | doublet | Al Dee | Rookgaard | 3 | `data/npc/aldee.npc:118` |
| 3409 | steel shield | Sam | Thais | 80 | `data/npc/sam.npc:133` |
| 3410 | plate shield | Shanar | Ab'Dendriel | 31 | `data/npc/shanar.npc:98` |
| 3411 | brass shield | Shanar | Ab'Dendriel | 16 | `data/npc/shanar.npc:97` |
| 3412 | wooden shield | Sam | Thais | 5 | `data/npc/sam.npc:131` |
| 3413 | battle shield | Sam | Thais | 95 | `data/npc/sam.npc:132` |
| 3425 | dwarven shield | Kroox | Kazordoon | 100 | `data/npc/kroox.npc:89` |
| 3426 | studded shield | Al Dee | Rookgaard | 16 | `data/npc/aldee.npc:126` |
| 3430 | copper shield | Al Dee | Rookgaard | 50 | `data/npc/aldee.npc:129` |
| 3431 | viking shield | Willard | Edron | 85 | `data/npc/willard.npc:151` |
| 3552 | leather boots | Al Dee | Rookgaard | 2 | `data/npc/aldee.npc:130` |
| 3557 | plate legs | Shanar | Ab'Dendriel | 115 | `data/npc/shanar.npc:69` |
| 3558 | chain legs | Sam | Thais | 25 | `data/npc/sam.npc:127` |
| 3559 | leather legs | Shanar | Ab'Dendriel | 1 | `data/npc/shanar.npc:66` |
| 3561 | jacket | Shanar | Ab'Dendriel | 1 | `data/npc/shanar.npc:51` |
| 3562 | coat | Shanar | Ab'Dendriel | 1 | `data/npc/shanar.npc:50` |
| 3577 | meat | Willie | Rookgaard | 2 | `data/npc/willie.npc:61` |
| 3579 | salmon | Willie | Rookgaard | 2 | `data/npc/willie.npc:63` |
| 3582 | ham | Willie | Rookgaard | 4 | `data/npc/willie.npc:62` |
| 3590 | cherry | Willie | Rookgaard | 1 | `data/npc/willie.npc:65` |
| 3600 | bread | Willie | Rookgaard | 1 | `data/npc/willie.npc:59` |
| 3607 | cheese | Willie | Rookgaard | 2 | `data/npc/willie.npc:60` |

Full literal offers, confirmation rules, access and merchant classification are in `NPC_BUY_AUDIT.json`. The complete extracted single-unit evidence is reproducible with `python3 tests/audit/scan_npc_offers.py --csv /tmp/npc-offers.csv`; it is not used to generate runtime prices automatically. Item names above come from actual item definitions and the existing validated spill policy.

## Investigated and withheld offers

| Identity | Finding / decision |
| --- | --- |
| 3376 studded helmet | Al Dee Topic 2 buys for 20 GP; approved. The player recollection is contradicted by the actual source. |
| 3559 leather legs | Shanar Topic 2 buys for 1 GP; approved. |
| 3378 studded armor | H.L. 18 GP is male-only at the outlaw camp. Lee'Delle 25 GP is premium-gated. No unconditional ordinary buyer proved; excluded pending a separately reviewed access model. |
| 3362 studded legs | H.L. 15 GP, male-only outlaw buyer; excluded. |
| 3375 soldier helmet | H.L. 16 GP only; excluded. |
| 3446 bolts / 3447 arrows | Ordinary NPC rules sell ammunition to the player; bundle prices are not buyback offers. Liane's 12-arrow hand-in is quest-gated. No unconditional ordinary purchase proved; remain player-only spill. |
| 3287 throwing stars / 3298 throwing knives | H.L. conditional purchase; ordinary shop sale-to-player rules do not qualify. |
| 3578/3580 fish | Willie explicitly refuses ordinary fish; do not infer a price from salmon. Safe food may still spill for the player. |
| 3003 rope, 3293 sickle, 3304 crowbar, 2906 lantern, 3453/3456/3457/3463/3481/3483 tools | Some ordinary merchants have real purchase offers. Not approved for broker movement: use/transform/decay/tool semantics need a separate movement-safety review. They are not described as unsellable. |

## Specialty/high-value player-only items

All purchase identities of Alesar, Nah'bob, Haroun and Yaman are excluded from the broker, even when a low ordinary-town fallback exists. Their greetings require Djinn faction/quest value 278. This prevents liquidation of knight armor/legs, warrior helmets, guardian/dragon shields, fire swords and similar valuable loot. No Rashid-like best-price market is created. Eremo's 50,000-GP amulet trade, Rachel's vocation-gated trade, the Prisoner, H.L., Hardek's out-of-town forestaller trade, premium-gated Lee'Delle/Norma/Billy and quest hand-ins are not approved buyer routes.

Brengus is an ordinary town trader, but his additional jungle/exotic equipment (3345–3348, 3443–3445, 3403–3408, 3556, 3560) stays player-interest pending explicit tier review; a legitimate offer alone does not authorize automatic liquidation of rare-looking equipment. Golden armor 3360 and devil helmet 3356 are also conservatively player-interest. Jewels/pearls and decorative collectibles stay player-interest despite jeweler offers. These are deliberate review decisions, not name/value heuristics used by the engine.

Excluded Djinn purchase identities: `3045, 3046, 3048, 3049, 3050, 3051, 3052, 3053, 3054, 3056, 3060, 3061, 3062, 3065, 3066, 3067, 3069, 3070, 3071, 3072, 3073, 3074, 3075, 3077, 3078, 3079, 3081, 3082, 3083, 3084, 3085, 3091, 3092, 3093, 3097, 3098, 3271, 3279, 3280, 3281, 3284, 3297, 3299, 3301, 3302, 3307, 3313, 3318, 3320, 3322, 3324, 3369, 3370, 3371, 3373, 3380, 3381, 3382, 3383, 3384, 3385, 3391, 3392, 3415, 3416, 3418, 3419, 3428, 3429, 3432, 3434, 3439, 3567, 3574`. Some charged/transforming identities cannot spill at all; player-only does not override physical safety.

## Safe decorative / quest spill review

| IDs | Evidence / permitted state |
| --- | --- |
| 2991 doll, 2993 teddy bear (existing), 2994 model ship, 3001 bear doll, 3002 voodoo doll | Plain `Take` identities, no decay/transform or movement handler; player-interest only. No inferred collectible sale values. |
| 3231 enchanted gemmed lamp | Item definition is plain `Take`; scripts 71/72 consume it only after explicit player use with required storage. Spill never invokes them or moves it into player inventory. |
| 3232 spyreport | Ratamari creates it; Fa'hradin requires the player to possess it and talk before deleting it and advancing quest 281. Ground movement advances nothing. |
| 3233 tear of daraman | Action 70 creates it; Alesar's quest 287 hand-in requires explicit dialogue and carried count. No automatic hand-in/sale. |
| 3234 cookbook | Maryza sells it; Bo'ques consumes it only in quest dialogue. Plain `Take`, not a readable/use-triggered container. |
| 3243 sleeping gemmed lamp | Created by explicit scripts 71/72 at fixed quest positions; no identity-bound movement/use handler. Only a genuinely generated, witnessed instance could spill. Existing fixed-map/script-created or later player-inserted instances never gain provenance. |

Safe candidates may retain inert description/text/date/writer metadata unchanged. **No nonzero Action ID, Movement ID, chest-quest/key/charge/decay/owner state is approved by this audit.** They remain protected, including all Easy Mode marked items. This engine has Movement ID and quest attributes, not a separate general Unique ID field. No new marker or loot/drop-table entry is introduced. Quest fixtures demonstrate safe generated instances; they do not add these objects to monsters.

Still protected: 3235–3242 pharaoh pieces (Expire/decay state), 3244 Sam's old backpack (special container, including its subtree), keys, rune items, charged/transforming items, all arbitrary marked or movement-triggered items, quest chests, and unknown/custom identities. Resident tile/position movement handlers and the ten-thing tile limit remain mandatory checks.

## Corpse junk

3112 rotten meat, 3115 bone, 3116 big bone, 3120 moldy cheese, 3122 dirty cape and 3130 twigs leave the visual spill list; 3492 worms remain excluded for fishing. They stay inside the actual generated corpse/container, without changing loot tables, rates or consumption. Skull 3114 and hydra egg 4839 remain player-interest collectibles; this is an explicit reviewed distinction.

## Floor transition feasibility — audit only

A simple presentation pause is feasible without stair/rope/hole pathfinding. A future owner movement hook can despawn on floor change, clear only the session approach target, and arm a session-bound 1.5–2-second settling timer. Revalidate owner GUID/session, health, stable floor, native safe adjacent placement and hunting/travel state before reappearing. Restart the timer if the owner moves floors again. Do not spawn during an outbound/returning sale trip; do not collect while absent or across floors. Already durable uncertain claims must reconcile under existing escrow rules, rather than being silently cancelled by a presentation transition. Escrow/trip/bank state must remain unchanged.

Implement separately with original-client visibility tests and logout/death/teleport cancellation tests. No floor-transition behavior is changed in this pass.

Runtime source validation also pins all included `.ndb` dependencies. Among the
nine selected buyers, Zaidal includes `gen-t-furniture-jungle-s.ndb` (Topic 81);
its currently disjoint furniture rules cannot silently change after review.
Native regression checks changed includes and a valid narrowed Sam-only catalog.
