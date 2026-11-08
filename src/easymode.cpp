#include "otpch.h"

#include "easymode.h"
#include "configmanager.h"
#include "item.h"
#include "actions.h"
#include "combat.h"
#include "container.h"
#include "game.h"
#include "monster.h"
#include "player.h"

extern ConfigManager g_config;
extern Game g_game;
extern Actions* g_actions;

namespace {
bool inTrade(Player& player, const Item& item)
{
	const Item* trade = player.getTradeItem();
	for (const Item* current = &item; current; ) {
		if (current == trade) {
			return true;
		}
		const Cylinder* parent = current->getParent();
		current = parent ? parent->getItem() : nullptr;
	}
	return false;
}

Item* findConsumable(Player& player, EasyMode::Consumable kind)
{
	std::vector<Item*> items;
	for (int slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
		if (Item* item = player.getInventoryItem(static_cast<slots_t>(slot))) {
			items.push_back(item);
		}
	}
	for (size_t i = 0; i < items.size(); ++i) {
		Item* item = items[i];
		if (inTrade(player, *item)) {
			continue;
		}
		if (EasyMode::recognizeConsumable(item->getID(), item->getActionId(), item->getSubType()) == kind &&
		    (kind == EasyMode::Consumable::ManaFluid || item->getCharges() > 0)) {
			return item;
		}
		if (Container* container = item->getContainer()) {
			items.insert(items.end(), container->getItemList().begin(), container->getItemList().end());
		}
	}
	return nullptr;
}
}

bool EasyMode::isEndlessRune(const Item& item)
{
	if (!g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) ||
	    !g_config.getBoolean(ConfigManager::EASY_MODE_ENDLESS_RUNES)) {
		return false;
	}
	const Consumable kind = recognizeConsumable(item.getID(), item.getActionId(), item.getSubType());
	return kind == Consumable::UltimateHealing || kind == Consumable::GreatFireball || kind == Consumable::Explosion;
}

bool EasyMode::isEndlessManaFluid(const Item& item)
{
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
	       g_config.getBoolean(ConfigManager::EASY_MODE_ENDLESS_MANA_FLUID) &&
	       recognizeConsumable(item.getID(), item.getActionId(), item.getSubType()) == Consumable::ManaFluid;
}

bool EasyMode::useConsumable(Player& player, const std::string& command)
{
	if (!g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) ||
	    !g_config.getBoolean(ConfigManager::EASY_MODE_COMMANDS)) {
		player.sendCancelMessage("Easy Mode consumable commands are disabled.");
		return false;
	}
	Consumable kind = Consumable::None;
	for (const auto& definition : CONSUMABLES) {
		if (command == definition.command) {
			kind = definition.kind;
			break;
		}
	}
	if (kind == Consumable::None || player.isRemoved() || player.getHealth() <= 0) {
		return false;
	}
	const bool mana = kind == Consumable::ManaFluid;
	if (!g_config.getBoolean(mana ? ConfigManager::EASY_MODE_ENDLESS_MANA_FLUID : ConfigManager::EASY_MODE_ENDLESS_RUNES)) {
		player.sendCancelMessage("The required endless consumable feature is disabled.");
		return false;
	}
	Item* item = findConsumable(player, kind);
	if (!item || item->getTopParent() != &player) {
		player.sendCancelMessage("You are not carrying the required marked endless item.");
		return false;
	}
	Creature* target = &player;
	if (kind == Consumable::Explosion || kind == Consumable::GreatFireball) {
		target = player.getAttackedCreature();
		if (!target || !target->getMonster() || target->isRemoved() || target->getHealth() <= 0 ||
		    isPlayerControlled(*target) ||
		    target->getPosition().z != player.getPosition().z ||
		    !player.canSee(target->getPosition()) || !player.canSeeCreature(target)) {
			player.sendCancelMessage("Select a living, visible monster first.");
			return false;
		}
		ReturnValue ret = Combat::canTargetCreature(&player, target);
		if (ret != RETURNVALUE_NOERROR) {
			player.sendCancelMessage(ret);
			return false;
		}
	}
	// Share the existing action clock. Never queue stale target/item requests.
	if (!player.canDoAction()) {
		player.sendCancelMessage(RETURNVALUE_YOUAREEXHAUSTED);
		return false;
	}
	// Items are already verified as carried; no client container window is needed.
	return g_actions->useItemEx(&player, Position(0xFFFF, 0, 0), target->getPosition(), 0, item, target);
}

bool EasyMode::isPlayerControlled(const Creature& creature)
{
	const Creature* master = creature.getMaster();
	for (unsigned depth = 0; master && depth < 32; ++depth, master = master->getMaster()) {
		if (master->getPlayer()) return true;
	}
	return master != nullptr; // Exclude malformed/cyclic master chains as well.
}
