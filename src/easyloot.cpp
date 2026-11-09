#include "otpch.h"
#include "easymode.h"
#include "easyspill.h"
#include "configmanager.h"
#include "container.h"
#include "game.h"
#include "player.h"
#include "tile.h"

extern ConfigManager g_config;
extern Game g_game;

namespace {
bool enabled()
{
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
	       g_config.getBoolean(ConfigManager::EASY_MODE_AUTO_GOLD);
}

bool spillEnabled()
{
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
	       g_config.getBoolean(ConfigManager::EASY_MODE_LOOT_SPILL);
}

bool ordinaryCurrency(Item& item)
{
	// No Action ID, movement/quest metadata, custom text or other item attributes.
	return item.isStackable() && item.getWorth() != 0 && !item.hasAttribute(static_cast<itemAttrTypes>(0xFFFFFFFFu));
}

struct CorpseEntry {
	Container* corpse;
	Cylinder* originalParent;
	uint16_t originalId;
	int64_t registeredAt;
	std::vector<Item*> coins;
	std::vector<Item*> spill;
	std::unordered_set<Item*> touched;
	bool invalidated = false;
	explicit CorpseEntry(Container& container) : corpse(&container), originalParent(container.getParent()),
		originalId(container.getID()), registeredAt(OTSYS_TIME()) {
		corpse->incrementReferenceCounter();
	}
	~CorpseEntry() {
		for (Item* coin : coins) coin->decrementReferenceCounter();
		for (Item* item : spill) item->decrementReferenceCounter();
		corpse->decrementReferenceCounter();
	}
	bool valid(int64_t now) const {
		return !invalidated && corpse->getParent() == originalParent && originalParent && originalParent->getTile() == originalParent &&
		       !corpse->isRemoved() && corpse->getID() == originalId && now - registeredAt < EasyMode::CORPSE_REGISTRY_TTL_MS;
	}
};

// Dispatcher-only. References protect queued handles through moves, decay and cleanup.
std::list<std::unique_ptr<CorpseEntry>> corpses;
Item* activeGoldSource = nullptr;

bool inside(Item& item, const Container& corpse)
{
	for (const Cylinder* parent = item.getParent(); parent; parent = parent->getParent()) {
		if (parent == &corpse) return true;
	}
	return false;
}

bool inAnyTrade(Item& item)
{
	for (const auto& online : g_game.getPlayers()) {
		Item* offered = online.second->getTradeItem();
		if (offered == &item || (offered && offered->getContainer() && offered->getContainer()->isHoldingItem(&item))) return true;
	}
	return false;
}

bool ordinarySpillCorpse(const Container& corpse)
{
	const uint32_t normal = ITEM_ATTRIBUTE_DURATION | ITEM_ATTRIBUTE_DECAYSTATE | ITEM_ATTRIBUTE_CORPSEOWNER;
	return !corpse.isChestQuest() && !corpse.hasAttribute(static_cast<itemAttrTypes>(~normal));
}

bool safeSpillAncestors(Item& item, const Container& corpse)
{
	if (!ordinarySpillCorpse(corpse)) return false;
	for (Cylinder* parent = item.getParent(); parent; parent = parent->getParent()) {
		if (parent == &corpse) return true;
		Item* ancestor = parent->getItem();
		if (!ancestor || !EasyMode::isSpillContainer(*ancestor)) return false;
	}
	return false;
}

void spillEntry(CorpseEntry& entry)
{
	Tile* tile = entry.corpse->getTile();
	for (auto it = entry.spill.begin(); it != entry.spill.end(); ) {
		Item* item = *it;
		if (!entry.touched.count(item) && inside(*item, *entry.corpse) && safeSpillAncestors(*item, *entry.corpse) &&
		    EasyMode::isSpillItem(*item) && !inAnyTrade(*item)) {
			Item* sourceItem = item->getParent()->getItem();
			Container* source = sourceItem ? sourceItem->getContainer() : nullptr;
			if (source && tile) EasyMode::spillItem(*source, *tile, *item);
		}
		if (entry.touched.count(item) || !inside(*item, *entry.corpse) || !safeSpillAncestors(*item, *entry.corpse) || !EasyMode::isSpillItem(*item)) {
			entry.touched.erase(item);
			item->decrementReferenceCounter();
			it = entry.spill.erase(it);
		} else ++it;
	}
}

void prune()
{
	const int64_t now = OTSYS_TIME();
	for (auto it = corpses.begin(); it != corpses.end(); ) {
		if (!(*it)->valid(now)) it = corpses.erase(it);
		else ++it;
	}
}

uint32_t moveCurrency(Player& player, Item& coin, Cylinder& destination, int32_t index, uint32_t requested)
{
	const uint32_t weight = Item::items[coin.getID()].weight;
	if (!player.hasFlag(PlayerFlag_HasInfiniteCapacity) && weight) requested = std::min(requested, player.getFreeCapacity() / weight);
	if (!requested) return 0;
	Cylinder* source = coin.getParent();
	const uint32_t before = coin.getItemCount();
	struct GoldMove {
		Item* previous;
		explicit GoldMove(Item& source) : previous(activeGoldSource) { activeGoldSource = &source; }
		~GoldMove() { activeGoldSource = previous; }
	} moving(coin);
	g_game.internalMoveItem(source, &destination, index, &coin, std::min(before, requested), nullptr);
	// A movement error can still mean that part of a stack was transferred.
	const uint32_t remaining = coin.getParent() == source ? coin.getItemCount() : 0;
	return before >= remaining ? before - remaining : 0;
}

uint32_t transfer(Player& player, Item& coin)
{
	const std::vector<Item*> carried = EasyMode::getCarriedItems(player);
	if (g_config.getBoolean(ConfigManager::EASY_MODE_GOLD_BAG)) {
		Container* goldBag = nullptr;
		for (Item* item : carried) {
			if (EasyMode::recognizeGoldBag(item->getID(), item->getActionId()) && item->getContainer()) {
				goldBag = item->getContainer();
				break; // Stable carried breadth-first order; no persistent selection.
			}
		}
		if (goldBag) {
			for (Item* destination : carried) {
				if (inside(*destination, *goldBag) && ordinaryCurrency(*destination) && destination->equals(&coin) && destination->getItemCount() < 100) {
					uint32_t moved = moveCurrency(player, coin, *destination->getParent(), destination->getParent()->getThingIndex(destination), 100 - destination->getItemCount());
					if (moved) return moved;
				}
			}
			for (Item* item : carried) {
				if (item == goldBag || inside(*item, *goldBag)) {
					if (Container* destination = item->getContainer()) {
						if (destination->size() < destination->capacity()) {
							uint32_t moved = moveCurrency(player, coin, *destination, destination->size(), coin.getItemCount());
							if (moved) return moved;
						}
					}
				}
			}
		}
	}
	for (Item* destination : carried) {
		if (ordinaryCurrency(*destination) && destination->equals(&coin) && destination->getItemCount() < 100) {
			uint32_t moved = moveCurrency(player, coin, *destination->getParent(), destination->getParent()->getThingIndex(destination), 100 - destination->getItemCount());
			if (moved) return moved;
		}
	}
	for (Item* item : carried) {
		if (Container* container = item->getContainer()) {
			if (container->size() < container->capacity()) {
				uint32_t moved = moveCurrency(player, coin, *container, container->size(), coin.getItemCount());
				if (moved) return moved;
			}
		}
	}
	for (int slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
		if (!player.getInventoryItem(static_cast<slots_t>(slot))) {
			uint32_t moved = moveCurrency(player, coin, player, slot, coin.getItemCount());
			if (moved) return moved;
		}
	}
	return 0;
}
}

