# Gold Bag catalog and Action ID audit

Audited against accepted Wave 1 commit `ed3c7c671711f82e97c1fadd619ffb7b4929bebd`
on `zedas-easy-mode`, on 2026-11-09. This audit does not modify the item catalog,
client assets, map, database, or historical container behavior.

## Selected existing container

Reserve **item 2856 + Action ID 65005** for the optional Easy Mode Gold Bag.
Create one with the existing GM command:

```text
/i 2856,1,65005
```

`data/items/items.srv` defines item 2856 as `a present`, with `Container,Take`,
capacity **5 slots**, and weight 600 (6 oz). It is an existing historical
container, rather than a new client item ID. The present shape provides a
distinct container choice alongside the default bag/backpack. Its ordinary
name and appearance remain those of a present; marking it does not create a
new sprite or distinguish it visually from an unmarked present.

The catalog alternatives inspected were:

| Existing ID | Catalog name | Slots | Reason |
| --- | --- | --- | --- |
| 2853 | a bag | 8 | Ordinary default bag; less distinctive |
| 2854 | a backpack | 20 | Ordinary default backpack; less distinctive |
| 2855 | a basket | 5 | Distinct existing container; reasonable alternative |
| **2856** | **a present** | **5** | **Selected distinct existing container** |
| 2857–2864 | a bag | 8 | Catalog does not identify individual sprite colors |
| 2865–2872 | a backpack | 20 | Catalog does not identify individual sprite colors |
| 3244 | an old and used backpack | 20 | Quest-associated Sam backpack; rejected |
| 3253 | a backpack of holding | None | Lacks the Container flag; rejected |

Ordinary NPC gear data sells 2856, including `data/npc/gen-t-gear-s.ndb`,
`halif.npc`, `perod.npc`, and `shiantis.npc`. The map contains nine item-node
instances of 2856. No item-specific action registers 2856; the existing
`misc/present.lua` action belongs to quest present 3218 instead.

`NetworkMessage::addItem` in `src/networkmessage.cpp` sends the existing
catalog item ID directly unless the type has a disguise. Item 2856 has no
disguise. No executable, item definition, client DAT, sprite, or protocol
extension is needed. Actual sprite appearance and container-window behavior
still require the original Tibia 7.72 client; client assets are not in this
checkout.

## Action ID collision verification

Repository source and data were checked before reserving 65005. This includes
sparse-checkout-excluded movements, NPC scripts, creature scripts, global
events, raids, world XML, and `nostalrius.sql`, read directly from the accepted
Git tree. No existing text reference to 65005 was found. Across all 211 XML
files, the 83 registered Action IDs are 1–83, and there are no `fromaid`/`toaid`
ranges that can include 65005.

The binary world map was also audited, rather than relying on text search:

| Map evidence | Result |
| --- | --- |
| Git path | `data/world/map.otbm` |
| Git blob | `701d9dc7bd08d541842ddd8337f4d1e091304dfe` |
| Length | 74,354,465 bytes |
| SHA-256 | `4afa6b85543df78989a123172aea9d2c57b4be1c0b3c0e4860233305157cdb22` |
| Parsed OTBM item nodes | 754,944 |
| Parsed Action ID attributes | 97 |
| Distinct map Action IDs | 83, all between 1 and 83 |
| Map Action IDs 65001–65005 | Zero occurrences |
| Unknown/truncated item attributes | Zero |

The scan followed `src/fileloader.cpp` node/escape encoding and the attribute
widths in `Item::readAttr` / `src/iomap.h`. It balanced all nodes, unescaped
each item's property bytes, skipped length-prefixed strings, and parsed Action
ID attribute 4 as a little-endian uint16. A second raw-byte check found zero
occurrences of `04 ED FD FD`, the OTBM-escaped encoding of Action ID 65005.

This establishes no collision in the accepted repository. An operator's live
player-item database, local custom map, and local scripts are outside this
repository audit and may contain additional manually assigned Action IDs.

## Recognition, selection, and persistence

Recognize the exact **2856/65005** pair only, with an actual `Container`.
An ordinary present, another container carrying Action ID 65005, and a present
carrying another Action ID must not receive Gold Bag preference.

When multiple exact marked bags are carried, prefer the first in the existing
`EasyMode::getCarriedItems` breadth-first traversal: ascending player equipment
slots, followed by each container's stored child order. That helper excludes
the active-trade item and its subtree. The first match gets dedicated priority;
other marked containers may still be used by the normal fallback destination
rules. Moving a bag can change its traversal priority. Selection requires no
new persistent identifier, ownership record, or balance.

Prefer merging ordinary matching coin stacks directly inside the selected bag,
then its free slots, using existing `Game::internalMoveItem` behavior. If full,
absent, capacity-limited, or otherwise unavailable, preserve the accepted
auto-gold fallback. Do not create coins or convert denominations. A five-slot
bag holds only five direct item stacks, so fallback is expected during longer
hunts. Unrelated manually stored contents retain their normal behavior and
consume normal slots/capacity.

`Item::serializeAttr` writes a moveable item's nonzero Action ID as
`ATTR_ACTION_ID`/uint16; `Item::readAttr` restores it. This existing serialization
supports the marked present in ordinary player/container persistence without a
schema change. Both Gold Bag preference and automatic collection must remain
gated by their respective Easy Mode configuration settings.

## Required acceptance checks

Automated/runtime checks should cover exact-pair recognition, nested-carried
selection, deterministic multiple bags, active trades, partial coin-stack
movement, full-bag fallback, capacity exhaustion, ordinary unmarked presents,
and disabled-feature fallback. Count total gold/platinum/crystal items before
and after repeated/competing collection; preserve value without denomination
conversion.

Original-client checks must confirm:

1. `/i 2856,1,65005` produces the existing present sprite and opens as a
   five-slot container; visually distinguish it from the player's regular bag.
2. With Easy Mode, auto-gold, and Gold Bag preference enabled, coins first
   fill/merge into this container even when it is inside nested carried bags.
3. Once full, ordinary destinations receive the remainder; undeliverable
   currency remains in the corpse. Manual coin handling stays historical.
4. Two marked presents follow the documented traversal order; active-trade
   bags are skipped, and two players cannot duplicate the same corpse coins.
5. Save/relog preserves Action ID 65005 and bag contents; transferring the bag
   transfers only the existing bag/contents, with no separate stored balance.
6. Disabling the master switch, auto-gold, or Gold Bag preference produces the
   corresponding historical/accepted fallback behavior.
