#ifndef ZEDAS_BROKER_FEEDBACK_CASES_H
#define ZEDAS_BROKER_FEEDBACK_CASES_H
struct BrokerWire {
	std::vector<std::pair<SpeakClasses, std::string>> speech;
	std::vector<std::string> chat;
	explicit BrokerWire(Player &player)
	{
		const auto bytes = BrokerEngineTestAccess::output(player);
		auto word = [&](size_t at) { return unsigned(bytes[at]) + (unsigned(bytes[at + 1]) << 8); };
		for (size_t i = 0; i + 4 <= bytes.size(); ++i) {
			if (bytes[i] == 0xB4 && (bytes[i + 1] == MESSAGE_STATUS_SMALL || bytes[i + 1] == MESSAGE_STATUS_DEFAULT)) {
				const unsigned length = word(i + 2);
				if (length && i + 4 + length <= bytes.size())
					chat.emplace_back(bytes.begin() + i + 4, bytes.begin() + i + 4 + length);
			}
			if (bytes[i] != 0xAA || i + 7 > bytes.size())
				continue;
			const unsigned nameLength = word(i + 5);
			size_t type = i + 7 + nameLength;
			if (type + 8 > bytes.size() ||
			    (std::string(bytes.begin() + i + 7, bytes.begin() + type) != "Loot Broker" &&
			     std::string(bytes.begin() + i + 7, bytes.begin() + type) != "Broker Runner"))
				continue;
			// Classic positional speech is mode + five-byte position + text.
			const unsigned length = word(type + 6);
			if (type + 8 + length <= bytes.size())
				speech.emplace_back(static_cast<SpeakClasses>(bytes[type]),
				                    std::string(bytes.begin() + type + 8, bytes.begin() + type + 8 + length));
		}
	}
	void expect(const std::string &text) const
	{
		require(chat.empty(), "routine broker feedback never uses a chat/status text packet");
		if (text.empty())
			require(speech.empty(), "throttled/absent broker emits no speech");
		else
			require(speech.size() == 1 && speech[0].first == TALKTYPE_MONSTER_SAY && speech[0].second == text,
			        "classic floating monster-noise packet carries exact broker speech");
	}
};
static void testBrokerFeedback(bool disabled)
{
	Player *owner = makePlayer(100, 100);
	owner->setGUID(1);
	owner->setBankBalance(100);
	if (disabled) {
		require(!Broker::enabled(), "feedback fixture broker disabled");
		Broker::initialize();
		Container *corpse = corpseAt(99, 100);
		Item *item = Item::CreateItem(3276);
		require(g_game.internalAddItem(corpse, item) == RETURNVALUE_NOERROR, "disabled generated fixture");
		BrokerEngineTestAccess::clearOutput(*owner);
		EasyMode::onLootGenerated(*corpse);
		Broker::tick(*owner, OTSYS_TIME() + 60000);
		require(item->getParent() == corpse->getTile() && !brokerFor(*owner),
		        "disabled broker leaves ordinary spill unchanged");
		BrokerWire(*owner).expect("");
		return;
	}
	require(Database::getInstance()->connect(), "feedback isolated DB fixture");
	Database *db = Database::getInstance();
	for (const char *table :
	     {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
		require(db->executeQuery(std::string("DELETE FROM ") + table), "reset feedback ledger fixture");
	Broker::initialize();
	Broker::command(*owner, "hire");
	require(brokerFor(*owner), "feedback actor hired");
	const uint16_t ids[] = {3276, 3354, 3350};
	const Position positions[] = {{99, 100, 7}, {98, 99, 7}, {98, 100, 7}};
	std::vector<Item *> items;
	for (unsigned i = 0; i < 3; ++i) {
		Container *corpse = corpseAt(positions[i].x, positions[i].y);
		Item *item = Item::CreateItem(ids[i]);
		require(g_game.internalAddItem(corpse, item) == RETURNVALUE_NOERROR, "feedback generated loot");
		EasyMode::onLootGenerated(*corpse);
		items.push_back(item);
	}
	const int64_t base = OTSYS_TIME();
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 1100);
	BrokerWire(*owner).expect("Got a hatchet.");
	require(items[0]->getParent()->getItem()->getContainer(), "speech follows actual pickup");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 2100);
	BrokerWire(*owner).expect("");
	require(items[1]->getParent()->getItem()->getContainer(), "throttle suppresses speech, not actual collection");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 4200);
	BrokerWire(*owner).expect("Got a bow.");
	Npc *collector = brokerFor(*owner);
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 121000);
	BrokerWire(*owner).expect("");
	require(brokerFor(*owner) == collector && !runnerFor(*owner), "small low-value batch does not dispatch on idle");
	Broker::command(*owner, "sell");
	BrokerWire(*owner).expect("Pickup!");
	Npc *runner = runnerFor(*owner);
	require(runner && runner != collector && runner->getCurrentOutfit().lookType == 69 &&
	            !static_cast<Creature *>(runner)->isAttackable() && !runner->getMaster() && owner->getSummons().empty(),
	        "temporary native noncombatant courier uses no summon slot");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 123000);
	auto departure = BrokerWire(*owner);
	require(departure.speech.size() == 1 && departure.speech[0].first == TALKTYPE_MONSTER_SAY &&
	            departure.speech[0].second == "Off to town!" &&
	            departure.chat ==
	                std::vector<std::string>{"Broker runner left for town with 3 items (estimated 185 gp)."},
	        "departure has one floating caption and one persistent summary");
	require(brokerFor(*owner) == collector && !runnerFor(*owner), "collector remains when runner departs");
	// Only the collector can approach and claim. Put new loot adjacent to the departing runner's old tile.
	Container *newCorpse = corpseAt(99, 101);
	Item *newItem = Item::CreateItem(3276);
	require(g_game.internalAddItem(newCorpse, newItem) == RETURNVALUE_NOERROR, "new hunting loot while away");
	EasyMode::onLootGenerated(*newCorpse);
	require(g_game.internalTeleport(collector, Position(99, 100, 7)) == RETURNVALUE_NOERROR,
	        "collector approach while runner away");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 125000);
	BrokerWire(*owner).expect("Got a hatchet.");
	require(
	    newItem->getParent() != newCorpse->getTile() &&
	        db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND sold=0 AND trip IS NULL")
	                ->getNumber<unsigned>("n") == 1,
	    "collector continues new field batch while courier travels");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "advance outbound fixture");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 127000);
	auto sale = BrokerWire(*owner);
	require(
	    sale.speech.empty() &&
	        sale.chat ==
	            std::vector<std::string>{"Broker sale complete: 185 gp gross. Your share: 139 gp. Broker fund: 46 gp."},
	    "only confirmed sale emits one persistent accounting summary");
	require(
	    newItem->getParent() &&
	        db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND sold=0 AND trip IS NULL")
	                ->getNumber<unsigned>("n") == 1,
	    "new field item is not sold with older courier batch");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "advance returning fixture");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 129000);
	BrokerWire(*owner).expect("");
	require(brokerFor(*owner) == collector && !runnerFor(*owner), "courier completes return off-scene");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::command(*owner, "hire");
	auto status = BrokerWire(*owner);
	require(status.speech.empty() &&
	            status.chat ==
	                std::vector<std::string>{
	                    "Broker is with you. Carrying 1/20 items. Broker has 139 gp waiting for cashout."},
	        "repeated hire reports live status without creating an actor or repeating notices");
	require(brokerFor(*owner) == collector && !runnerFor(*owner), "repeated hire preserves one collector");

	Broker::stop(*owner);
}
#endif
