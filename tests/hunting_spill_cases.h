#ifndef ZEDAS_HUNTING_SPILL_CASES_H
#define ZEDAS_HUNTING_SPILL_CASES_H

static void testHuntingSpill()
{
	auto add = [](Container& container, uint16_t id, uint16_t count = 1) {
		Item* item = Item::CreateItem(id, count);
		require(item && g_game.internalAddItem(&container, item, INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR,
		        "add generated hunting fixture");
		return item;
	};
	// Validate the explicit policy against real catalog state and real registered
	// use/equipment handlers, rather than assuming XML membership implies safety.
	pugi::xml_document policy;
	require(policy.load_file("data/zedas/lootspill.xml"), "load hunting identity policy");
	unsigned identities = 0;
	for (const auto node : policy.child("lootspill").children("item")) {
		const uint16_t id = node.attribute("id").as_uint();
		Item* item = Item::CreateItem(id, 1);
		require(item && EasyMode::isSpillItem(*item), ("reviewed hunting identity is actually eligible: " + std::to_string(id)).c_str());
		item->decrementReferenceCounter();
		++identities;
	}
	require(identities == 242, "validate every reviewed hunting identity");
	require(g_actions->getItemGroundUse(3264) == Action::GroundUse::Weapon &&
	        g_actions->getItemGroundUse(3577) == Action::GroundUse::Food, "real stock handlers are fingerprinted and recognized");
	require(g_moveEvents->hasItemEvent(3079) && !g_moveEvents->hasUnsafeGroundItemEvent(3079),
	        "native equipment-only boots handler is safe for ground movement");
	for (uint16_t id : {2968, 3105, 3147, 3160, 3191, 3200, 3235, 3244, 3250, 3284, 3045, 3048, 3050, 3051, 3081, 3094}) {
		Item* item = Item::CreateItem(id, 1);
		require(item && !EasyMode::isSpillItem(*item), "quest, key, rune, charged or transforming identity stays excluded");
		item->decrementReferenceCounter();
	}

	Player* collector = makePlayer(100, 104);
	Container* goldBag = carry(*collector, 2856, 65005)->getContainer();
	const int32_t initialSpeed = collector->getSpeed();
	Container* corpse = corpseAt(100, 103);
	Container* bag = add(*corpse, 2853)->getContainer();
	Container* nested = add(*bag, 2853)->getContainer();
	Item* sword = add(*bag, 3264);
	Item* armor = add(*bag, 3355);
	Item* tusks = add(*nested, 3044, 3);
	Item* meat = add(*nested, 3577, 3);
	Item* ring = add(*nested, 3007);
	Item* arrows = add(*nested, 3447, 20);
	Item* boots = add(*bag, 3079);
	addCoin(*nested, 3031, 30);
	addCoin(*nested, 3035, 2);
	addCoin(*nested, 3043, 1);
	Item* marked = add(*bag, 3264);
	marked->setActionId(65001);
	Item* rune = add(*bag, 3160);
	Item* iceRapier = add(*bag, 3284);
	Container* specialBag = add(*corpse, 3244)->getContainer();
	Item* protectedChild = add(*specialBag, 3357);
	require(!EasyMode::isSpillContainer(*specialBag), "special quest container is never traversed");
	const uint64_t money = contentsValue(*corpse);
	EasyMode::onLootGenerated(*corpse);
	Tile* tile = corpse->getTile();
	for (Item* item : {sword, armor, tusks, meat, ring, arrows, boots}) {
		require(item->getParent() == tile && tile->getThingIndex(item) >= 0,
		        "ordinary nested hunting loot moves the exact existing instance onto the corpse tile");
	}
	require(tusks->getItemCount() == 3 && meat->getItemCount() == 3 && arrows->getItemCount() == 20 &&
	        collector->getSpeed() == initialSpeed && !ring->hasAttribute(static_cast<itemAttrTypes>(0xFFFFFFFFu)),
	        "stack counts, passive jewelry state and player equipment stats stay unchanged; food is not consumed");
	require(marked->getParent() == bag && rune->getParent() == bag && iceRapier->getParent() == bag &&
	        protectedChild->getParent() == specialBag, "mixed protected contents and quest-container subtree remain inside");
	require(money == 10230 && contentsValue(*corpse) == money && EasyMode::collectGold(*collector) == money &&
	        contentsValue(*goldBag) == money && contentsValue(*corpse) == 0,
	        "currency in the same generated bag uses existing auto-gold and the marked Gold Bag only");
	for (uint16_t id : {3031, 3035, 3043}) require(tile->getItemTypeCount(id) == 0, "coins are never also spilled");
	Item* inserted = add(*bag, 3274);
	EasyMode::onLootGenerated(*corpse);
	EasyMode::spillLoot();
	require(inserted->getParent() == bag && EasyMode::collectGold(*collector) == 0,
	        "broader policy never rescans later insertions or duplicates value");

	Container* overflow = corpseAt(100, 105);
	std::vector<Item*> swords;
	for (int i = 0; i < 9; ++i) swords.push_back(add(*overflow, 3264));
	EasyMode::onLootGenerated(*overflow);
	unsigned exposed = 0;
	Item* picked = nullptr;
	for (Item* item : swords) if (item->getParent() == overflow->getTile()) { ++exposed; picked = item; }
	require(exposed == 8 && overflow->size() == 1 && overflow->getTile()->getThingCount() == 10,
	        "spilled stock-use weapons do not block following loot, while the ten-thing budget still bounds overflow");
	Container* backpack = collector->getInventoryItem(CONST_SLOT_BACKPACK)->getContainer();
	require(picked && g_game.internalMoveItem(overflow->getTile(), backpack, INDEX_WHEREEVER, picked, 1, nullptr) == RETURNVALUE_NOERROR,
	        "manually pick up an existing spilled weapon");
	EasyMode::spillLoot();
	require(overflow->empty() && overflow->getTile()->getThingCount() == 10 && picked->getTopParent() == collector,
	        "overflow retries expose only the remaining actual weapon after a visible slot opens");

	// A real death exercises the engine's automatic weapon bag as well as an
	// explicitly generated nested bag containing armor, food and products.
	MonsterType* type;
	Monster* monster = makeMonster(5, 105, 104, true, true, &type);
	type->info.lookcorpse = 3994;
	LootBlock generatedBag;
	generatedBag.id = 2853; generatedBag.countmax = 1; generatedBag.chance = MAX_LOOTCHANCE + 1;
	for (uint16_t id : {3355, 3044, 3577, 3007, 3447}) {
		LootBlock child; child.id = id; child.countmax = 1; child.chance = MAX_LOOTCHANCE + 1;
		generatedBag.childLoot.push_back(child);
	}
	type->info.lootItems.push_back(generatedBag);
	LootBlock weapon; weapon.id = 3264; weapon.countmax = 1; weapon.chance = MAX_LOOTCHANCE + 1;
	type->info.lootItems.push_back(weapon);
	getRandomGenerator().seed(12345);
	monster->changeHealth(-5);
	monster->onDeath();
	EasyMode::spillLoot();
	Tile* deathTile = g_game.map.getTile(105, 104, 7);
	require(deathTile->getItemTypeCount(3264) == 1 && deathTile->getItemTypeCount(3355) == 1,
	        "actual monster death exposes its automatic weapon bag and nested ordinary armor");

	// Reload real interfaces with unrelated/custom handlers. An approved ID or
	// a stock-looking filename must never grant an exception by itself.
	auto retained = [&](uint16_t id, int x) {
		Container* protectedCorpse = corpseAt(x, 96);
		Item* item = add(*protectedCorpse, id);
		EasyMode::onLootGenerated(*protectedCorpse);
		require(item->getParent() == protectedCorpse, "unsafe/custom handler retains reviewed identity in corpse");
	};
	{
		std::ofstream file("data/actions/actions.xml");
		file << "<actions><action itemid='3416' script='misc/fluids.lua'/></actions>";
	}
	require(g_actions->reload(), "reload unrelated item use handler");
	retained(3416, 90);
	{
		std::ofstream file("data/actions/actions.xml");
		file << "<actions><action itemid='3357' script='misc/weapons.lua'/>"
		        "<action itemid='3264' script='misc/food.lua'/></actions>";
	}
	require(g_actions->reload(), "load stock handlers on incompatible item roles");
	retained(3357, 100);
	retained(3264, 102);
	{
		std::ofstream file("data/actions/scripts/misc/weapons.lua", std::ios::app);
		file << "\n-- custom handler revision\n";
	}
	{
		std::ofstream file("data/actions/scripts/misc/food.lua", std::ios::app);
		file << "\n-- custom food revision\n";
	}
	{
		std::ofstream file("data/actions/actions.xml");
		file << "<actions><action itemid='3264' script='misc/weapons.lua'/>"
		        "<action itemid='3577' script='misc/food.lua'/></actions>";
	}
	require(g_actions->reload() && g_actions->getItemGroundUse(3264) == Action::GroundUse::Unknown &&
	        g_actions->getItemGroundUse(3577) == Action::GroundUse::Unknown, "changed stock filenames fail fingerprint approval");
	retained(3264, 92);
	retained(3577, 94);
	{
		std::ofstream file("data/movements/scripts/custom.lua");
		file << "function onEquip(player, item, slot, isCheck) return true end\n";
	}
	{
		std::ofstream file("data/movements/movements.xml");
		file << "<movements><movevent event='StepIn' itemid='3116' function='onStepInField'/>"
		        "<movevent event='Equip' itemid='3079' slot='feet' script='custom.lua'/></movements>";
	}
	require(g_moveEvents->reload() && g_moveEvents->hasUnsafeGroundItemEvent(3116), "other movement registrations remain unsafe");
	retained(3116, 96);
	require(g_moveEvents->hasUnsafeGroundItemEvent(3079), "Lua equipment scripts never inherit native equipment approval");
	retained(3079, 98);
}

#endif
