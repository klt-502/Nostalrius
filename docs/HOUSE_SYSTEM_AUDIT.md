# House architecture and conservative exploration overlay audit

Audited from accepted `9b5eae2` before this implementation. Historical OTBM, world
houses/spawns XML, native ownership, save logic and item definitions are unchanged.
No live database/configuration was supplied: sample SQL ownership is not current
player ownership. No production location is approved.

## Authoritative identities and inventory

`config.lua.example` selects `mapName="map"`. `Game::loadMainMap` opens
`data/world/map.otbm`; its OTBM map-data property 13 names **houses.xml**, resolved
relative to the map directory by `IOMap`. Thus metadata is
**data/world/houses.xml**, not the new Easy manifest. If that property is absent,
`IOMap::loadHouses` falls back to `mapName + "-house.xml"` relative to the working
directory. The fallback is not used here.

`OTBM_HOUSETILE` (node 14) stores byte X/Y offsets from its tile-area node's uint16
base X/Y, uses the area's floor, then stores a uint32 house ID. `IOMap::loadMap`
creates a `HouseTile`, registers it with `Houses::addHouse(id)` and adds it to that
house's tile list. Ordinary OTBM tiles have no house ownership identity. XML
supplies name, entry, rent and town; its display size/guildhall attributes are not
proof of current tile membership or encounter safety. The loader uses actual OTBM
tile lists; even an entry coordinate is not an approved furnishing footprint.

Deterministic [HOUSE_MATRIX.json](HOUSE_MATRIX.json) and
[HOUSE_REVIEW_QUEUE.csv](HOUSE_REVIEW_QUEUE.csv) contain every actual house tile,
exact XML identities/entries and explicitly scoped sample-owner data:

| Inventory/classification | Count | Decision |
| --- | ---: | --- |
| XML houses / distinct OTBM house IDs | 861 / 861 | All player-ownable; excluded |
| Actual OTBM house tiles | 39,127 | All excluded, even if presently unowned |
| Currently owned in tracked sample SQL | 0 | Sample only; live count UNKNOWN |
| NPC Home references in repository `.npc` files | 337 | Public/NPC reference points, not inferred building footprints; commerce/quest review required |
| Source-verified safe candidate interiors | 0 | No automatic approval |
| Production pilot interiors | 0 | Empty approval manifest |
| Quest/special/scripted interiors | Not enumerable | Hold closed; no authoritative polygon/index |
| Other uncertain non-HouseTile buildings | Not enumerable | Hold closed; no fabricated building count |

The 861 ownable identities are classified rather than guessed safe from names,
coordinates, house IDs or zero sample owners. NPC references can overlap house
coordinates and are not added to the house total. The map has no trustworthy
residential/public/quest building-region inventory. Non-HouseTile adjacency,
appearance or lack of an obvious handler cannot prove ordinariness. In-game and
source encounter review must precede any production entry.

Regenerate/validate: `python3 tests/audit/house_matrix.py [--check]`. The script
reads the real escaped OTBM stream and metadata, detects duplicate XML/sample
identities, records exact tiles and hashes, and changes only audit artifacts.
The runtime additionally pins the original map's whole-file SHA-1. Audit SHA-256
is `4afa6b85543df78989a123172aea9d2c57b4be1c0b3c0e4860233305157cdb22`.

## Ownership, loading and saving

- `src/house.cpp`, `Houses::loadHousesXML`: XML metadata initializes houses with
  owner zero; it does not establish live ownership. `IOMapSerialize::loadHouseInfo`
  then restores `houses.owner`, paid/rent warnings and `house_lists` from SQL.
  Native `!buyhouse` can buy an unowned HouseTile; zero owner is not non-ownable.
- `src/map.cpp`, `Map::loadMap`: OTBM → native NPC load → spawn definitions → house
  XML → SQL house info → `IOMapSerialize::loadHouseItems`. After main-map loading,
  `GAME_STATE_INIT` places spawn/NPC actors, initializes raids and global startup
  events. `Game::start` runs the new overlay afterward, before serving gameplay.
  Existing objects or nearby actors therefore prevent an approved point from use.
- `IOMap::loadMap` explicitly **discards movable OTBM defaults on HouseTiles** in
  both property-item and child-item paths. Map-default furniture and SQL contents
  cannot be treated as interchangeable sources.
- Items carry a limited in-memory `loadedFromMap` flag used by cleaning. It is not
  a complete generated-loot/provenance identity and is not a persisted immutable
  origin marker. This overlay does not repurpose it or infer ownership from it.
- `src/iomapserialize.cpp`: `tile_store` contains house ID and native serialized
  tile position/item trees. Movable items are recreated; stationary items are
  matched to existing map objects by ID (door/bed type fallbacks), then attributes,
  nested contents and transformations are restored. Missing stationary map
  objects consume/discard their saved data through a dummy object, not reliable
  dynamic-fixture reconstruction.
- `saveHouseItems` saves **all registered houses**, including unowned ones. Per
  tile it saves movable items, doors, nonempty containers, writable items and beds;
  an inert immovable table is not serialized just because it exists. Nested trees
  use native `saveItem`/`loadContainer`. Unowned house contents therefore survive
  successful native saves/restarts; ownership is not a persistence gate.
- `Game::saveGameState` saves players, then `Map::save`; house metadata/access lists
  and `tile_store` have separate persistence operations. `saveHouseItems` replaces
  tile_store in an SQL transaction. Shutdown invokes the normal save path. This is
  not an atomic transaction covering every player's inventory plus the whole world.
