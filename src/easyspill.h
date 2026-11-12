#ifndef ZEDAS_EASYSPILL_H
#define ZEDAS_EASYSPILL_H

class Item;
class Container;
class Tile;

namespace EasyMode {
enum class LootClass { Protected, BrokerFodder, PlayerInterest, CorpseJunk };
LootClass lootClass(const Item& item);
// Internal helpers for the shared generated-corpse registry, not manual moves.
bool isSpillContainer(const Item& item);
bool isSpillItem(const Item& item);
bool brokerPickupTile(const Tile &tile);
bool takeBrokerItem(Item &item, Container &escrow);
bool spillItem(Container& source, Tile& tile, Item& item);
}

#endif
