#ifndef ZEDAS_BROKER_FOOD_RING_CASES_H
#define ZEDAS_BROKER_FOOD_RING_CASES_H
static void testBrokerFoodRings(bool mislabeled)
{
	if (mislabeled) {
		// A data-only mistake cannot re-enable food spill or ring liquidation.
		pugi::xml_document policy;
		require(policy.load_file("data/zedas/lootspill.xml"), "load guard fixture policy");
		for (const char *id : {"3577", "3007"})
			policy.child("lootspill")
			    .find_child_by_attribute("item", "id", id)
			    .attribute("category")
			    .set_value("broker-fodder");
		policy.child("lootspill")
		    .find_child_by_attribute("item", "id", "3600")
		    .attribute("category")
		    .set_value("player-interest");
		policy.save_file("data/zedas/lootspill.xml");
		std::string error;
		require(Broker::loadCatalog("data/zedas/broker.xml", error), error.c_str());
		auto *corpse = corpseAt(99, 100);
		auto *meat = Item::CreateItem(3577, 2);
		auto *bread = Item::CreateItem(3600, 2);
		auto *ring = Item::CreateItem(3007);
		for (auto *item : {meat, bread, ring})
			require(g_game.internalAddItem(corpse, item) == RETURNVALUE_NOERROR, "mislabeled generated fixture");
		EasyMode::onLootGenerated(*corpse);
		require(meat->getParent() == corpse && bread->getParent() == corpse && !EasyMode::isSpillItem(*meat) &&
		            !EasyMode::isSpillItem(*bread),
		        "native nutrition prevents food spill despite either eligible data class");
		require(ring->getParent() == corpse->getTile() &&
		            EasyMode::lootClass(*ring) == EasyMode::LootClass::PlayerInterest,
		        "native ring slot overrides accidental broker-fodder label without changing safe spill");
		Container escrow(2854, 20);
		require(!EasyMode::takeBrokerItem(*ring, escrow) && !Broker::price(*ring) &&
		            !EasyMode::isBrokerCandidate(*ring, OTSYS_TIME() + 1000, 0) && escrow.empty(),
		        "misclassified ring cannot be witnessed, valued or transferred as broker stock");
		auto *groundFood = Item::CreateItem(3577, 2);
		require(g_game.internalAddItem(corpse->getTile(), groundFood) == RETURNVALUE_NOERROR,
		        "manual ground food fixture");
		require(!EasyMode::takeBrokerItem(*groundFood, escrow) && !Broker::price(*groundFood) && escrow.empty(),
		        "native food guard also blocks direct private-escrow transfer");
		return;
	}
	require(Database::getInstance()->connect(), "food/ring fixture DB");
	auto *db = Database::getInstance();
	for (const char *table :
	     {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
		require(db->executeQuery(std::string("DELETE FROM ") + table), "reset food/ring fixture");
	Broker::initialize();
	auto *owner = makePlayer(100, 100);
	owner->setGUID(1);
	Broker::command(*owner, "hire");
	struct GeneratedTree {
		Container *corpse, *bag = nullptr;
		explicit GeneratedTree(Container *source) : corpse(source) {}
		Item *add(uint16_t id)
		{
			if (!bag || bag->size() == bag->capacity()) {
				bag = Item::CreateItem(2853)->getContainer();
				require(g_game.internalAddItem(corpse, bag) == RETURNVALUE_NOERROR, "generated ordinary food/ring bag");
			}
			Item *item = Item::CreateItem(id, Item::items[id].stackable ? 2 : 1);
			require(g_game.internalAddItem(bag, item) == RETURNVALUE_NOERROR, "generated exact food/ring instance");
			return item;
		}
	} foodTree(corpseAt(99, 100)), ringTree(corpseAt(98, 100));
	struct State {
		Item *item;
		Cylinder *parent;
		uint16_t subtype;
		uint32_t duration;
	};
	std::vector<State> foods, rings;
	const std::set<uint16_t> passive = {3004, 3006, 3007, 3063};
	for (size_t id = 1; id < Item::items.size(); ++id) {
		const auto &type = Item::items[id];
		if (type.id != id)
			continue;
		if (type.nutrition > 0) {
			auto *item = foodTree.add(id);
			foods.push_back({item, item->getParent(), item->getSubType(), item->getDuration()});
		}
		if (type.slotPosition & SLOTP_RING) {
			auto *item = ringTree.add(id);
			rings.push_back({item, item->getParent(), item->getSubType(), item->getDuration()});
		}
	}
	require(foods.size() == 39 && rings.size() == 26, "audit every native edible and ring-slot identity");
	EasyMode::onLootGenerated(*foodTree.corpse);
	EasyMode::onLootGenerated(*ringTree.corpse);
	for (const auto &state : foods) {
		auto &item = *state.item;
		require(item.getParent() == state.parent && !EasyMode::isSpillItem(item) && !Broker::price(item) &&
		            item.getSubType() == state.subtype && item.getDuration() == state.duration,
		        "every food including meat/ham/bread/cheese stays in its generated bag unchanged");
		require(EasyMode::lootClass(item) ==
		            (item.getID() == 3250 ? EasyMode::LootClass::Protected : EasyMode::LootClass::CorpseJunk),
		        "ordinary food is corpse contents; quest carrot retains protection");
	}
	Container escrow(2854, 20);
	for (const auto &state : rings) {
		auto &item = *state.item;
		require(!Broker::price(item) && !EasyMode::takeBrokerItem(item, escrow) && item.getSubType() == state.subtype &&
		            item.getDuration() == state.duration,
		        "no ring is sellable or transferable; charges/duration unchanged");
		if (passive.count(item.getID()))
			require(item.getParent() == ringTree.corpse->getTile() &&
			            EasyMode::lootClass(item) == EasyMode::LootClass::PlayerInterest,
			        "safe passive ring spills as exact player-interest instance");
		else
			require(item.getParent() == state.parent && EasyMode::lootClass(item) == EasyMode::LootClass::Protected,
			        "charged/transforming/scripted/special ring remains protected inside");
	}
	auto base = OTSYS_TIME();
	BrokerEngineTestAccess::clearOutput(*owner);
	for (int step = 1; step <= 3; ++step)
		Broker::tick(*owner, base + step * 1100);
	BrokerWire(*owner).expect("");
	require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1")->getNumber<unsigned>("n") ==
	                0 &&
	            escrow.empty(),
	        "food and rings never enter broker escrow or consume capacity");
	// Ordinary approved fodder still claims immediately using the accepted logistics.
	auto *equipment = corpseAt(99, 101);
	auto *hatchet = Item::CreateItem(3276);
	require(g_game.internalAddItem(equipment, hatchet) == RETURNVALUE_NOERROR, "mixed hunt equipment");
	EasyMode::onLootGenerated(*equipment);
	require(g_game.internalTeleport(brokerFor(*owner), Position(99, 100, 7)) == RETURNVALUE_NOERROR,
	        "collector native endpoint");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 4400);
	BrokerWire(*owner).expect("Got a hatchet.");
	require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND item=3276")
	                ->getNumber<unsigned>("n") == 1,
	        "approved equipment still enters one field slot");
	// The real eating action remains usable after manual retrieval, with unchanged consumption.
	auto *backpack = Item::CreateItem(2854)->getContainer();
	require(g_game.internalAddItem(owner, backpack, CONST_SLOT_BACKPACK) == RETURNVALUE_NOERROR,
	        "manual food backpack");
	auto bread = std::find_if(foods.begin(), foods.end(), [](const State &s) { return s.item->getID() == 3600; });
	require(bread != foods.end() && g_game.internalMoveItem(bread->item->getParent(), backpack, INDEX_WHEREEVER,
	                                                        bread->item, 2, nullptr) == RETURNVALUE_NOERROR,
	        "player may retrieve ordinary bread normally");
	// Historical manual stack movement may replace the source instance; inspect the actual destination.
	Item *carriedBread = nullptr;
	for (auto *item : backpack->getItemList())
		if (item->getID() == 3600)
			carriedBread = item;
	require(carriedBread && carriedBread->getItemCount() == 2 &&
	            g_actions->useItem(owner, Position(0xFFFF, CONST_SLOT_BACKPACK, 0), 0, carriedBread) &&
	            carriedBread->getItemCount() == 1,
	        "unchanged food Lua action feeds and consumes one manually retrieved bread");
	// Old persisted food is never silently consumed, repriced, sold or dropped.
	require(
	    db->executeQuery("INSERT INTO zedas_broker_accounts(owner,pending,funds) VALUES(2,17,5)") &&
	        db->executeQuery(
	            "INSERT INTO zedas_broker_escrow(claim_key,owner,item,count,price) VALUES('legacy-food',2,3577,2,2)"),
	    "legacy food escrow recovery fixture");
	auto *other = makePlayer(104, 100);
	other->setGUID(2);
	Broker::command(*other, "hire");
	require(
	    !brokerFor(*other) &&
	        db->storeQuery("SELECT count FROM zedas_broker_escrow WHERE claim_key='legacy-food' AND item=3577 AND "
	                       "price=2 AND sold=0")
	                ->getNumber<unsigned>("count") == 2 &&
	        db->storeQuery("SELECT pending FROM zedas_broker_accounts WHERE owner=2")->getNumber<unsigned>("pending") ==
	            17 &&
	        db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_sales WHERE owner=2")->getNumber<unsigned>("n") == 0,
	    "narrowed catalog fails closed and retains original legacy food/balance for recovery");
	Broker::stop(*other);
	Broker::stop(*owner);
}
#endif
