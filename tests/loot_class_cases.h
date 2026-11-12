#ifndef ZEDAS_LOOT_CLASS_CASES_H
#define ZEDAS_LOOT_CLASS_CASES_H
static void testLootClasses()
{
	auto add = [](Container &source, uint16_t id) {
		Item *item = Item::CreateItem(id, 1);
		require(item && g_game.internalAddItem(&source, item, INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR,
		        "add classification fixture");
		return item;
	};
	Container *corpse = corpseAt(100, 101);
	Container *bag = add(*corpse, 2853)->getContainer();
	Item *fodder = add(*bag, 3276);
	Item *djinn = add(*bag, 3370);
	Item *decorative = add(*bag, 2994);
	Item *quest = add(*bag, 3232);
	quest->setText("A coded report");
	quest->setWriter("Witness");
	quest->setDate(123456);
	quest->setSpecialDescription("Unchanged quest evidence");
	std::vector<Item *> junk;
	for (uint16_t id : {3112, 3115, 3116, 3120, 3122, 3130, 3492})
		junk.push_back(add(*bag, id));
	Item *unsafe = add(*bag, 3235);
	const auto duration = unsafe->getDuration();
	Item *marked = add(*bag, 3232);
	marked->setActionId(65001);
	Item *movement = add(*bag, 3232);
	movement->setMovementID(123);
	Container *special = add(*corpse, 3244)->getContainer();
	Item *hidden = add(*special, 3232);
	EasyMode::onLootGenerated(*corpse);
	for (Item *item : {fodder, djinn, decorative, quest})
		require(item->getParent() == corpse->getTile(), "safe generated exact instance exposed");
	require(EasyMode::lootClass(*fodder) == EasyMode::LootClass::BrokerFodder &&
	            EasyMode::lootClass(*djinn) == EasyMode::LootClass::PlayerInterest &&
	            EasyMode::lootClass(*quest) == EasyMode::LootClass::PlayerInterest,
	        "three-class policy separates routine and player interest");
	require(quest->getText() == "A coded report" && quest->getWriter() == "Witness" && quest->getDate() == 123456 &&
	            quest->getSpecialDescription() == "Unchanged quest evidence",
	        "inert quest metadata preserved without clone or rewrite");
	for (Item *item : junk)
		require(item->getParent() == bag && EasyMode::lootClass(*item) == EasyMode::LootClass::CorpseJunk,
		        "junk and fishing worms stay in generated bag");
	require(unsafe->getParent() == bag && unsafe->getDuration() == duration && marked->getParent() == bag &&
	            marked->getActionId() == 65001 && movement->getParent() == bag && movement->getMovementId() == 123 &&
	            hidden->getParent() == special,
	        "decaying/marked/movement and special subtree protections retained");
	Container escrow(2854, 20);
	for (Item *item : {djinn, decorative, quest})
		require(!EasyMode::takeBrokerItem(*item, escrow), "player-interest has no broker transfer path");
	Item *later = add(*bag, 3232);
	EasyMode::onLootGenerated(*corpse);
	EasyMode::spillLoot();
	require(later->getParent() == bag, "later quest insertion never gains generated provenance");
	Container *overflow = corpseAt(102, 101);
	std::vector<Item *> reports;
	for (unsigned i = 0; i < 9; ++i) {
		Item *item = add(*overflow, 3232);
		item->setText("unchanged");
		reports.push_back(item);
	}
	EasyMode::onLootGenerated(*overflow);
	require(overflow->size() == 1 && overflow->getTile()->getThingCount() == 10,
	        "quest metadata exception preserves ten-thing overflow limit");
	{
		std::ofstream actions("data/actions/actions.xml");
		actions << "<actions><action itemid='3232' script='misc/fluids.lua'/></actions>";
	}
	require(g_actions->reload(), "register unrelated quest use handler");
	Container *changed = corpseAt(104, 101);
	Item *scripted = add(*changed, 3232);
	EasyMode::onLootGenerated(*changed);
	require(scripted->getParent() == changed, "custom script registration defeats exact identity allowance");
}
#endif