void EasyMode::onLootItemMove(Item& item, const Item* mergeTarget)
{
	if ((!enabled() && !spillEnabled()) || &item == activeGoldSource) return;
	const Container* movedContainer = item.getContainer();
	for (const auto& entry : corpses) {
		if (entry->corpse == &item) entry->invalidated = true;
		auto mark = [&](const std::vector<Item*>& candidates) {
			for (Item* candidate : candidates) {
				if (candidate == &item || candidate == mergeTarget || (movedContainer && movedContainer->isHoldingItem(candidate)))
					entry->touched.insert(candidate);
			}
		};
		mark(entry->coins);
		mark(entry->spill);
	}
}

void EasyMode::onLootGenerated(Container& corpse)
{
	if ((!enabled() && !spillEnabled()) || !corpse.getParent() || corpse.getParent()->getTile() != corpse.getParent()) return;
	prune();
	for (const auto& entry : corpses) if (entry->corpse == &corpse) return;
	std::unique_ptr<CorpseEntry> entry(new CorpseEntry(corpse));
	std::vector<Container*> containers{&corpse};
	for (size_t i = 0; i < containers.size(); ++i) {
		for (Item* item : containers[i]->getItemList()) {
			if (Container* child = item->getContainer()) containers.push_back(child);
			else if (ordinaryCurrency(*item)) {
				item->incrementReferenceCounter();
				entry->coins.push_back(item);
			}
		}
	}
	if (spillEnabled() && ordinarySpillCorpse(corpse)) {
		containers = {&corpse};
		for (size_t i = 0; i < containers.size(); ++i) {
			for (Item* item : containers[i]->getItemList()) {
				if (Container* child = item->getContainer()) {
					if (isSpillContainer(*item)) containers.push_back(child);
				} else if (isSpillItem(*item)) {
					item->incrementReferenceCounter();
					entry->spill.push_back(item);
				}
			}
		}
	}
	if (corpses.size() >= MAX_REGISTERED_CORPSES) corpses.pop_front();
	corpses.push_back(std::move(entry));
	if (spillEnabled()) spillEntry(*corpses.back());
}

void EasyMode::spillLoot()
{
	if (!spillEnabled()) return;
	prune();
	for (const auto& entry : corpses) spillEntry(*entry);
	prune();
}

uint64_t EasyMode::collectGold(Player& player)
{
	if (!enabled() || player.isRemoved() || player.getHealth() <= 0) return 0;
	prune();
	uint64_t collected = 0;
	for (const auto& entry : corpses) {
		Container& corpse = *entry->corpse;
		const Position& position = corpse.getPosition();
		if (!Position::areInRange<GOLD_PICKUP_RADIUS_TILES, GOLD_PICKUP_RADIUS_TILES, 0>(position, player.getPosition()) ||
		    !player.canSee(position) || !g_game.isSightClear(player.getPosition(), position, true)) continue;
		for (auto it = entry->coins.begin(); it != entry->coins.end(); ) {
			if (entry->invalidated) break;
			Item* coin = *it;
			if (!entry->touched.count(coin) && inside(*coin, corpse) && ordinaryCurrency(*coin) && !inAnyTrade(*coin)) {
				const uint64_t unit = coin->getWorth() / coin->getItemCount();
				while (inside(*coin, corpse)) {
					uint32_t moved = transfer(player, *coin);
					if (!moved) break;
					collected += unit * moved;
				}
			}
			if (entry->touched.count(coin) || !inside(*coin, corpse) || !ordinaryCurrency(*coin)) {
				entry->touched.erase(coin);
				coin->decrementReferenceCounter();
				it = entry->coins.erase(it);
			} else ++it;
		}
	}
	prune();
	return collected;
}
