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
static void testBrokerFloor()
{
	require(Database::getInstance()->connect(), "floor fixture DB");
	auto *db = Database::getInstance();
	for (const char *table :
	     {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
		require(db->executeQuery(std::string("DELETE FROM ") + table), "reset floor fixture");
	const auto ground = g_game.map.getTile(100, 100, 7)->getGround()->getID();
	for (int z : {5, 6})
		for (int x = 98; x <= 102; ++x)
			for (int y = 98; y <= 102; ++y) {
				auto *tile = new StaticTile(x, y, z);
				tile->internalAddThing(Item::CreateItem(ground));
				g_game.map.setTile(Position(x, y, z), tile);
			}
	Broker::initialize();
	auto *owner = makePlayer(100, 100);
	owner->setGUID(1);
	Broker::command(*owner, "hire");
	Container *corpse = corpseAt(99, 100);
	Item *helmet = Item::CreateItem(3355);
	require(g_game.internalAddItem(corpse, helmet) == RETURNVALUE_NOERROR, "floor escrow fixture");
	EasyMode::onLootGenerated(*corpse);
	Broker::tick(*owner, OTSYS_TIME() + 1100);
	Broker::command(*owner, "sell");
	require(brokerFor(*owner) && runnerFor(*owner), "floor change starts during handoff");
	require(g_game.internalTeleport(owner, Position(100, 100, 6)) == RETURNVALUE_NOERROR, "first floor change");
	const auto first = OTSYS_TIME();
	require(!brokerFor(*owner) && !runnerFor(*owner), "floor hook immediately removes both visible actors");
	Broker::tick(*owner, first + 1000);
	require(!brokerFor(*owner), "collector absent before stable-floor delay");
	BrokerEngineTestAccess::clearOutput(*owner);
	Broker::command(*owner, "hire");
	require(!brokerFor(*owner) && BrokerWire(*owner).chat.back().find("Broker is paused after a floor change.") == 0,
	        "hire reports pending floor rejoin without bypassing delay");
	// Repeated floor changes reset the due time; no old-floor callbacks or loot claims.
	require(g_game.internalTeleport(owner, Position(100, 100, 5)) == RETURNVALUE_NOERROR, "second floor change");
	const auto second = OTSYS_TIME();
	Broker::tick(*owner, second + 1000);
	require(!brokerFor(*owner), "changed floor resets delay");
	for (int x = 99; x <= 101; ++x)
		for (int y = 99; y <= 101; ++y)
			if (x != 100 || y != 100)
				g_game.map.getTile(x, y, 5)->setFlag(TILESTATE_BLOCKSOLID);
	Broker::tick(*owner, second + 2000);
	require(!brokerFor(*owner), "no safe placement leaves collector absent");
	g_game.map.getTile(99, 99, 5)->resetFlag(TILESTATE_BLOCKSOLID);
	Broker::tick(*owner, second + 3000);
	require(brokerFor(*owner) && brokerFor(*owner)->getPosition() == Position(99, 99, 5) && !runnerFor(*owner),
	        "delayed safe adjacent reappearance with active trip off-scene");
	auto *collector = brokerFor(*owner);
	Broker::command(*owner, "hire");
	require(brokerFor(*owner) == collector, "hire after floor rejoin is idempotent");
	require(
	    db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND trip IS NOT NULL AND sold=0")
	            ->getNumber<unsigned>("n") == 1,
	    "floor changes preserve trip batch");
	Container *oldFloor = Item::CreateItem(3994)->getContainer();
	require(g_game.internalAddItem(g_game.map.getTile(101, 100, 6), oldFloor) == RETURNVALUE_NOERROR,
	        "old-floor corpse");
	Item *brass = Item::CreateItem(3354);
	require(g_game.internalAddItem(oldFloor, brass) == RETURNVALUE_NOERROR, "old-floor brass helmet");
	EasyMode::onLootGenerated(*oldFloor);
	Broker::tick(*owner, second + 4000);
	require(brass->getParent() == oldFloor->getTile(), "no cross-floor remote claims");
	require(g_game.internalTeleport(owner, Position(100, 100, 6)) == RETURNVALUE_NOERROR, "return to witnessed floor");
	const auto third = OTSYS_TIME();
	Broker::tick(*owner, third + 1000);
	require(brass->getParent() == oldFloor->getTile() && !brokerFor(*owner), "no collection while absent");
	Broker::tick(*owner, third + 2000);
	collector = brokerFor(*owner);
	require(collector && collector->getPosition().z == 6, "collector returns on correct floor");
	require(g_game.internalTeleport(collector, Position(100, 99, 6)) == RETURNVALUE_NOERROR,
	        "floor native pickup endpoint");
	Broker::tick(*owner, third + 3000);
	require(brass->getParent() != oldFloor->getTile(), "collection resumes locally while original courier still away");
	require(g_game.internalTeleport(owner, Position(100, 100, 5)) == RETURNVALUE_NOERROR, "floor change before death");
	owner->changeHealth(-owner->getHealth());
	Broker::tick(*owner, OTSYS_TIME() + 3000);
	require(!brokerFor(*owner) && !runnerFor(*owner), "dead owner cannot spawn delayed actors");
}
static void testBrokerBatch()
{
	require(Database::getInstance()->connect(), "batch fixture DB");
	auto *db = Database::getInstance();
	for (const char *table :
	     {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
		require(db->executeQuery(std::string("DELETE FROM ") + table), "reset batch fixture");
	Broker::initialize();
	auto *owner = makePlayer(100, 100);
	owner->setGUID(1);
	Broker::command(*owner, "hire");
	auto *collector = brokerFor(*owner);
	unsigned generated = 0;
	auto generate = [&]() {
		const int offsets[][2] = {{98, 98}, {98, 99}, {98, 100}, {99, 98}, {99, 100}, {100, 98}, {100, 99}, {100, 100}};
		const auto *pos = offsets[generated++ % 8];
		auto *corpse = corpseAt(pos[0], pos[1]);
		auto *item = Item::CreateItem(3276);
		require(g_game.internalAddItem(corpse, item) == RETURNVALUE_NOERROR, "generated batch hatchet");
		EasyMode::onLootGenerated(*corpse);
		require(item->getParent() == corpse->getTile(), "batch loot actually spilled");
		return item;
	};
	auto base = OTSYS_TIME();
	auto *single = generate();
	Broker::tick(*owner, base + 1100);
	require(single->getParent() != single->getTile(), "single low-value item collected");
	Broker::tick(*owner, base + Broker::AUTO_BATCH_IDLE_MS + 2000);
	require(!runnerFor(*owner) && brokerFor(*owner) == collector &&
	            db->storeQuery("SELECT state FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("state") ==
	                0,
	        "one cheap hatchet never triggers automatic idle trip");
	int64_t now = base + Broker::AUTO_BATCH_IDLE_MS + 3000;
	for (unsigned i = 1; i < Broker::AUTO_BATCH_MIN_INSTANCES; ++i) {
		generate();
		Broker::tick(*owner, now);
		now += 1000;
	}
	const int64_t last = now - 1000;
	Broker::tick(*owner, last + Broker::AUTO_BATCH_IDLE_MS - 1000);
	require(!runnerFor(*owner), "minimum count and gross still require full idle interval");
	Broker::tick(*owner, last + Broker::AUTO_BATCH_IDLE_MS);
	auto *runner = runnerFor(*owner);
	require(runner && brokerFor(*owner) == collector, "qualified idle batch dispatches runner only");
	require(
	    db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND trip IS NOT NULL AND sold=0")
	            ->getNumber<unsigned>("n") == 10,
	    "idle batch assigns exactly ten instances");
	Broker::command(*owner, "sell");
	require(runnerFor(*owner) == runner, "explicit sell while busy cannot create second runner");
	auto *fresh = generate();
	auto *source = fresh->getParent();
	const auto rowsBefore =
	    db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1")->getNumber<unsigned>("n");
	BrokerEngineTestAccess::think(*runner);
	require(
	    fresh->getParent() == source &&
	        db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1")->getNumber<unsigned>("n") ==
	            rowsBefore,
	    "courier think cannot collect adjacent generated ground loot");
	now = last + Broker::AUTO_BATCH_IDLE_MS + 1000;
	Broker::tick(*owner, now);
	now += 1000;
	for (unsigned i = 1; i < Broker::INVENTORY_LIMIT; ++i) {
		generate();
		Broker::tick(*owner, now);
		now += 1000;
	}
	require(
	    brokerFor(*owner) == collector &&
	        db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND trip IS NULL AND sold=0")
	                ->getNumber<unsigned>("n") == 20,
	    "field fills independent twenty slots while runner travels");
	auto *overflow = generate();
	source = overflow->getParent();
	Broker::tick(*owner, now);
	now += 1000;
	require(overflow->getParent() == source && !Broker::locked(overflow),
	        "full field leaves excess loot visible and unreserved");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "finish first outbound");
	Broker::tick(*owner, now);
	now += 1000;
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "finish first return");
	Broker::tick(*owner, now);
	require(runnerFor(*owner) && brokerFor(*owner) == collector &&
	            db->storeQuery("SELECT COUNT(DISTINCT trip) AS n FROM zedas_broker_escrow WHERE owner=1")
	                    ->getNumber<unsigned>("n") == 2 &&
	            db->storeQuery(
	                  "SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND trip IS NOT NULL AND sold=0")
	                    ->getNumber<unsigned>("n") == 20,
	        "full field dispatches immediately into a distinct batch when courier slot becomes available");
	Broker::stop(*owner);
}
static void testBrokerNotices()
{
	require(Database::getInstance()->connect(), "notice fixture DB");
	auto *db = Database::getInstance();
	for (const char *table :
	     {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
		require(db->executeQuery(std::string("DELETE FROM ") + table), "reset notice fixture");
	Broker::Connection settings;
	settings.host = "127.0.0.1";
	settings.user = "root";
	settings.password = "fixture-only";
	settings.database = "zedas_broker_test";
	settings.port = g_config.getNumber(ConfigManager::SQL_PORT);
	auto *faults = new BrokerFaultConnection(settings);
	Broker::initialize(std::unique_ptr<Broker::Store>(faults));
	auto *owner = makePlayer(100, 100);
	owner->setGUID(1);
	Broker::command(*owner, "hire");
	auto saleMessages = [&]() {
		const auto messages = BrokerWire(*owner).chat;
		return std::count_if(messages.begin(), messages.end(),
		                     [](const std::string &s) { return s.find("Broker sale complete:") == 0; });
	};
	int64_t now = OTSYS_TIME() + 1100;
	for (unsigned trial = 0; trial < 3; ++trial) {
		if (trial) {
			require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"),
			        "finish prior return");
			Broker::tick(*owner, now);
			now += 2000;
			Broker::command(*owner, "hire");
		}
		auto *corpse = corpseAt(99, 100);
		auto *helmet = Item::CreateItem(3355);
		require(g_game.internalAddItem(corpse, helmet) == RETURNVALUE_NOERROR, "notice generated helmet");
		EasyMode::onLootGenerated(*corpse);
		Broker::tick(*owner, now);
		now += 2000;
		Broker::command(*owner, "sell");
		Broker::tick(*owner, now);
		now += 2000;
		BrokerEngineTestAccess::clearOutput(*owner);
		require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"),
		        "notice advance outbound");
		faults->stage = trial == 2 ? "notice" : "sale";
		faults->failure =
		    trial == 0 ? BrokerFaultConnection::Failure::BeforeCommit : BrokerFaultConnection::Failure::AfterCommit;
		Broker::tick(*owner, now);
		now += 2000;
		require(saleMessages() == 0, "failed/unacknowledged commit never emits sale completion prematurely");
		if (trial == 0) {
			require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_sales WHERE owner=1")
			                ->getNumber<unsigned>("n") == 0,
			        "rolled-back sale has no ledger or completion notice");
			BrokerEngineTestAccess::clearOutput(*owner);
			Broker::command(*owner, "hire"); // Reconcile, settle successfully and then announce.
		} else {
			BrokerEngineTestAccess::clearOutput(*owner);
			Broker::command(*owner, "balance"); // Only committed ledger is eligible.
		}
		require(saleMessages() == (trial == 2 ? 0 : 1),
		        "confirmed sale announces once, lost notice acknowledgement emits nothing");
		require(db->storeQuery("SELECT sale_notified FROM zedas_broker_accounts WHERE owner=1")
		                ->getNumber<unsigned>("sale_notified") == 1,
		        "sale notice durable claim survives reconciliation");
		BrokerEngineTestAccess::clearOutput(*owner);
		Broker::command(*owner, "balance");
		Broker::command(*owner, "hire");
		Broker::tick(*owner, now);
		now += 2000;
		require(saleMessages() == 0, "replayed settlement/hire/status cannot duplicate summary");
		require(
		    db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_sales WHERE owner=1")->getNumber<unsigned>("n") ==
		        trial + 1,
		    "one committed ledger per trial batch");
	}
	Broker::stop(*owner);
}
#endif
