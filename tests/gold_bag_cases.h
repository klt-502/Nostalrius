#ifndef ZEDAS_GOLD_BAG_CASES_H
#define ZEDAS_GOLD_BAG_CASES_H

// Include after the shared native-engine fixtures in easymode_engine_test.cpp.
// Run as a separate scenario after setup(), with Easy Mode/auto-gold/Gold Bag on.
static void testGoldBag()
{
	const uint16_t bagId = 2856;
	const uint16_t bagActionId = 65005;
	require(EasyMode::recognizeGoldBag(bagId, bagActionId) &&
	        !EasyMode::recognizeGoldBag(2853, bagActionId) &&
	        !EasyMode::recognizeGoldBag(bagId, 0) &&
	        !EasyMode::recognizeGoldBag(bagId, 65004), "Gold Bag recognition requires the exact reserved pair");
	auto backpack = [](Player& player) {
		return player.getInventoryItem(CONST_SLOT_BACKPACK)->getContainer();
	};
	auto markedBag = [bagId, bagActionId](Container& parent) {
		Container* bag = Item::CreateItem(bagId)->getContainer();
		require(bag != nullptr, "Gold Bag catalog type is a container");
		bag->setActionId(bagActionId);
		require(g_game.internalAddItem(&parent, bag) == RETURNVALUE_NOERROR, "add marked Gold Bag");
		return bag;
	};
	auto denominationCount = [](Container& container, uint16_t id) {
		uint32_t result = 0;
		std::vector<Container*> pending{&container};
		for (size_t i = 0; i < pending.size(); ++i) {
			for (Item* item : pending[i]->getItemList()) {
				if (item->getID() == id) result += item->getItemCount();
				if (Container* child = item->getContainer()) pending.push_back(child);
			}
		}
		return result;
	};

	Player* nestedOwner = makePlayer(90, 94);
	Container* nestedBag = carry(*nestedOwner, bagId, bagActionId)->getContainer();
	require(nestedBag && nestedBag->capacity() == 5, "historical present provides five slots");
	Item* ordinaryStack = addCoin(*backpack(*nestedOwner), 3031, 95);
	Container* coinPouch = Item::CreateItem(2853)->getContainer();
	require(g_game.internalAddItem(nestedBag, coinPouch) == RETURNVALUE_NOERROR, "add carried coin pouch inside Gold Bag");
	Item* bagStack = addCoin(*coinPouch, 3031, 95);
	Container* nestedCorpse = corpseAt(90, 95);
	Container* generatedBag = Item::CreateItem(2853)->getContainer();
	require(g_game.internalAddItem(nestedCorpse, generatedBag) == RETURNVALUE_NOERROR, "add generated loot bag");
	addCoin(*generatedBag, 3031, 30);
	addCoin(*generatedBag, 3035, 2);
	addCoin(*generatedBag, 3043, 1);
	const uint64_t nestedValue = nestedOwner->getMoney() + contentsValue(*nestedCorpse);
	EasyMode::onLootGenerated(*nestedCorpse);
	require(EasyMode::collectGold(*nestedOwner) == 10230, "collect all three denominations from generated nested loot");
	require(ordinaryStack->getItemCount() == 95 && bagStack->getItemCount() == 100,
	        "nested Gold Bag stacks before ordinary carried coin destinations");
	require(denominationCount(*nestedBag, 3031) == 125 && denominationCount(*nestedBag, 3035) == 2 &&
	        denominationCount(*nestedBag, 3043) == 1, "Gold Bag preserves exact coin denominations and counts");
	require(nestedOwner->getMoney() + contentsValue(*nestedCorpse) == nestedValue && contentsValue(*nestedCorpse) == 0,
	        "nested transfer conserves total currency value");
	require(EasyMode::collectGold(*nestedOwner) == 0 && nestedOwner->getMoney() == nestedValue,
	        "repeated Gold Bag collection never duplicates value");

	PropWriteStream attributes;
	nestedBag->serializeAttr(attributes);
	size_t length;
	const char* serialized = attributes.getStream(length);
	PropStream input;
	input.init(serialized, length);
	Item* restored = Item::CreateItem(bagId);
	require(restored->unserializeAttr(input) && restored->getActionId() == bagActionId && restored->getContainer(),
	        "Gold Bag marker survives normal item attribute serialization");
	restored->decrementReferenceCounter();

	Player* invalidOwner = makePlayer(98, 94);
	Container* wrongType = carry(*invalidOwner, 2853, bagActionId)->getContainer();
	Container* invalidRoot = backpack(*invalidOwner);
	Container* unmarked = Item::CreateItem(bagId)->getContainer();
	require(g_game.internalAddItem(invalidRoot, unmarked) == RETURNVALUE_NOERROR, "carry ordinary unmarked present");
	Container* wrongMarker = markedBag(*invalidRoot);
	wrongMarker->setActionId(65004);
	Item* nonContainer = Item::CreateItem(3264);
	nonContainer->setActionId(bagActionId);
	require(g_game.internalAddItem(invalidRoot, nonContainer) == RETURNVALUE_NOERROR, "carry noncontainer with Gold Bag Action ID");
	Item* fallbackStack = addCoin(*invalidRoot, 3031, 95);
	Container* invalidCorpse = corpseAt(98, 95);
	addCoin(*invalidCorpse, 3031, 10);
	const uint64_t invalidValue = invalidOwner->getMoney() + contentsValue(*invalidCorpse);
	EasyMode::onLootGenerated(*invalidCorpse);
	require(EasyMode::collectGold(*invalidOwner) == 10 && fallbackStack->getItemCount() == 100,
	        "invalid Gold Bag pairs retain Wave 1 merge/fallback behavior");
	require(contentsValue(*wrongType) == 0 && contentsValue(*unmarked) == 0 && contentsValue(*wrongMarker) == 0,
	        "wrong item, missing marker, and wrong marker receive no dedicated priority");
	require(invalidOwner->getMoney() + contentsValue(*invalidCorpse) == invalidValue,
	        "invalid-pair fallback conserves currency");

	Player* multipleOwner = makePlayer(106, 94);
	Container* deep = carry(*multipleOwner, bagId, bagActionId)->getContainer();
	Container* shallow = markedBag(*backpack(*multipleOwner));
	Item* multipleOrdinary = addCoin(*backpack(*multipleOwner), 3031, 95);
	Container* firstMarked = nullptr;
	for (Item* item : EasyMode::getCarriedItems(*multipleOwner)) {
		if (item->getID() == bagId && item->getActionId() == bagActionId) {
			firstMarked = item->getContainer();
			break;
		}
	}
	require(firstMarked == shallow, "carried breadth-first traversal selects shallower marked bag");
	for (int iteration = 0; iteration < 2; ++iteration) {
		Container* multipleCorpse = corpseAt(106, 95);
		addCoin(*multipleCorpse, 3031, 10);
		EasyMode::onLootGenerated(*multipleCorpse);
		require(EasyMode::collectGold(*multipleOwner) == 10, "multiple-bag collection transfers each actual stack once");
	}
	require(contentsValue(*shallow) == 20 && contentsValue(*deep) == 0 && multipleOrdinary->getItemCount() == 95,
	        "multiple marked bags deterministically prefer first BFS match across collections");

	Player* limitedOwner = makePlayer(90, 105);
	Container* limitedBag = carry(*limitedOwner, bagId, bagActionId)->getContainer();
	Item* heavy = Item::CreateItem(3264);
	heavy->setIntAttr(ITEM_ATTRIBUTE_WEIGHT, limitedOwner->getFreeCapacity() - 50);
	require(g_game.internalAddItem(limitedOwner, heavy, CONST_SLOT_RIGHT) == RETURNVALUE_NOERROR,
	        "prepare Gold Bag capacity-limited inventory");
	require(limitedOwner->getFreeCapacity() == 50, "Gold Bag owner has capacity for only five gold coins");
	Container* partialCorpse = corpseAt(90, 106);
	Item* partialCoins = addCoin(*partialCorpse, 3031, 20);
	EasyMode::onLootGenerated(*partialCorpse);
	require(EasyMode::collectGold(*limitedOwner) == 5 && contentsValue(*limitedBag) == 5 &&
	        partialCoins->getParent() == partialCorpse && partialCoins->getItemCount() == 15,
	        "Gold Bag partial transfer respects capacity and leaves actual remainder in corpse");
	require(EasyMode::collectGold(*limitedOwner) == 0 && partialCoins->getItemCount() == 15,
	        "capacity exhaustion cannot duplicate or ground-drop Gold Bag currency");
	Player* remainderOwner = makePlayer(91, 105);
	Container* remainderBag = carry(*remainderOwner, bagId, bagActionId)->getContainer();
	require(EasyMode::collectGold(*remainderOwner) == 15 && contentsValue(*remainderBag) == 15,
	        "another Gold Bag collector receives only the actual uncollected remainder");
	require(limitedOwner->getMoney() + remainderOwner->getMoney() + contentsValue(*partialCorpse) == 20 &&
	        EasyMode::collectGold(*limitedOwner) == 0 && EasyMode::collectGold(*remainderOwner) == 0,
	        "competing partial collectors conserve value under repeated processing");

	Player* tradeOwner = makePlayer(100, 105);
	Container* tradeBag = carry(*tradeOwner, bagId, bagActionId)->getContainer();
	Container* tradeRoot = backpack(*tradeOwner);
	Container* offeredSubtree = tradeBag->getParent()->getItem()->getContainer();
	Item* tradeFallback = addCoin(*tradeRoot, 3031, 95);
	Player* tradePartner = makePlayer(101, 105);
	tradeOwner->addContainer(1, tradeRoot);
	g_game.playerRequestTrade(tradeOwner->getID(),
	    Position(0xFFFF, 0x41, tradeRoot->getThingIndex(offeredSubtree)), 0, tradePartner->getID(), offeredSubtree->getID());
	require(tradeOwner->getTradeItem() == offeredSubtree, "offer marked-bag ancestor through real trade API");
	Container* tradeCorpse = corpseAt(100, 106);
	addCoin(*tradeCorpse, 3031, 10);
	EasyMode::onLootGenerated(*tradeCorpse);
	require(EasyMode::collectGold(*tradeOwner) == 10 && contentsValue(*tradeBag) == 0 && tradeFallback->getItemCount() == 100,
	        "active traded subtree is skipped and ordinary destinations remain usable");
	require(tradeOwner->getTradeItem() == offeredSubtree, "Gold Bag fallback does not alter active offered subtree");
	g_game.playerCloseTrade(tradeOwner->getID());
	Container* afterTrade = corpseAt(100, 106);
	addCoin(*afterTrade, 3031, 10);
	EasyMode::onLootGenerated(*afterTrade);
	require(EasyMode::collectGold(*tradeOwner) == 10 && contentsValue(*tradeBag) == 10,
	        "Gold Bag priority resumes after trade cancellation");

	Player* fullOwner = makePlayer(108, 105);
	Container* fullBag = carry(*fullOwner, bagId, bagActionId)->getContainer();
	Item* fullBagGold = addCoin(*fullBag, 3031, 95);
	addCoin(*fullBag, 3035, 100);
	addCoin(*fullBag, 3043, 100);
	for (int i = 0; i < 2; ++i) {
		require(g_game.internalAddItem(fullBag, Item::CreateItem(3264)) == RETURNVALUE_NOERROR,
		        "fill remaining historical Gold Bag slots with ordinary manual contents");
	}
	require(fullBag->size() == 5 && fullBag->size() == fullBag->capacity(), "Gold Bag five-slot fixture is full");
	Item* fullFallback = addCoin(*backpack(*fullOwner), 3031, 95);
	Container* fullCorpse = corpseAt(108, 106);
	addCoin(*fullCorpse, 3031, 7);
	addCoin(*fullCorpse, 3035, 2);
	const uint64_t fullBagValue = contentsValue(*fullBag);
	const uint64_t fullValue = fullOwner->getMoney() + contentsValue(*fullCorpse);
	EasyMode::onLootGenerated(*fullCorpse);
	require(EasyMode::collectGold(*fullOwner) == 207 && fullBagGold->getItemCount() == 100 &&
	        fullFallback->getItemCount() == 97 && contentsValue(*fullBag) == fullBagValue + 5 && fullBag->size() == 5,
	        "full Gold Bag fills existing partial stack then falls back to ordinary destinations");
	require(fullOwner->getMoney() + contentsValue(*fullCorpse) == fullValue && contentsValue(*fullCorpse) == 0 &&
	        EasyMode::collectGold(*fullOwner) == 0, "full-bag fallback preserves all value without denomination conversion");
	Container* completelyFullCorpse = corpseAt(108, 106);
	addCoin(*completelyFullCorpse, 3031, 7);
	EasyMode::onLootGenerated(*completelyFullCorpse);
	require(EasyMode::collectGold(*fullOwner) == 7 && fullFallback->getItemCount() == 100 &&
	        contentsValue(*fullBag) == fullBagValue + 5 && fullOwner->getMoney() == fullValue + 7,
	        "Gold Bag with no merge or slot capacity uses historical global fallback");
}

#endif
