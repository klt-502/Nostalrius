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
			if (bytes[i] == 0xB4 && bytes[i + 1] == MESSAGE_STATUS_SMALL) {
				const unsigned length = word(i + 2);
				if (length && i + 4 + length <= bytes.size())
					chat.emplace_back(bytes.begin() + i + 4, bytes.begin() + i + 4 + length);
			}
			if (bytes[i] != 0xAA || i + 7 > bytes.size())
				continue;
			const unsigned nameLength = word(i + 5);
			size_t type = i + 7 + nameLength;
			if (type + 8 > bytes.size() || std::string(bytes.begin() + i + 7, bytes.begin() + type) != "Loot Broker")
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
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 21000);
	BrokerWire(*owner).expect("Off to town!");
	require(!brokerFor(*owner), "departure speech precedes actor removal");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "advance outbound fixture");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 23000);
	BrokerWire(*owner).expect("");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "advance returning fixture");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 25000);
	BrokerWire(*owner).expect("Back from town.");
	require(brokerFor(*owner), "return speech requires a physically placed actor");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::tick(*owner, base + 27000);
	BrokerWire(*owner).expect("");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::command(*owner, "balance");
	require(!BrokerWire(*owner).chat.empty(),
	        "wire observer detects the explicit accounting reply still allowed in chat");
	Broker::stop(*owner);
}
#endif
