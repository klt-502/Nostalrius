# Endless Magic Light Wand

The authentic repository item is **3046**, unlit Magic Light Wand. Native
`Actions::internalUseItem` handles `ChangeUse` with `Game::transformItem` and
`Game::startDecay`; no new spell or use script is added. Use toggles 3046 ↔ 3047.
Lit **3047** has brightness **8**, color **209**, and `TotalExpireTime=3000`
**seconds** (native loader converts to milliseconds). Unlit brightness is 0,
color 215. Native toggling preserves remaining duration through ExpireStop.
Ordinary lit wands expire with `ExpireTarget=0`: `Game::internalDecayItem` removes
the wand. Item definitions and ordinary behavior are unchanged.

Reserved **Action ID 65006** applies only to the exact authentic 3046/3047 pair.
It was checked against tracked scripts/source and serialized map Action-ID bytes
before reservation. Other items bearing that ID receive no exemption.

New startup Boolean `easyModeEndlessLightWand = false` defaults disabled in the
generic example. Both it and `easyModeEnabled` must be enabled. At depletion only,
the marked lit instance renews its authentic 3000-second duration through the
native decay queue. It stays lit, with unchanged intensity/color, on the **same
physical instance**, without consumption, replacement or depletion transform.
It may still be switched off/on normally. Native action timing is unchanged;
there is no independent cooldown. Disabling either gate restores normal decay,
even for marked wands. Duration/Action-ID persistence uses existing item storage.

`!light` in local chat, without parameters, grants one unlit marked wand when
`easyModeCommands` is also enabled. It is a convenience grant, not a light spell
or auto-use. Equip and use the wand as usual: native player light scans direct
inventory slots, not items inside bags. Exact marked lit/unlit wands anywhere in carried nested containers
(including trade inventory) suppress another grant. Ordinary wands never qualify.
Insufficient capacity/space fails with no ground fallback. GM creation:

```
/i 3046,1,65006
```

Normal NPCs buy ordinary Magic Light Wands (35 gp in `data/npc/haroun.npc`; its sale uses `Count(Type)` before `Delete(Type)`).
A free grant must not become a repeated sale source. While the light feature is
enabled, native NPC Count/Delete paths exclude only exact marked wands, and remove
ordinary wands explicitly. Both checks are necessary: counting only ordinary
items while deleting marked ones would leave a sale exploit. Ordinary wands keep
their NPC price; no broker catalog or logistics behavior changes.

Automated native tests cover nested duplicate suppression, grant idempotence,
exact pairing/gates, authentic duration/light values/native toggling, expiry on
the same marked instance, ordinary/disabled removal and NPC count/removal.

Original-client retest: enable the three flags, run `!light` twice, use the wand
on/off, move it into nested bags and relog; verify one authentic wand/light and
retained marking. Carry an ordinary wand alongside it and sell to an appropriate
NPC: only ordinary inventory is valued/removed. Verify capacity/full-inventory
failure, normal wand expiry and marked wand renewal after 3000 seconds (or use
an isolated server test to shorten remaining duration, without editing item
definitions). Disable the feature and verify historical use/expiry. Confirm
existing runes, mana, gold bag and broker remain as accepted.