- Ordinary non-HouseTile dynamic ground items are **not** included in that house
  saver. Creating a new world container there and assuming native house-style
  persistence would be wrong.

## Special items and ownership transfer

`HouseTile::updateHouse` registers house door IDs and beds when items are added.
Doors use house access lists; beds register house/sleeper state and paired sleep
behavior. Depots route to player depot storage. Quest chests, keys, readable or
writable objects, movement/action handlers, decay and transforms carry independent
semantics: item appearance does not make them safe decoration.

`House::setOwner` can transfer the previous owner's property to their town depot,
kick inhabitants, wake sleepers, clear access lists and reset paid/rent state.
`transferToDepot` moves pickupable objects, or immediate contents of stationary
containers, using actual native items. A newly generated pickupable fixture would
become transferable player property; a generated stationary container might save
contents but fail to restore without a map counterpart. Repeated generation on
house tiles risks map/SQL copies and ownership conflicts. Player-owned houses can
contain normal server-created objects, but safe overlay lifetime/removal is not
established merely by that fact. This pilot categorically rejects every HouseTile
and its immediate neighboring placement area. It changes no player property,
ownership/deed behavior or inventory.

## Implemented pilot architecture

See [HOUSE_FURNISHING.md](HOUSE_FURNISHING.md) for exact schema and client tests.
`data/zedas/houses.xml` contains five reusable **restricted profile vocabularies**
and **zero** approved interiors. `src/easyhouse.{h,cpp}` validates the complete
manifest before applying it. No fuzzy areas or species-style auto classification
exist. All locations require explicit review evidence, stable identity, absolute
anchor/placement positions, floor, houseId=0, exact bare ground and whole-map hash.
Malformed XML, stale map/item identity, unknown attributes/roles and unsupported
items fail closed. Existing objects are never moved or removed to make room.

Fixtures use reviewed native non-movable/non-pickupable types, not a new Action ID
or a global change to item behavior. Initial vocabulary: table forms 2322/2323/2324,
carpets 2572/2581, bones 4306; authentic source names/flags are validated at runtime.
Beds, chairs that rotate/destroy, functional containers and many conceptual clutter
roles remain unsupported until reviewed. Profiles are poor, working, merchant,
wealthy and abandoned residence; they are a foundation, not complete room designs.
Future inns/shops/ruins/memorial themes can reuse named roles and exact placements,
but receive no current area authorization or generation behavior.

## One-time loot and container boundary

The chosen model is **one-time world issuance, at most once**, not a reset timer.
Only loose bread 3600, cheese 3607 (one each) and at most five total ordinary GP
3031 per interior are initially supported. There is no high-tier equipment,
random rare gear, automatic sale, broker fund operation or reward schedule.

Additive explicit migration `003_house_loot.sql` creates only `zedas_house_loot`.
A stable `interior/role` primary key is committed **before** creating/placing a
reward. The stored identity includes map hash, profile, role, exact coordinates,
item/count. Existing keys are never reused, even after profile/map edits. No
UPDATE/DELETE/REPLACE/INSERT IGNORE or reset command is provided. Migration is not
automatically run on a live database.

The ledger uses its own bounded, no-reconnect MySQL session with autocommit enabled
and validates InnoDB and its primary key. The general engine Database automatically
retries lost connections and shares transactions; it is deliberately not used to
guess the result of a durable award reservation. No new connection is opened when
features are disabled, the manifest is empty or only fixtures are active.
Duplicate keys, connection/schema errors and uncertain commits issue no item;
errors disable further issuance for that session. One successful INSERT permits
one native physical item insertion, with automatic stacking disabled. No value
conversion or replacement copy exists.

**Tradeoff:** a crash after reservation but before placement can forfeit the tiny
reward. Uncollected public-ground rewards are not restored after restart; their
key stays retired forever. This favors no duplication over guaranteed delivery.
It is not a durable chest-storage or guaranteed-until-discovered quest-reward
system. Once picked up, normal player persistence applies. Restore from old DB
backups, deliberate key deletion/renaming or multi-world deployments sharing/resetting
state are operator actions outside the at-most-once guarantee; never purge this table
as routine maintenance.

**Mutable and nested container generation is rejected**, not partially persisted.
Native house persistence cannot safely be applied to arbitrary public interiors
without a larger transactional design. Thus no generated container can accept
player contents, be destructively reset, be replaced after mutation or be farmed
for shell value. Existing unrelated/player containers and nested items cause
placement rejection and remain untouched. The overlay never deletes any item.
Native ordinary-world restart behavior is unchanged. This narrow container limit
is intentional; safe mutable-container persistence is deferred, not claimed done.

## Broker interaction and future work

This code never calls monster loot generation, corpse registration, loot spill or
broker witness registration. Actor brokers cannot claim new fixtures or loose
house rewards without a witnessed generated-corpse identity. Subsequent manual
moves do not invent that identity. Native tests verify the absence of broker
eligibility. No broker code/catalog or corpse rules changed.

Future opportunities: deliberate encounter review of exact public interiors;
small room grammars with more reviewed inert 7.72 objects; transactional public
container storage; optional guaranteed discovery rewards; source-backed city/biome
themes; and a handcrafted memorial/training profile after separate review. No
procedural dungeon, market, simulated player or memorial location is implemented.
