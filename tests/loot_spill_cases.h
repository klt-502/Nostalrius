#ifndef ZEDAS_LOOT_SPILL_CASES_H
#define ZEDAS_LOOT_SPILL_CASES_H

// Separate spill_enabled scenario: real item definitions/actions and manifest,
// Easy Mode/loot spill/auto-gold enabled. Item 3410 has a test-only item action.
static void testLootSpill()
{
	auto addGenerated = [](Container& container, uint16_t id, uint16_t count = 1) {
		Item* item = Item::CreateItem(id, count);
		require(item != nullptr, "create generated spill fixture");
		// Also exercise overfull generated corpse trees without changing production limits.
		require(g_game.internalAddItem(&container, item, INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR,
		        "add generated spill fixture");
		return item;
	};
	auto blockSpillTile = [](Container& corpse) {
		Tile* tile = corpse.getTile();
		std::vector<Item*> blockers;
		while (tile->getThingCount() < 10) {
			Item* filler = Item::CreateItem(3416); // Inert ordinary down item, not a scripted/interactive weapon.
			require(g_game.internalAddItem(tile, filler, INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR,
			        "fill client-visible tile slots");
			blockers.push_back(filler);
		}
		require(tile->getThingCount() == 10, "spill overflow fixture has ten tile things");
		return blockers;
	};
	auto containsPointer = [](Tile& tile, Item* sought) {
		const TileItemVector* items = tile.getItemList();
		return items && std::find(items->begin(), items->end(), sought) != items->end();
	};

	Container* single = corpseAt(88, 92);
	Item* armor = addGenerated(*single, 3357);
	Tile* singleTile = single->getTile();
	EasyMode::onLootGenerated(*single);
	require(armor->getParent() == singleTile && containsPointer(*singleTile, armor) && single->empty(),
	        "single ordinary armor spills as the actual existing item");
	require(singleTile->getTopDownItem() == armor, "spilled armor is placed above the corpse");
	EasyMode::onLootGenerated(*single);
	EasyMode::spillLoot();
	require(singleTile->getThingCount() == 3 && armor->getParent() == singleTile,
	        "repeated notification and spill do not duplicate simple loot");
	Item* inserted = addGenerated(*single, 3416);
	EasyMode::onLootGenerated(*single);
	EasyMode::spillLoot();
	require(inserted->getParent() == single, "later player-inserted corpse items never enter the generated snapshot");

	Container* multiple = corpseAt(90, 92);
	Tile* multipleTile = multiple->getTile();
	Item* oldGem = Item::CreateItem(3028, 80);
	require(g_game.internalAddItem(multipleTile, oldGem, INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR,
	        "place preexisting ground gem stack");
	Item* shield = addGenerated(*multiple, 3416);
	Item* rareArmor = addGenerated(*multiple, 3366);
	Item* gems = addGenerated(*multiple, 3028, 70);
	EasyMode::onLootGenerated(*multiple);
	for (Item* item : {shield, rareArmor, gems}) {
		require(item->getParent() == multipleTile && containsPointer(*multipleTile, item),
		        "multiple and rare ordinary equipment preserve instance identity on ground");
	}
	require(gems->getItemCount() == 70 && oldGem->getItemCount() == 80 && oldGem->getParent() == multipleTile,
	        "complete stack spill preserves both identities and counts without merging or splitting");
	require(multipleTile->getThingCount() == 6 && multiple->empty(), "multiple generated items appear once each");

	Container* nested = corpseAt(92, 92);
	Container* generatedBag = addGenerated(*nested, 2853)->getContainer();
	Container* deepBag = addGenerated(*generatedBag, 2853)->getContainer();
	Item* chainArmor = addGenerated(*deepBag, 3358);
	Item* rubies = addGenerated(*generatedBag, 3030, 17);
	Container* protectedBag = addGenerated(*nested, 2853)->getContainer();
	protectedBag->setActionId(65001);
	Item* protectedChild = addGenerated(*protectedBag, 3416);
	EasyMode::onLootGenerated(*nested);
	require(chainArmor->getParent() == nested->getTile() && rubies->getParent() == nested->getTile() &&
	        rubies->getItemCount() == 17 && deepBag->empty(), "ordinary generated nested bags expose their actual contents");
	require(generatedBag->getParent() == nested && deepBag->getParent() == generatedBag &&
	        protectedBag->getParent() == nested && protectedChild->getParent() == protectedBag,
	        "generated containers remain and marked ancestor protects its whole subtree");

	Container* overflow = corpseAt(94, 92);
	makePlayer(94, 92); // Occupying creatures also consume protocol stack positions.
	Item* overflowArmor = addGenerated(*overflow, 3357);
	Item* overflowShield = addGenerated(*overflow, 3416);
	std::vector<Item*> blockers = blockSpillTile(*overflow);
	Tile* overflowTile = overflow->getTile();
	EasyMode::onLootGenerated(*overflow);
	require(overflowArmor->getParent() == overflow && overflowShield->getParent() == overflow,
	        "ten visible tile things leave eligible loot in corpse");
	require(g_game.internalRemoveItem(blockers.back()) == RETURNVALUE_NOERROR, "free one visible tile slot");
	blockers.pop_back();
	require(overflowTile->getThingCount() == 9, "overflow retry begins at nine visible things");
	EasyMode::spillLoot();
	const int firstExposed = (overflowArmor->getParent() == overflowTile) + (overflowShield->getParent() == overflowTile);
	require(firstExposed == 1 && overflowTile->getThingCount() == 10 && overflow->size() == 1,
	        "retry exposes exactly one existing item at the nine-to-ten boundary");
	EasyMode::spillLoot();
	require(overflowTile->getThingCount() == 10 && overflow->size() == 1, "repeated overflow retry cannot exceed visible limit");
	require(g_game.internalRemoveItem(blockers.back()) == RETURNVALUE_NOERROR, "free second visible tile slot");
	EasyMode::spillLoot();
	require(overflowArmor->getParent() == overflowTile && overflowShield->getParent() == overflowTile &&
	        overflow->empty() && overflowTile->getThingCount() == 10, "remaining overflow item spills safely on later retry");

	Container* full = corpseAt(96, 92);
	std::vector<Item*> many;
	for (int i = 0; i < 12; ++i) many.push_back(addGenerated(*full, 3416));
	Item* markedFull = addGenerated(*full, 3357);
	markedFull->setActionId(65002);
	EasyMode::onLootGenerated(*full);
	unsigned groundCount = 0;
	for (Item* item : many) {
		require(item->getParent() == full || item->getParent() == full->getTile(), "full corpse preserves every existing generated instance");
		if (item->getParent() == full->getTile()) ++groundCount;
	}
	require(groundCount == 8 && full->size() == 5 && markedFull->getParent() == full && full->getTile()->getThingCount() == 10,
	        "complex overfull corpse exposes only eight available tile slots and preserves overflow/special loot");
	EasyMode::onLootGenerated(*full);
	EasyMode::spillLoot();
	require(full->size() == 5 && full->getTile()->getThingCount() == 10, "full-corpse repeated processing conserves instance count");

	Container* protectedCorpse = corpseAt(98, 92);
	std::vector<Item*> protectedItems;
	Item* marked = addGenerated(*protectedCorpse, 3357);
	marked->setActionId(65001);
	protectedItems.push_back(marked);
	Item* quest = addGenerated(*protectedCorpse, 3357);
	quest->setIntAttr(ITEM_ATTRIBUTE_CHESTQUESTNUMBER, 77);
	protectedItems.push_back(quest);
	Item* movement = addGenerated(*protectedCorpse, 3357);
	movement->setMovementID(21);
	protectedItems.push_back(movement);
	Item* written = addGenerated(*protectedCorpse, 3357);
	written->setText("special inscription");
	protectedItems.push_back(written);
	Item* charged = addGenerated(*protectedCorpse, 3357);
	charged->setCharges(1);
	protectedItems.push_back(charged);
	Item* named = addGenerated(*protectedCorpse, 3357);
	named->setStrAttr(ITEM_ATTRIBUTE_NAME, "unique armor");
	protectedItems.push_back(named);
	Item* key = addGenerated(*protectedCorpse, 2968);
	key->setIntAttr(ITEM_ATTRIBUTE_KEYNUMBER, 1234);
	protectedItems.push_back(key);
	for (uint16_t id : {3250, 3235, 3253, 2874, 3410, 3284}) protectedItems.push_back(addGenerated(*protectedCorpse, id));
	EasyMode::onLootGenerated(*protectedCorpse);
	EasyMode::spillLoot();
	for (Item* item : protectedItems) {
		require(item->getParent() == protectedCorpse, "marked/quest/key/custom/unknown/scripted/unsafe loot stays in corpse");
	}
	require(protectedCorpse->getTile()->getThingCount() == 2 && protectedCorpse->size() == protectedItems.size(),
	        "allowlisted identity never bypasses metadata and item-action safety exclusions");
	Container* questCorpse = corpseAt(112, 92);
	questCorpse->setIntAttr(ITEM_ATTRIBUTE_CHESTQUESTNUMBER, 77);
	Item* questCorpseArmor = addGenerated(*questCorpse, 3357);
	EasyMode::onLootGenerated(*questCorpse);
	require(questCorpseArmor->getParent() == questCorpse, "quest-marked root corpse protects generated descendants");
	Container* movementSource = corpseAt(110, 92);
	Item* movementHelmet = addGenerated(*movementSource, 3351);
	EasyMode::onLootGenerated(*movementSource);
	require(movementHelmet->getParent() == movementSource, "registered item movement handler excludes even a reviewed ordinary item");
	Container* positionSource = corpseAt(110, 94);
	Item* positionArmor = addGenerated(*positionSource, 3357);
	EasyMode::onLootGenerated(*positionSource);
	require(positionArmor->getParent() == positionSource, "registered position movement handler protects destination");
	Container* residentSource = corpseAt(110, 96);
	Item* resident = Item::CreateItem(3352);
	require(g_game.internalAddItem(residentSource->getTile(), resident) == RETURNVALUE_NOERROR, "place resident movement-handler item");
	Item* residentArmor = addGenerated(*residentSource, 3357);
	EasyMode::onLootGenerated(*residentSource);
	require(residentArmor->getParent() == residentSource, "resident item-tile movement handler protects destination");
	Container* hazardous = corpseAt(110, 98);
	Item* hazardArmor = addGenerated(*hazardous, 3357);
	hazardous->getTile()->setFlag(TILESTATE_TELEPORT);
	EasyMode::onLootGenerated(*hazardous);
	require(hazardArmor->getParent() == hazardous, "hazardous destination leaves generated item in corpse");

	Player* manualOwner = makePlayer(100, 93);
	carry(*manualOwner, 3160);
	Player* secondOwner = makePlayer(101, 93);
	carry(*secondOwner, 3160);
	Container* manualCorpse = corpseAt(100, 92);
	Item* manuallyTaken = addGenerated(*manualCorpse, 3416);
	Item* waitingArmor = addGenerated(*manualCorpse, 3357);
	std::vector<Item*> manualBlockers = blockSpillTile(*manualCorpse);
	EasyMode::onLootGenerated(*manualCorpse);
	Container* manualBackpack = manualOwner->getInventoryItem(CONST_SLOT_BACKPACK)->getContainer();
	require(g_game.internalMoveItem(manualCorpse, manualBackpack, INDEX_WHEREEVER, manuallyTaken, 1, nullptr) == RETURNVALUE_NOERROR,
	        "manual pickup takes existing item while automated spill is blocked");
	require(manuallyTaken->getTopParent() == manualOwner, "manual pickup preserves ownership of original instance");
	require(g_game.internalRemoveItem(manualBlockers.back()) == RETURNVALUE_NOERROR, "free manual-pickup spill retry slot");
	EasyMode::spillLoot();
	require(manuallyTaken->getTopParent() == manualOwner && waitingArmor->getParent() == manualCorpse->getTile(),
	        "retry ignores manually removed item and exposes only remaining generated loot");
	Container* secondBackpack = secondOwner->getInventoryItem(CONST_SLOT_BACKPACK)->getContainer();
	require(g_game.internalMoveItem(manualCorpse, secondBackpack, INDEX_WHEREEVER, manuallyTaken, 1, nullptr) != RETURNVALUE_NOERROR &&
	        manuallyTaken->getTopParent() == manualOwner, "competing manual collector cannot transfer an already removed instance");
	EasyMode::onLootGenerated(*manualCorpse);
	EasyMode::spillLoot();
	require(manuallyTaken->getTopParent() == manualOwner && waitingArmor->getParent() == manualCorpse->getTile(),
	        "repeated notification after manual pickup cannot duplicate either item");

	Container* moved = corpseAt(102, 92);
	Item* movedPending = addGenerated(*moved, 3357);
	blockSpillTile(*moved);
	EasyMode::onLootGenerated(*moved);
	Tile* movedDestination = g_game.map.getTile(102, 94, 7);
	require(g_game.internalMoveItem(moved->getParent(), movedDestination, INDEX_WHEREEVER, moved, 1, nullptr) == RETURNVALUE_NOERROR,
	        "move a corpse with pending overflow loot");
	EasyMode::spillLoot();
	EasyMode::spillLoot();
	require(moved->getParent() == movedDestination && movedPending->getParent() == moved && movedDestination->getThingCount() == 2,
	        "moved corpse invalidates pending spill without losing its contents");

	Container* removed = corpseAt(104, 92);
	Item* removedPending = addGenerated(*removed, 3416);
	removed->incrementReferenceCounter();
	removedPending->incrementReferenceCounter();
	Tile* removedTile = removed->getTile();
	blockSpillTile(*removed);
	EasyMode::onLootGenerated(*removed);
	require(g_game.internalRemoveItem(removed) == RETURNVALUE_NOERROR, "remove registered spill corpse");
	g_game.cleanup();
	EasyMode::spillLoot();
	require(removed->isRemoved() && removedPending->getParent() == removed && removedTile->getThingCount() == 9,
	        "removed corpse and pending-item handles survive cleanup without exposure or duplication");
	removed->decrementReferenceCounter();
	removedPending->decrementReferenceCounter();

	Container* decaying = corpseAt(106, 92);
	Item* decayPending = addGenerated(*decaying, 3357);
	decaying->incrementReferenceCounter();
	decayPending->incrementReferenceCounter();
	Tile* decayTile = decaying->getTile();
	blockSpillTile(*decaying);
	EasyMode::onLootGenerated(*decaying);
	decaying->setDecaying(DECAYING_FALSE);
	decaying->setDuration(0);
	g_game.startDecay(decaying);
	g_game.cleanup();
	EasyMode::spillLoot();
	require(decaying->isRemoved() && decayPending->getParent() == decaying && decayTile->getThingCount() == 10,
	        "real corpse decay invalidates queued spill and preserves original pending-item lifetime");
	decaying->decrementReferenceCounter();
	decayPending->decrementReferenceCounter();

	Player* goldOwner = makePlayer(108, 93);
	carry(*goldOwner, 3160);
	Player* competingGoldOwner = makePlayer(109, 93);
	carry(*competingGoldOwner, 3160);
	Container* mixed = corpseAt(108, 92);
	Container* coinBag = addGenerated(*mixed, 2853)->getContainer();
	Item* mixedArmor = addGenerated(*mixed, 3357);
	addCoin(*coinBag, 3031, 30);
	addCoin(*coinBag, 3035, 2);
	addCoin(*coinBag, 3043, 1);
	const uint64_t mixedValue = contentsValue(*mixed);
	EasyMode::onLootGenerated(*mixed);
	require(mixedArmor->getParent() == mixed->getTile() && contentsValue(*mixed) == mixedValue,
	        "simultaneous spill exposes equipment while all currency remains available to auto-gold");
	require(EasyMode::collectGold(*goldOwner) == mixedValue && mixedValue == 10230 && contentsValue(*mixed) == 0,
	        "auto-gold transfers actual nested currency after noncurrency spill");
	EasyMode::spillLoot();
	EasyMode::onLootGenerated(*mixed);
	require(EasyMode::collectGold(*goldOwner) == 0 && EasyMode::collectGold(*competingGoldOwner) == 0 &&
	        goldOwner->getMoney() == mixedValue && competingGoldOwner->getMoney() == 0 && mixedArmor->getParent() == mixed->getTile(),
	        "repeated spill and competing auto-gold preserve all currency and equipment value");

	Container* gemProvenance = corpseAt(88, 103);
	Item* originalGemStack = addGenerated(*gemProvenance, 3028, 30);
	std::vector<Item*> gemProvenanceBlockers = blockSpillTile(*gemProvenance);
	EasyMode::onLootGenerated(*gemProvenance);
	Player* gemDonor = makePlayer(88, 104);
	Item* incomingGems = carry(*gemDonor, 3028, 0, 20);
	require(g_game.internalMoveItem(incomingGems->getParent(), gemProvenance,
	    gemProvenance->getThingIndex(originalGemStack), incomingGems, 20, nullptr) == RETURNVALUE_NOERROR,
	        "manually merge unrelated gems into registered generated stack through real move API");
	require(originalGemStack->getParent() == gemProvenance && originalGemStack->getItemCount() == 50,
	        "manual gem merge preserves target identity and actual combined quantity");
	require(g_game.internalRemoveItem(gemProvenanceBlockers.back()) == RETURNVALUE_NOERROR,
	        "free a ground slot after manual gem provenance mixing");
	EasyMode::spillLoot();
	EasyMode::onLootGenerated(*gemProvenance);
	EasyMode::spillLoot();
	require(originalGemStack->getParent() == gemProvenance && originalGemStack->getItemCount() == 50 &&
	        gemProvenance->getTile()->getThingCount() == 9 && !containsPointer(*gemProvenance->getTile(), originalGemStack),
	        "mixed generated/player gem stack is permanently excluded instead of exposing inserted quantity");

	Container* coinProvenance = corpseAt(90, 103);
	Item* originalCoinStack = addCoin(*coinProvenance, 3031, 30);
	EasyMode::onLootGenerated(*coinProvenance);
	Player* coinDonor = makePlayer(90, 104);
	Item* incomingCoins = carry(*coinDonor, 3031, 0, 20);
	const uint64_t mixedCoinValue = coinDonor->getMoney() + contentsValue(*coinProvenance);
	require(g_game.internalMoveItem(incomingCoins->getParent(), coinProvenance,
	    coinProvenance->getThingIndex(originalCoinStack), incomingCoins, 20, nullptr) == RETURNVALUE_NOERROR,
	        "manually merge unrelated currency into registered generated stack through real move API");
	require(originalCoinStack->getParent() == coinProvenance && originalCoinStack->getItemCount() == 50,
	        "manual currency merge leaves one actual mixed fifty-coin stack");
	require(EasyMode::collectGold(*coinDonor) == 0 && EasyMode::collectGold(*coinDonor) == 0,
	        "auto-gold permanently skips whole mixed-provenance coin stack under repeated collection");
	EasyMode::onLootGenerated(*coinProvenance);
	EasyMode::spillLoot();
	require(EasyMode::collectGold(*gemDonor) == 0 && originalCoinStack->getParent() == coinProvenance &&
	        originalCoinStack->getItemCount() == 50 && coinDonor->getMoney() + contentsValue(*coinProvenance) == mixedCoinValue,
	        "repeat notification and competing collector preserve all mixed currency in corpse");

	Container* returnedCorpse = corpseAt(92, 103);
	Container* returnedBag = addGenerated(*returnedCorpse, 2853)->getContainer();
	Container* returnedDeepBag = addGenerated(*returnedBag, 2853)->getContainer();
	Item* returnedRubies = addGenerated(*returnedDeepBag, 3030, 12);
	Item* returnedCoins = addCoin(*returnedDeepBag, 3031, 9);
	std::vector<Item*> returnedBlockers = blockSpillTile(*returnedCorpse);
	EasyMode::onLootGenerated(*returnedCorpse);
	Player* returnedOwner = makePlayer(92, 104);
	carry(*returnedOwner, 3160);
	Container* returnedBackpack = returnedOwner->getInventoryItem(CONST_SLOT_BACKPACK)->getContainer();
	require(g_game.internalMoveItem(returnedCorpse, returnedBackpack, INDEX_WHEREEVER, returnedBag, 1, nullptr) == RETURNVALUE_NOERROR,
	        "manually remove registered generated bag with nested gem and currency descendants");
	require(g_game.internalMoveItem(returnedBackpack, returnedCorpse, INDEX_WHEREEVER, returnedBag, 1, nullptr) == RETURNVALUE_NOERROR,
	        "manually return same generated bag to original corpse");
	require(g_game.internalRemoveItem(returnedBlockers.back()) == RETURNVALUE_NOERROR,
	        "free ground slot after generated container out-and-back movement");
	EasyMode::spillLoot();
	EasyMode::onLootGenerated(*returnedCorpse);
	EasyMode::spillLoot();
	require(returnedRubies->getParent() == returnedDeepBag && returnedRubies->getItemCount() == 12 &&
	        returnedCorpse->getTile()->getThingCount() == 9 && EasyMode::collectGold(*returnedOwner) == 0 &&
	        EasyMode::collectGold(*coinDonor) == 0 && returnedCoins->getParent() == returnedDeepBag && returnedCoins->getItemCount() == 9,
	        "moving a generated ancestor out and back permanently excludes all snapshot descendants from automation");
	MonsterType* deathType;
	Monster* deathMonster = makeMonster(5, 112, 105, true, true, &deathType);
	deathType->info.lookcorpse = 3994;
	LootBlock deathArmor;
	deathArmor.id = 3357;
	deathArmor.countmax = 1;
	deathArmor.chance = MAX_LOOTCHANCE + 1;
	deathType->info.lootItems.push_back(deathArmor);
	LootBlock deathGem;
	deathGem.id = 3028;
	deathGem.countmax = 20;
	deathGem.chance = MAX_LOOTCHANCE + 1;
	LootBlock deathBag;
	deathBag.id = 2853;
	deathBag.countmax = 1;
	deathBag.chance = MAX_LOOTCHANCE + 1;
	deathBag.childLoot.push_back(deathGem);
	deathType->info.lootItems.push_back(deathBag);
	getRandomGenerator().seed(12345);
	deathMonster->changeHealth(-5);
	deathMonster->onDeath();
	Container* realCorpse = nullptr;
	unsigned realArmor = 0, realGems = 0;
	for (Item* item : *g_game.map.getTile(112, 105, 7)->getItemList()) {
		if (item->getID() == 3994) realCorpse = item->getContainer();
		if (item->getID() == 3357) ++realArmor;
		if (item->getID() == 3028) ++realGems;
	}
	require(realCorpse && realArmor == 1 && realGems == 1 && realCorpse->size() == 1 &&
	        realCorpse->getItemByIndex(0)->getContainer()->empty(),
	        "real monster death spills completed direct and nested generated loot through shared post-loot hook");
}

#endif
