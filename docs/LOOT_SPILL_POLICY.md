# Expanded hunting-loot spill policy

The source audit revises the 46-ID policy at `f83a1fd` to **241 explicit ID/name
pairs**, adding **195 identities**. This is an identity list, not a global item
category exemption, NPC valuation table or sale feature. The authoritative names
are in `data/zedas/lootspill.xml`; custom/unknown identities are not inferred.
No monster loot tables, probabilities, item definitions or client IDs change.

| Category | Total IDs | Exact eligible IDs |
| --- | ---: | --- |
| Ordinary melee weapons | 54 | 3264, 3265, 3266, 3267, 3268, 3269, 3270, 3271, 3272, 3273, 3274, 3275, 3276, 3279, 3280, 3281, 3282, 3283, 3285, 3286, 3289, 3290, 3291, 3292, 3293, 3294, 3297, 3299, 3300, 3301, 3302, 3305, 3306, 3307, 3311, 3313, 3316, 3318, 3320, 3322, 3324, 3327, 3328, 3332, 3333, 3335, 3336, 3337, 3338, 3339, 3345, 3346, 3347, 3348 |
| Ordinary ranged weapons, rods/wands and ammunition | 20 | 1781, 2992, 3065, 3066, 3067, 3069, 3070, 3071, 3072, 3073, 3277, 3287, 3298, 3349, 3350, 3446, 3447, 3448, 3449, 3450 |
| Normal armor, helmets, legs, boots and shields | 87 | 3079, 3351, 3352, 3353, 3354, 3355, 3356, 3357, 3358, 3359, 3360, 3361, 3362, 3364, 3366, 3367, 3369, 3370, 3371, 3372, 3373, 3374, 3375, 3376, 3377, 3378, 3381, 3382, 3383, 3384, 3385, 3386, 3391, 3392, 3393, 3394, 3403, 3404, 3405, 3406, 3407, 3408, 3409, 3410, 3411, 3412, 3413, 3414, 3415, 3416, 3418, 3419, 3420, 3425, 3426, 3428, 3429, 3430, 3432, 3434, 3435, 3436, 3437, 3439, 3440, 3441, 3443, 3444, 3445, 3551, 3552, 3553, 3554, 3556, 3557, 3558, 3559, 3560, 3562, 3563, 3565, 3566, 3567, 3568, 3572, 3573, 3574 |
| Gems and pearls | 13 | 3026, 3027, 3028, 3029, 3030, 3032, 3033, 3034, 3036, 3037, 3038, 3039, 3041 |
| Passive rings, amulets and jewelry with no charges, decay or equip transformations | 14 | 3004, 3006, 3007, 3008, 3010, 3012, 3016, 3017, 3018, 3019, 3022, 3025, 3055, 3063 |
| Ordinary creature products and inert hunting junk | 9 | 3044, 3112, 3114, 3115, 3116, 3120, 3122, 3130, 4839 |
| Other reviewed inert loot | 6 | 2991, 2993, 3068, 3509, 3658, 3659 |
| Normal food | 38 | 3577, 3578, 3579, 3580, 3581, 3582, 3583, 3584, 3585, 3586, 3587, 3588, 3589, 3590, 3591, 3592, 3593, 3594, 3595, 3596, 3597, 3598, 3599, 3600, 3601, 3602, 3606, 3607, 3723, 3724, 3725, 3726, 3727, 3728, 3729, 3730, 3731, 3732 |

## Why ordinary weapons and food are safe to expose

`Actions::useItem` / `useItemEx` dispatch explicit item use. The stock weapon
`onUse` delegates to `doDestroyItem(target)` in `data/actions/lib/actions.lua`;
the stock food `onUse` feeds the player and removes one item. None of these use
functions are called by `Container::removeThing`, `Tile::addThing`, or their
post-movement notifications. `MultiUse` describes item use, not an on-drop event.

`Tile::postAddNotification` / `postRemoveNotification` instead dispatch through
`MoveEvents::onItemMove`. Its add/remove/position/resident-item handlers remain
unsafe. Native `onEquipItem` / `onDeEquipItem` run through player equipment
changes, not corpse-to-ground movement; those exact native handlers are safe.
Lua equipment handlers and all other registered movement types remain unsafe.
This also permits ordinary boots of haste without equipping them or changing
player speed. Passive jewelry has no charge, duration or transform mutation.

Actions record a movement classification only after successful script loading.
Both the exact registered filename and normalized source fingerprint must match
this reviewed source. SHA-1 is used as a change detector through the existing
engine helper, not as a new account/security mechanism. CRLF is normalized to LF.

| Audited file | LF-normalized SHA-1 |
| --- | --- |
| `data/actions/scripts/misc/weapons.lua` | `089ef11a54fd3c68e65fcfc0b5f4592e1f9fc05b` |
| `data/actions/lib/actions.lua` | `d765451a45fe6be35ca7805c343b28b0d3173c60` |
| `data/actions/scripts/misc/food.lua` | `b7cb3c821a69571ef48009314ea6ceabde0692a9` |

