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
	for (Item* item : EasyMode::getCarriedItems(player)) {
		if (EasyMode::recognizeConsumable(item->getID(), item->getActionId(), item->getSubType()) == kind &&
		    (kind == EasyMode::Consumable::ManaFluid || item->getCharges() > 0)) {
			return item;
		}
	}
	return nullptr;
}
}

std::vector<Item*> EasyMode::getCarriedItems(Player& player)
{
	std::vector<Item*> pending, result;
	for (int slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
		if (Item* item = player.getInventoryItem(static_cast<slots_t>(slot))) pending.push_back(item);
	}
	for (size_t i = 0; i < pending.size(); ++i) {
		Item* item = pending[i];
		if (inTrade(player, *item)) continue;
		result.push_back(item);
		if (Container* container = item->getContainer()) {
			pending.insert(pending.end(), container->getItemList().begin(), container->getItemList().end());
		}
	}
	return result;
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

namespace {
std::vector<Item*> allLightInventory(Player& player)
{
    std::vector<Item*> items;
    for (int slot=CONST_SLOT_FIRST;slot<=CONST_SLOT_LAST;++slot)
        if (Item* item=player.getInventoryItem(static_cast<slots_t>(slot))) items.push_back(item);
    for (size_t i=0;i<items.size();++i)
        if (Container* bag=items[i]->getContainer()) items.insert(items.end(),bag->getItemList().begin(),bag->getItemList().end());
    return items;
}
}
bool EasyMode::lightWandEnabled()
{
    return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) && g_config.getBoolean(ConfigManager::EASY_MODE_ENDLESS_LIGHT_WAND);
}
bool EasyMode::isEndlessLightWand(const Item& item)
{
    return lightWandEnabled() && item.getActionId()==LIGHT_WAND_ACTION_ID &&
           (item.getID()==LIGHT_WAND_ITEM_ID || item.getID()==LIGHT_WAND_LIT_ITEM_ID);
}
void EasyMode::giveLightWand(Player& player)
{
    if (!lightWandEnabled() || !g_config.getBoolean(ConfigManager::EASY_MODE_COMMANDS)) {
        player.sendCancelMessage("Easy Mode endless light commands are disabled."); return;
    }
    for (Item* item : allLightInventory(player)) if (isEndlessLightWand(*item)) {
        player.sendTextMessage(MESSAGE_STATUS_DEFAULT,"You already carry an endless Magic Light Wand. Use it normally."); return;
    }
    std::unique_ptr<Item> item(Item::CreateItem(LIGHT_WAND_ITEM_ID,1));
    item->setActionId(LIGHT_WAND_ACTION_ID);
    ReturnValue result=g_game.internalAddItem(&player,item.get(),INDEX_WHEREEVER);
    if (result!=RETURNVALUE_NOERROR) { player.sendCancelMessage(result); return; }
    item.release();
    player.sendTextMessage(MESSAGE_STATUS_DEFAULT,"Endless Magic Light Wand received. Use it normally; it is not NPC-sellable.");
}
uint32_t EasyMode::npcWandCount(Player& player,uint16_t id,int32_t subtype)
{
    uint32_t count=0;
    for(Item* item:allLightInventory(player))
        if(item->getID()==id && !isEndlessLightWand(*item) && (subtype<0 || item->getSubType()==subtype)) count+=Item::countByType(item,subtype);
    return count;
}
bool EasyMode::npcRemoveWands(Player& player,uint16_t id,uint32_t amount,int32_t subtype)
{
    if(npcWandCount(player,id,subtype)<amount) return false;
    for(Item* item:allLightInventory(player)) {
        if(!amount)break;
        if(item->getID()!=id || isEndlessLightWand(*item) || (subtype>=0 && item->getSubType()!=subtype))continue;
        const uint32_t count=std::min(amount,Item::countByType(item,subtype));
        if(g_game.internalRemoveItem(item,count)!=RETURNVALUE_NOERROR)return false;
        amount-=count;
    }
    return amount==0;
}

const char* EasyMode::endlessDisplayName(const Item& item)
{
    if (isEndlessRune(item)) {
        switch (recognizeConsumable(item.getID(), item.getActionId(), item.getSubType())) {
            case Consumable::UltimateHealing: return "endless ultimate healing rune";
            case Consumable::GreatFireball: return "endless great fireball rune";
            case Consumable::Explosion: return "endless explosion rune";
            default: break;
        }
    }
    if (isEndlessManaFluid(item)) return "endless mana fluid";
    if (isEndlessLightWand(item)) return "endless magic light wand";
    return nullptr;
}
