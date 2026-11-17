# Endless-item Look text on the original 7.72 client

Existing marked items now identify themselves through ordinary **Look** text.
No re-creation, name-attribute migration, database update or client change is
needed. Exact pairing and the existing master/relevant feature gates apply:

| Item / Action ID | Active Look name | Gate after `easyModeEnabled` |
| --- | --- | --- |
| 3160 / 65001 | an endless ultimate healing rune | `easyModeEndlessRunes` |
| 3191 / 65002 | an endless great fireball rune | `easyModeEndlessRunes` |
| 3200 / 65003 | an endless explosion rune | `easyModeEndlessRunes` |
| 2874 / 65004, mana subtype 10 | an endless mana fluid | `easyModeEndlessManaFluid` |
| 3046 or 3047 / 65006 | an endless magic light wand | `easyModeEndlessLightWand` |

Runes show `endless uses` instead of a misleading finite charge count. Normal
magic-level requirements, weight and existing special descriptions remain.
Ordinary items, incorrect pairings/subtypes and disabled features retain their
historical Look text. Item-type-only descriptions cannot inherit a mark.
Disabling a feature restores historical behavior too: a marked but inactive item
must not promise endless uses. No new configuration options or Action IDs exist.

`EasyMode::endlessDisplayName` centralizes the five names. `Item::getNameDescription`
and `Item::getDescription` render them dynamically. Underlying `getName()`, item
attributes, persistence, matching, physical instances and use machinery are
unchanged. Existing items receive the same presentation after reload/relogin.

## Wand timer finding and implementation

The apparent timer is server-generated text from `Item::getDescription`'s
`showDuration` branch. `ShowDetail` in `items.srv` enables this branch; it is not
a locally ticking countdown supplied by this protocol. `Game::playerLookAt`
sends the description as normal text. `NetworkMessage::addItem` sends item ID
and applicable stack/fluid subtype, with no item duration, custom name, Action ID
or badge field. No protocol extension is necessary for this fix.

Active marked wands now say **"that provides endless light while lit."** for
both lit and paused/unlit forms. They never say "brand-new", "minutes left" or
"less than a minute left" because of their internal duration. Normal wands keep
all that historical text. The actual 3000-second duration still counts down and
renews on the same marked lit instance using the previously accepted decay queue.
Native toggling, action timing, brightness 8/color 209, expiry renewal and NPC sale
protection are unchanged. We intentionally retain this tested implementation.

## Original-client limits and optional later work

This pass makes the items unambiguous **when looked at**. Their inventory/ground
sprites remain authentic. It does not introduce a hover tooltip, inventory name
label, icon overlay, timer bar or custom UI. A gold-star marker drawn directly on
an item sprite requires a separate client-asset phase (typically DAT/SPR plus
reviewed server item/sprite mapping). Rendering a per-instance overlay in the
executable would require a separate client-code phase plus server marker/protocol
design: Action IDs are not transmitted. Neither is implemented;
reusing an existing unrelated sprite would obscure authentic item identity.

## Exact local validation

1. Enable the master and relevant existing feature flags. As GM create:
   `/i 3160,2,65001`, `/i 3191,2,65002`, `/i 3200,2,65003`,
   `/i 2874,10,65004`, and `/i 3046,1,65006` (or use `!light`).
   The `/i` second field is subtype/charges for these native item types.
2. Look at all five in inventory, nested carried bags and on adjacent ground.
   Check the names above. Look at otherwise identical unmarked items too:
   ordinary names, charge counts and mana-fluid contents must remain historical.
3. Use each marked consumable and its ordinary counterpart. Verify existing
   effects, requirements/exhaustion and endless-versus-consumable behavior.
4. Equip and light the marked wand. Look again after several minutes, toggle it
   off/on, move it into a bag and relog. Expect the endless-light statement and
   authentic lit/unlit sprite, with no finite-energy text. The original wand still
   shows its native remaining duration. No light is granted merely by `!light`.
5. Across a full 3000-second lit cycle, verify marked light renews without a new
   item; an ordinary wand still expires. Repeated `!light` with an existing carried
   marked wand still grants nothing, and NPCs still cannot buy the free marked one.
6. Restart with master off, then with each relevant feature off. Look text and use
   behavior must revert to historical behavior for those inactive marked items.

Native tests cover all six item forms, exact/wrong pairing, wrong mana subtypes,
master/feature disabled parity, type-only descriptions, articles, rune requirements,
finite-charge suppression, two different wand duration values, attribute immutability,
inscription preservation and a serialization round trip. Existing use/expiry,
NPC sale and duplicate-grant engine regressions remain in the suite.