An edited stock file, unrelated handler on an approved item, or changed action
registration fails closed after reload. The spill predicate additionally checks
the item's role: a weapon handler on armor is not an ordinary weapon exemption.
A spilled ordinary weapon/food also remains safe as a resident item so it does
not stop later approved loot on the same tile. Quest/custom/unsafe residents
continue to block exposure. No item use, feeding, destructive action or equipment
callback is invoked to accomplish spill.

## State and quest exclusions

All instance attributes still disqualify candidates, including Action IDs,
quest/key metadata, custom names/text/stats, charges and duration. Items with
catalog charges, decay or equip/use transformations also stay inside. Examples:

- All runes, including blank runes, and Easy Mode marked consumables.
- Keys and quest tools, Sam's backpack 3244 and other special/custom containers.
- Quest identities such as dirty fur 3105, ancient rune 3235 and carrot of doom 3250.
- Charged amulets/might rings, ice rapier 3284, active rings and inactive rings
  whose equip operation transforms them. No partial-state exception is made.
- Unreviewed tools, instruments, writable books, custom actions/movement scripts
  and unknown/unique identities. A high price or ordinary-looking sprite does
  not establish approval.

Generic tradable commodities that can also be requested by NPCs are not unique
quest objects: ordinary elephant tusks 3044 and hydra eggs 4839, for example,
have normal monster drops and explicit NPC sale dialogs (`zaidal.npc` and
`explorer.ndb`). They remain ordinary generated hunting loot unless marked or
otherwise unsafe. This feature only exposes them; it never performs a sale.

Only ordinary generated shells **2853–2872** (bags, backpacks, basket, present
and color variants) are traversed. Marked shells and every other container are
opaque to spill. They are not moved or emptied by a broad container rule.

The existing post-loot registry, manual-mutation invalidation, exact instance
movement, corpse lifetime checks and ten-total-things limit are unchanged.
Currency stays in the corpse tree for the existing auto-gold/Gold Bag path or
manual pickup. Overflow and unsafe items remain inside; nothing is recreated,
converted, sold or deleted for lack of space. No new gameplay switch or Action
ID is added. Classic/master-off behavior is preserved.

## Automated coverage

`engine_spill_hunting` validates all 241 entries against actual engine item
state, stock weapon/food actions and native equipment registrations. Mixed nested
fixtures cover sword 3264, leather helmet 3355, elephant tusks 3044, meat 3577,
crystal ring 3007, arrows 3447 and boots of haste 3079, with actual currency in
the same bag and protected contents beside them. It also checks weapon-filled
tile overflow/retry, a real death's automatic weapon bag, later insertions,
special container protection and refusal of unrelated/edited stock handlers.
The existing provenance, merge, decay, trade, two-player and value tests remain.

## Local gameplay retest

Use ordinary player accounts for capacity/pickup checks; GM flags can affect
normal mechanics. Keep the original 7.72 executable. Enable `easyModeEnabled`
and `easyModeLootSpill`; optionally enable auto-gold and Gold Bag preference.
Restart after updating the binary and `data/zedas/lootspill.xml`.

1. Kill ordinary orcs, dwarfs, skeletons or minotaurs until they generate a sword,
   axe, mace or other listed weapon in the engine's generated loot bag. Confirm
   it appears above the corpse and is draggable/pickable without opening a bag.
2. Repeat for leather/chain/plate armor, helmets, shields and boots. Where a listed
   item is rare, use a controlled isolated encounter; do not change production
   drop rates. Boots of haste must spill without applying speed/equip effects.
3. Kill elephants or hydras for tusks/eggs, plus monsters producing bones or worms.
   Check the existing stack count after exposure. Test arrows, bolts, throwing
   weapons and gems/pearls; no merge/split or denomination conversion is added.
4. Test meat, fish, ham or another listed normal food, including a generated
   nested bag in a controlled encounter. Food must expose with the same count,
   remain edible through normal use, and never feed the player automatically.
5. Test passive crystal/wedding/gold rings or necklaces. Inspect and pick up the
   existing item. Charged amulets, ice rapiers and transforming rings stay inside.
6. With auto-gold and Gold Bag enabled, kill a mixed-loot monster. Ordinary gear,
   products and food spill; coins go only through the existing inventory path.
   Fill capacity/bags: undelivered coins stay inside. Verify total denominations.
7. Check mixed safe/unsafe loot: marked items, keys, runes, quest/special items,
   custom-script items and contents of a special/marked container stay inside.
   Items manually inserted or merged after death must never gain spill eligibility.
8. Crowd a death tile. Ground, splash, corpse, creatures and existing items all
   count toward ten. Excess remains inside. Pick up one exposed weapon and wait:
   only available space is filled, without duplication or inaccessible overflow.
9. Repeat manual/two-player pickup, corpse movement/decay and save/relog checks
   from the Wave 2 checklist. Restart with spill disabled and then master disabled:
   ordinary historical corpse and item-use behavior must remain intact.

Original-client rendering, clickability, food/manual weapon use and live
save/relog still require these checks. Native/CI tests do not replace them.

Worms (3492) deliberately remain inside corpses/generated bags for classic fishing.
No fishing action or consumption behavior is changed.
