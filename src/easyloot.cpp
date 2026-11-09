#include "otpch.h"
#include "easymode.h"
#include "configmanager.h"
#include "container.h"
#include "game.h"
#include "player.h"

extern ConfigManager g_config;
extern Game g_game;

namespace {
bool enabled()
{
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
	       g_config.getBoolean(ConfigManager::EASY_MODE_AUTO_GOLD);
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
	explicit CorpseEntry(Container& container) : corpse(&container), originalParent(container.getParent()),
		originalId(container.getID()), registeredAt(OTSYS_TIME()) {
		corpse->incrementReferenceCounter();
	}
	~CorpseEntry() {
		for (Item* coin : coins) coin->decrementReferenceCounter();
		corpse->decrementReferenceCounter();
	}
	bool valid(int64_t now) const {
		return corpse->getParent() == originalParent && originalParent && originalParent->getTile() == originalParent &&
		       !corpse->isRemoved() && corpse->getID() == originalId && now - registeredAt < EasyMode::CORPSE_REGISTRY_TTL_MS;
	}
};

// Dispatcher-only. References protect queued handles through moves, decay and cleanup.
std::list<std::unique_ptr<CorpseEntry>> corpses;

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

void EasyMode::onLootGenerated(Container& corpse)
{
	if (!enabled() || !corpse.getParent() || corpse.getParent()->getTile() != corpse.getParent()) return;
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
	if (entry->coins.empty()) return;
	if (corpses.size() >= MAX_REGISTERED_CORPSES) corpses.pop_front();
	corpses.push_back(std::move(entry));
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
			Item* coin = *it;
			if (inside(*coin, corpse) && ordinaryCurrency(*coin) && !inAnyTrade(*coin)) {
				const uint64_t unit = coin->getWorth() / coin->getItemCount();
				while (inside(*coin, corpse)) {
					uint32_t moved = transfer(player, *coin);
					if (!moved) break;
					collected += unit * moved;
				}
			}
			if (!inside(*coin, corpse) || !ordinaryCurrency(*coin)) {
				coin->decrementReferenceCounter();
				it = entry->coins.erase(it);
			} else ++it;
		}
	}
	prune();
	return collected;
}
