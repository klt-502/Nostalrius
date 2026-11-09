#include "otpch.h"
#include "easyspill.h"
#include "easymode.h"
#include "actions.h"
#include "container.h"
#include "player.h"
#include "movement.h"
#include "tile.h"

extern Actions* g_actions;
extern MoveEvents* g_moveEvents;

namespace {
bool unmarked(const Item& item)
{
	return !item.hasAttribute(static_cast<itemAttrTypes>(0xFFFFFFFFu));
}

bool hasItemHandler(const Item& item)
{
	return !g_actions || !g_moveEvents || g_actions->hasItemAction(item.getID()) || g_moveEvents->hasItemEvent(item.getID());
}

bool interactive(const ItemType& type, bool resident = false)
{
	return type.useEvent || type.multiUseEvent || type.collisionEvent || type.separationEvent ||
	       type.useable || type.changeUse || type.forceUse || type.distUse || type.canReadText ||
	       type.canWriteText || type.rotatable || type.isHangable || type.conditionDamage ||
	       type.transformToOnUse || type.transformToFree ||
	       (type.fluidSource != FLUID_NONE && !(resident && type.corpse));
}

class SpillPolicy {
	public:
		SpillPolicy() {
			pugi::xml_document document;
			const auto result = document.load_file("data/zedas/lootspill.xml");
			if (!result || std::string(document.document_element().name()) != "lootspill") {
				std::cout << "[Warning - EasyMode] Loot spill disabled: invalid data/zedas/lootspill.xml" << std::endl;
				return;
			}
			std::map<uint16_t, std::string> validated;
			for (const auto node : document.document_element().children()) {
				const std::string idText = node.attribute("id").value();
				uint32_t id = 0;
				bool digits = !idText.empty();
				for (char digit : idText) {
					if (digit < '0' || digit > '9' || id > 6553) { digits = false; break; }
					id = id * 10 + digit - '0';
				}
				const std::string name = node.attribute("name").value();
				if (std::string(node.name()) != "item" || !digits || !id || id > 65535 || id >= Item::items.size() ||
				    name.empty() || Item::items[id].id != id || Item::items[id].name != name ||
				    !validated.emplace(static_cast<uint16_t>(id), name).second) {
					std::cout << "[Warning - EasyMode] Loot spill disabled: invalid/duplicate item identity in policy" << std::endl;
					return;
				}
			}
			items.swap(validated);
		}
		bool contains(const Item& item) const {
			const auto it = items.find(item.getID());
			return it != items.end() && it->second == Item::items[item.getID()].name;
		}
	private:
		std::map<uint16_t, std::string> items;
};

bool safeTile(const Tile& tile)
{
	if (!g_actions || !g_moveEvents || !tile.getGround() || tile.getThingCount() >= EasyMode::CLIENT_TILE_THING_LIMIT ||
	    tile.hasFlag(TILESTATE_TELEPORT | TILESTATE_MAGICFIELD | TILESTATE_MAILBOX | TILESTATE_BED |
	                 TILESTATE_DEPOT | TILESTATE_BLOCKSOLID | TILESTATE_BLOCKPATH) ||
	    g_moveEvents->hasPositionEvent(tile.getPosition())) return false;
	// A resident ground/item can react to an added item, even if the added item has no script.
	for (size_t i = tile.getFirstIndex(); i < tile.getLastIndex(); ++i) {
		Thing* thing = tile.getThing(i);
		if (!thing || !thing->getItem()) continue;
		const Item& resident = *thing->getItem();
		const ItemType& type = Item::items[resident.getID()];
		const uint32_t normal = ITEM_ATTRIBUTE_DURATION | ITEM_ATTRIBUTE_DECAYSTATE |
		                        (type.corpse ? ITEM_ATTRIBUTE_CORPSEOWNER : ITEM_ATTRIBUTE_NONE) |
		                        (type.isSplash() ? ITEM_ATTRIBUTE_FLUIDTYPE : ITEM_ATTRIBUTE_NONE);
		if (resident.hasAttribute(static_cast<itemAttrTypes>(~normal)) || resident.isChestQuest() ||
		    interactive(type, true) || g_actions->hasItemAction(resident.getID()) || g_moveEvents->hasItemEvent(resident.getID())) return false;
	}
	return true;
}
}

bool EasyMode::isSpillContainer(const Item& item)
{
	const ItemType& type = Item::items[item.getID()];
	return item.getContainer() && !item.isChestQuest() && type.isContainer() && type.moveable && type.pickupable &&
	       unmarked(item) && !interactive(type) && !hasItemHandler(item);
}

bool EasyMode::isSpillItem(const Item& item)
{
	// An explicit reviewed allowlist is required. Flags alone cannot identify an
	// unmarked semantic quest item. Unknown IDs fail closed, regardless of value.
	static const SpillPolicy policy;
	const ItemType& type = Item::items[item.getID()];
	return policy.contains(item) && !item.getContainer() && item.getWorth() == 0 &&
	       item.getItemCount() > 0 && (!item.isStackable() || item.getItemCount() <= 100) &&
	       type.moveable && type.pickupable && !type.alwaysOnTop && !type.isGroundTile() &&
	       !type.isFluidContainer() && !type.isSplash() && !type.isMagicField() && !type.isKey() &&
	       !type.isDoor() && !type.isChest() && !type.isDepot() && !type.isTeleport() && !type.isBed() &&
	       !type.isRune() && !type.corpse && !type.blockSolid && !type.blockPathFind &&
	       unmarked(item) && !interactive(type) && !hasItemHandler(item);
}

bool EasyMode::spillItem(Container& source, Tile& tile, Item& item)
{
	if (item.getParent() != &source || !isSpillItem(item) || !safeTile(tile)) return false;
	const uint32_t count = item.getItemCount();
	const int32_t oldIndex = source.getThingIndex(&item);
	uint32_t maximum = 0;
	if (oldIndex < 0 || source.queryRemove(item, count, 0) != RETURNVALUE_NOERROR ||
	    tile.queryAdd(INDEX_WHEREEVER, item, count, 0) != RETURNVALUE_NOERROR ||
	    tile.queryMaxCount(INDEX_WHEREEVER, item, count, maximum, 0) != RETURNVALUE_NOERROR || maximum < count) return false;

	// Game::internalMoveItem deliberately clones stackable remainders, including
	// complete stacks. Spill instead transfers this entire existing instance via
	// the same Cylinder checks, weight/client updates and normal notifications.
	// No autostack, exchange, partial move, item factory, release or count change.
	source.removeThing(&item, count);
	tile.addThing(&item); // Ordinary down item: prepended above the existing corpse.
	const int32_t newIndex = tile.getThingIndex(&item);
	if (item.getParent() != &tile || newIndex < 0) {
		// Prechecks exclude every Tile::addThing replacement/rejection case. Keep
		// the instance in its original container if a future engine change refuses it.
		source.addThing(0, &item);
		return false;
	}
	source.postRemoveNotification(&item, &tile, oldIndex);
	tile.postAddNotification(&item, &source, newIndex);
	return true;
}
