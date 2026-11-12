# Reviewed spill classes

The authoritative version-2 policy is `data/zedas/lootspill.xml`; `NPC_BUY_AUDIT.md` records source-level classification and the separate NPC purchase audit. Classification never grants generated provenance, movement safety or broker price by itself.

| Class | Count | Exact identities |
| --- | ---: | --- |
| broker-fodder | 58 | 3044, 3264, 3265, 3266, 3267, 3268, 3269, 3270, 3272, 3273, 3274, 3275, 3276, 3277, 3282, 3283, 3285, 3286, 3294, 3305, 3311, 3317, 3349, 3350, 3351, 3352, 3353, 3354, 3355, 3357, 3358, 3359, 3361, 3367, 3372, 3376, 3379, 3409, 3410, 3411, 3412, 3413, 3425, 3426, 3430, 3431, 3552, 3557, 3558, 3559, 3561, 3562, 3577, 3579, 3582, 3590, 3600, 3607 |
| player-interest | 189 | 1781, 2991, 2992, 2993, 2994, 3001, 3002, 3004, 3006, 3007, 3008, 3010, 3012, 3016, 3017, 3018, 3019, 3022, 3025, 3026, 3027, 3028, 3029, 3030, 3032, 3033, 3034, 3036, 3037, 3038, 3039, 3041, 3055, 3063, 3065, 3066, 3067, 3068, 3069, 3070, 3071, 3072, 3073, 3079, 3114, 3231, 3232, 3233, 3234, 3243, 3271, 3279, 3280, 3281, 3287, 3289, 3290, 3291, 3292, 3293, 3297, 3298, 3299, 3300, 3301, 3302, 3306, 3307, 3313, 3316, 3318, 3320, 3322, 3324, 3327, 3328, 3332, 3333, 3335, 3336, 3337, 3338, 3339, 3345, 3346, 3347, 3348, 3356, 3360, 3362, 3364, 3366, 3369, 3370, 3371, 3373, 3374, 3375, 3377, 3378, 3381, 3382, 3383, 3384, 3385, 3386, 3391, 3392, 3393, 3394, 3403, 3404, 3405, 3406, 3407, 3408, 3414, 3415, 3416, 3418, 3419, 3420, 3428, 3429, 3432, 3434, 3435, 3436, 3437, 3439, 3440, 3441, 3443, 3444, 3445, 3446, 3447, 3448, 3449, 3450, 3509, 3551, 3553, 3554, 3556, 3560, 3563, 3565, 3566, 3567, 3568, 3572, 3573, 3574, 3578, 3580, 3581, 3583, 3584, 3585, 3586, 3587, 3588, 3589, 3591, 3592, 3593, 3594, 3595, 3596, 3597, 3598, 3599, 3601, 3602, 3606, 3658, 3659, 3723, 3724, 3725, 3726, 3727, 3728, 3729, 3730, 3731, 3732, 4839 |
| corpse-junk | 7 | 3112, 3115, 3116, 3120, 3122, 3130, 3492 |

Only broker fodder has a possible broker transfer path, and only when its independent reviewed catalog also supplies a price. Player-interest items can never enter escrow. Corpse junk is never exposed. Unknown/custom identities fail closed. Neither this policy nor its classification changes loot generation, drop rates, XP, skills or monster statistics.

## State and handlers

The default is still no instance attributes, charges, decay or transforms. Only player-interest identities 3231, 3232, 3233, 3234 and 3243 admit inert DESCRIPTION/TEXT/DATE/WRITER state. The exact same instance is moved unchanged; no split/merge/recreation or quest hand-in occurs. No nonzero Action ID, Movement ID, key/chest quest state, owner, custom stat, duration or charge exception is approved. Special containers including 3244 keep their entire subtree protected.

The existing fingerprinted stock weapon and food use handlers remain the only narrow use exceptions. They do not dispatch from ground movement; changed scripts, incompatible roles, unknown handlers, Lua equipment events and movement handlers remain unsafe. Native equip/de-equip events can be safe because this transfer does not equip anything. Destination position/resident-item handlers remain checked, including when a textual quest item already occupies the tile.

Currency remains with auto-gold/Gold Bag or manual pickup. The generated snapshot is never rescanned for player insertions. Manual mutation invalidates provenance. Ground, creatures and items share the ten-total-things budget, including textual quest fixtures. Overflow stays inside. Classic/master-off and historical manual stacking remain unchanged.

## Regression tests and client retest

`engine_spill_hunting` checks every one of the 254 declared identities against real item state and handlers (247 eligible, 7 excluded). `engine_loot_classes` checks generated nested fodder, a Djinn identity, decoration, textual quest metadata, decaying/marked/movement-ID protection, opaque quest containers, all junk identities, later insertions, unsafe script reload and ten-thing overflow. Existing real-monster death, currency, stack, provenance and movement tests remain.

On the original client, kill monsters producing ordinary equipment and confirm exposure without opening bags. Check visible high-value loot remains under player control, the added safe decorative/quest identities preserve appearance/text, and pharaoh decay items/marked items/special backpacks stay inside. Rotten meat/bones/moldy cheese/dirty cape/twigs/worms stay inside; ordinary fishing and food use are unchanged. No new drop entries were added, so safe quest/decorative fixtures require a disposable generated-loot test setup, not a player inserting an item afterward.
