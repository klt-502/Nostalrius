#ifndef ZEDAS_BROKER_CASES_H
#define ZEDAS_BROKER_CASES_H
#include "broker.h"
#include "brokerstore.h"
#include "npc.h"
#include "iologindata.h"
#include "house.h"
#include "outputmessage.h"
#include <thread>

struct BrokerEngineTestAccess {
    static void think(Npc &npc) { npc.onThink(1000); }
	static void clearOutput(Player &player)
	{
		player.client->getCurrentBuffer().reset();
	}
	static std::vector<uint8_t> output(Player &player)
	{
		auto buffer = player.client->getCurrentBuffer();
		return buffer ? std::vector<uint8_t>(buffer->getBuffer() + 4, buffer->getBuffer() + 4 + buffer->getLength())
		              : std::vector<uint8_t>();
	}

	static void session(Player &player)
	{
		player.lastLoginSaved = time(nullptr);
	}
	static bool step(Npc &npc)
	{
		Direction dir;
		uint32_t flags = 0;
		return npc.getNextStep(dir, flags) && g_game.internalMoveCreature(&npc, dir, flags) == RETURNVALUE_NOERROR;
	}
};

static void testBrokerCatalog()
{
	std::string error;
	require(Broker::loadCatalog("data/zedas/broker.xml", error), error.c_str());
	Item *sword = Item::CreateItem(3264);
	Item *plate = Item::CreateItem(3357);
	for (const auto &entry : std::vector<std::pair<uint16_t, uint64_t>>{{3276, 25},
	                                                                    {3354, 30},
	                                                                    {3353, 145},
	                                                                    {3367, 66},
	                                                                    {3372, 49},
	                                                                    {3557, 115},
	                                                                    {3410, 31},
	                                                                    {3411, 16},
	                                                                    {3425, 100},
	                                                                    {3349, 160},
	                                                                    {3350, 130},
	                                                                    {3376, 20},
	                                                                    {3559, 1},
		                                                                    {3044, 100}}) {
		Item *item = Item::CreateItem(entry.first, 1);
		require(Broker::price(*item) == entry.second, "expanded ordinary price matches fixed reviewed source");
		item->decrementReferenceCounter();
	}
	require(Broker::price(*sword) == 25 && Broker::price(*plate) == 400, "reviewed unconditional Sam purchase prices");
	for (uint16_t id : {3577, 3579, 3582, 3590, 3600, 3607, 3004, 3006, 3007, 3063, 3492, 3031, 3160, 3244, 3366, 3375, 3362, 3378, 3446, 3447, 3370, 2994, 3232}) {
		Item *excluded = Item::CreateItem(id, 1);
		require(Broker::price(*excluded) == 0,
		        "currency/worm/protected/conditional/unreviewed identities have no sale price");
		excluded->decrementReferenceCounter();
	}
	sword->setActionId(65001);
	require(Broker::price(*sword) == 0, "marked approved identity has no price");
	sword->removeAttribute(ITEM_ATTRIBUTE_ACTIONID);
	pugi::xml_document file;
	require(file.load_file("data/zedas/broker.xml"), "load catalog fixture");
	file.child("broker").child("buyer").child("item").attribute("price").set_value(999999);
	file.save_file("bad-broker.xml");
	require(!Broker::loadCatalog("bad-broker.xml", error) && Broker::price(*plate) == 0,
	        "mismatched catalog fails closed atomically");
	require(file.load_file("data/zedas/broker.xml"), "reload reviewed XML for access rejection");
	file.child("broker").child("buyer").child("item").append_attribute("questRequired").set_value("true");
	file.save_file("bad-broker.xml");
	require(!Broker::loadCatalog("bad-broker.xml", error), "unknown access/quest attributes fail closed");
	require(Broker::loadCatalog("data/zedas/broker.xml", error), "restore reviewed catalog");
	{
		pugi::xml_document subset;
		require(subset.load_file("data/zedas/broker.xml"), "subset fixture source");
		auto root = subset.child("broker");
		for (auto buyer = root.first_child(); buyer;) {
			auto next = buyer.next_sibling();
			if (std::string(buyer.attribute("id").value()) != "sam")
				root.remove_child(buyer);
			buyer = next;
		}
		subset.save_file("subset-broker.xml");
		require(Broker::loadCatalog("subset-broker.xml", error) && Broker::price(*plate) == 400,
		        "configured catalog may narrow the reviewed route");
	}
	const std::string dependency = "data/npc/gen-t-furniture-jungle-s.ndb";
	std::ifstream included(dependency, std::ios::binary);
	const std::string original((std::istreambuf_iterator<char>(included)), std::istreambuf_iterator<char>());
	{
		std::ofstream edited(dependency, std::ios::app | std::ios::binary);
		edited << "\n# changed include\n";
	}
	require(!Broker::loadCatalog("data/zedas/broker.xml", error), "changed transitive NPC include fails closed");
	{
		std::ofstream restore(dependency, std::ios::binary);
		restore << original;
	}
	require(Broker::loadCatalog("data/zedas/broker.xml", error), "restore reviewed transitive source");
	std::ofstream changed("data/npc/sam.npc", std::ios::app);
	changed << "\n// changed offer\n";
	changed.close();
	require(!Broker::loadCatalog("data/zedas/broker.xml", error), "changed NPC rules fail source fingerprint");
	sword->decrementReferenceCounter();
	plate->decrementReferenceCounter();
}

static Npc *brokerFor(Player &owner)
{
	for (const auto &entry : g_game.getNpcs())
		if (entry.second->getBrokerOwner() == owner.getGUID() && !entry.second->isBrokerCourier())
			return entry.second;
	return nullptr;
}
static Npc *runnerFor(Player &owner)
{
    for (const auto &entry : g_game.getNpcs())
        if (entry.second->getBrokerOwner() == owner.getGUID() && entry.second->isBrokerCourier()) return entry.second;
    return nullptr;
}
class BrokerFaultConnection : public Broker::Store
{
  public:
	explicit BrokerFaultConnection(Broker::Connection c) : Store(std::move(c)) {}
	enum class Failure { None, BeforeCommit, AfterCommit };
	Failure failure = Failure::None;
	std::string stage = "payout";

  protected:
	void commit(const char *operation) override
	{
		const Failure selected = stage == operation ? failure : Failure::None;
		if (stage == operation)
			failure = Failure::None;
		if (selected == Failure::BeforeCommit) {
			close();
			throw std::runtime_error("fixture disconnected before COMMIT");
		}
		Broker::Store::commit(operation);
		if (selected == Failure::AfterCommit) {
			close();
			throw std::runtime_error("fixture lost COMMIT acknowledgement");
		}
	}
};
static void testBrokerRestart(const std::string &stage)
{
	require(Database::getInstance()->connect(), "connect restart fixture");
	Database *db = Database::getInstance();
	if (stage == "broker_seed") {
		for (const char *table :
		     {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
			require(db->executeQuery(std::string("DELETE FROM ") + table), "reset restart fixture");
		require(
		    db->executeQuery(
		        "UPDATE players SET balance=100,save=1,town_id=1,onlinetime=0,lastlogin=UNIX_TIMESTAMP() WHERE id=1"),
		    "restart bank baseline");
	}
	require(g_game.groups.load(), "restart actual groups");
	Town *town = new Town(1);
	town->setTemplePos(Position(100, 100, 7));
	g_game.map.towns.addTown(1, town);
	Player *owner = makePlayer(100, 100);
	owner->setGUID(1);
	owner->setTown(town);
	require(IOLoginData::loadPlayerById(owner, 1), "restart actual login");
	BrokerEngineTestAccess::session(*owner);
	Broker::Connection settings;
	settings.host = "127.0.0.1";
	settings.user = "root";
	settings.password = "fixture-only";
	settings.database = "zedas_broker_test";
	settings.port = g_config.getNumber(ConfigManager::SQL_PORT);
	BrokerFaultConnection *faults = new BrokerFaultConnection(settings);
	Broker::initialize(std::unique_ptr<Broker::Store>(faults));
	if (stage == "broker_seed") {
		Broker::command(*owner, "hire");
		Container *corpse = corpseAt(100, 101);
		Item *helmet = Item::CreateItem(3355);
		require(g_game.internalAddItem(corpse, helmet) == RETURNVALUE_NOERROR, "restart generated helmet");
		EasyMode::onLootGenerated(*corpse);
		require(g_game.internalTeleport(brokerFor(*owner), Position(99, 100, 7)) == RETURNVALUE_NOERROR,
		        "restart physical approach");
		faults->stage = "claim";
		faults->failure = BrokerFaultConnection::Failure::AfterCommit;
		Broker::tick(*owner, OTSYS_TIME() + 6000);
		require(Broker::locked(helmet) && helmet->getParent() == corpse->getTile(),
		        "restart after durable prepare before physical detach");
		std::_Exit(0);
	}
	Broker::command(*owner, "hire");
    if (stage == "broker_handoff_restart") {
        faults->stage = "depart";
        faults->failure = BrokerFaultConnection::Failure::AfterCommit;
        Broker::command(*owner, "sell");
        require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND sold=0 AND trip IS NOT NULL")->getNumber<unsigned>("n") == 1 &&
                db->storeQuery("SELECT state FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("state") == 1,
                "restart after atomic handoff commit with unknown acknowledgement");
        std::_Exit(0);
    }
    if (stage == "broker_travel_restart") {
        require(brokerFor(*owner) && !runnerFor(*owner), "restart resumes collector and existing trip off-scene");
        require(db->storeQuery("SELECT departure_notified FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("departure_notified") == 1,
                "recovered departure notification durably claimed");
        Container *corpse = corpseAt(99, 100);
        Item *hatchet = Item::CreateItem(3276);
        require(g_game.internalAddItem(corpse, hatchet) == RETURNVALUE_NOERROR, "travel restart hunting loot");
        EasyMode::onLootGenerated(*corpse);
        Broker::tick(*owner, OTSYS_TIME() + 2000);
        require(hatchet->getParent() != corpse->getTile() &&
                db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND sold=0 AND trip IS NULL")->getNumber<unsigned>("n") == 1,
                "new field batch persists independently during travel");
        std::_Exit(0);
    }
	if (stage == "broker_sale_restart") {
		require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE sold=0 AND trip IS NOT NULL AND owner=1")
		                ->getNumber<unsigned>("n") == 1,
		        "restart recovers one prepared image");
		Broker::command(*owner, "sell");
		require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"),
		        "restart advance outbound");
		faults->stage = "sale";
		faults->failure = BrokerFaultConnection::Failure::AfterCommit;
		Broker::tick(*owner, OTSYS_TIME() + 8000);
		require(
		    db->storeQuery("SELECT pending FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("pending") ==
		        3,
		    "restart after sale commit before consumption acknowledgement");
		std::_Exit(0);
	}
	require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE sold=0 AND trip IS NOT NULL AND owner=1")
                    ->getNumber<unsigned>("n") == 0 &&
            db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE sold=0 AND trip IS NULL AND owner=1")->getNumber<unsigned>("n") == 1,
            "sold trip never restores, independent field batch survives restart");
    require(db->storeQuery("SELECT sale_notified FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("sale_notified") == 1,
            "confirmed recovered sale summary is durably claimed once");
	if (stage == "broker_payout_restart") {
		faults->stage = "payout";
		faults->failure = BrokerFaultConnection::Failure::AfterCommit;
		Broker::command(*owner, "cashout");
		require(owner->getBankBalance() == 100 &&
		            db->storeQuery("SELECT balance FROM players WHERE id=1")->getNumber<unsigned>("balance") == 103,
		        "restart after payout commit before live memory sync");
		std::_Exit(0);
	}
	require(stage == "broker_final_restart" && owner->getBankBalance() == 103 && owner->getBrokerBankWatermark() > 0,
	        "restart loads posted payout consistently");
	Broker::command(*owner, "cashout");
	Broker::command(*owner, "cashout");
	require(IOLoginData::savePlayer(owner) && IOLoginData::savePlayer(owner), "restart repeated actual player save");
	require(
	    db->storeQuery("SELECT balance FROM players WHERE id=1")->getNumber<unsigned>("balance") == 103 &&
	        db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_sales WHERE owner=1")->getNumber<unsigned>("n") ==
	            1 &&
	        db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_payouts WHERE owner=1")->getNumber<unsigned>("n") ==
	            1 &&
	        db->storeQuery("SELECT funds FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("funds") == 1,
	    "process restart conserves one item, one sale, one payout and retained commission");
	Broker::stop(*owner);
}
static void testBroker()
{
	require(Database::getInstance()->connect(), "connect only to disposable fixture");
	Database *db = Database::getInstance();
	for (const char *table :
	     {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
		require(db->executeQuery(std::string("DELETE FROM ") + table), "reset disposable broker fixture");
	require(db->executeQuery("UPDATE players SET balance=100,save=1,town_id=1,onlinetime=0 WHERE id IN(1,2)"),
	        "reset fixture bank");
	require(g_game.groups.load(), "load actual groups for player persistence");
	Town *town = new Town(1);
	town->setTemplePos(Position(100, 100, 7));
	g_game.map.towns.addTown(1, town);
	Broker::Connection settings;
	settings.host = "127.0.0.1";
	settings.user = "root";
	settings.password = "fixture-only";
	settings.database = "zedas_broker_test";
	settings.port = g_config.getNumber(ConfigManager::SQL_PORT);
	BrokerFaultConnection *faults = new BrokerFaultConnection(settings);
	Broker::initialize(std::unique_ptr<Broker::Store>(faults));
	Player *owner = makePlayer(100, 100);
	owner->setGUID(1);
	owner->setTown(town);
	owner->setBankBalance(100);
	BrokerEngineTestAccess::session(*owner);
	Player *other = makePlayer(104, 100);
	other->setGUID(2);
	other->setTown(town);
	other->setBankBalance(100);
	BrokerEngineTestAccess::session(*other);
	auto add = [](Container &bag, uint16_t id, uint16_t count = 1) {
		Item *item = Item::CreateItem(id, count);
		require(g_game.internalAddItem(&bag, item, INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR,
		        "generated broker fixture");
		return item;
	};
	Container *corpse = corpseAt(99, 100);
	Container *bag = add(*corpse, 2853)->getContainer();
	Item *sword = add(*bag, 3264);
	Item *helmet = add(*bag, 3355);
	Item *worm = add(*bag, 3492, 7);
	Item *coin = add(*bag, 3031, 10);
	Item *protectedItem = add(*bag, 3357);
	protectedItem->setActionId(60000);
	Container *special = add(*corpse, 3244)->getContainer();
	Item *protectedChild = add(*special, 3357);
	Item *product = add(*bag, 3028);
	const int64_t before = OTSYS_TIME();
	EasyMode::onLootGenerated(*corpse);
	const int64_t exposed = OTSYS_TIME();
	require(sword->getParent() == corpse->getTile() && helmet->getParent() == corpse->getTile(),
	        "broker consumes only actually visible spilled items");
	require(worm->getParent() == bag && protectedItem->getParent() == bag && protectedChild->getParent() == special &&
	            coin->getParent() == bag,
	        "worms, coins and protected subtrees remain inside");
	require(!EasyMode::isBrokerCandidate(*sword, before + 4999, 5000) &&
	            EasyMode::isBrokerCandidate(*sword, exposed + 5000, 5000),
	        "full grace from actual exposure");
	Item *inserted = add(*bag, 3357);
	EasyMode::spillLoot();
	require(!EasyMode::isBrokerCandidate(*inserted, exposed + 60000, 5000),
	        "later insertions never become broker witnesses");
	// Manual pickup naturally wins and invalidates provenance even when dropped back.
	Container *carried = new Container(2854, 20);
	require(g_game.internalAddItem(owner, carried, CONST_SLOT_BACKPACK) == RETURNVALUE_NOERROR,
	        "equip manual pickup bag");
	require(g_game.internalMoveItem(sword->getTile(), carried, INDEX_WHEREEVER, sword, 1, nullptr) ==
	            RETURNVALUE_NOERROR,
	        "manual pickup before claim");
	require(!EasyMode::isBrokerCandidate(*sword, exposed + 60000, 5000), "manual pickup invalidates witness");
	require(EasyMode::collectGold(*owner) == 10 && coin->getParent() != bag,
	        "auto-gold remains responsible for currency");
	// The real Lua command may collect adjacent fodder immediately despite legacy grace.
	require(g_talkActions->playerSaySpell(owner, TALKTYPE_SAY, "!broker hire") == TALKACTION_BREAK,
	        "broker talkaction bridge");
	Npc *npc = brokerFor(*owner);
	require(npc && npc->getName() == "Loot Broker" && npc->getCurrentOutfit().lookType == 69 &&
	            !static_cast<Creature *>(npc)->isAttackable() && !npc->getMaster() && owner->getSummons().empty(),
	        "physical noncombatant NPC, no summon slot");
	require(helmet->getParent() != corpse->getTile(), "routine pickup is immediate even with configured legacy grace");
	Broker::command(*owner, "hire");
	require(brokerFor(*owner) == npc, "one actor per GUID");
	// Existing creatures can cross the broker, while its own route still uses native queries.
	require(npc->getTile()->queryAdd(0, *owner, 1, 0) == RETURNVALUE_NOERROR, "broker cannot block the player");
	Monster *hostile = makeMonster(20, 108, 100);
	require(npc->getTile()->queryAdd(0, *hostile, 1, 0) == RETURNVALUE_NOERROR,
	        "broker cannot block a hostile monster");
	require(!Broker::canStep(*npc, Position(111, 100, 7)) && !Broker::canStep(*npc, Position(100, 100, 6)),
	        "leash and floor guard queued steps");
	// Put the actor within one tile using real native movement (the same engine path endpoint).
	require(g_game.internalTeleport(npc, Position(99, 101, 7)) == RETURNVALUE_NOERROR,
	        "position actor at adjacent collection endpoint");
	Broker::tick(*owner, exposed + 6000);
	require(helmet->getParent() != corpse->getTile() && helmet->getParent()->getItem()->getContainer() &&
	            helmet->getParent() != bag,
	        "pickup moves the actual existing helmet instance into private escrow");
	Broker::command(*other, "hire");
	Broker::tick(*other, exposed + 7000);
	require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE item=3355")->getNumber<unsigned>("n") ==
	            1,
	        "two brokers cannot claim the same item");
	require(product->getParent() == corpse->getTile(), "spill allowlist is not automatically a sale catalog");
	require(worm->getParent() == bag && worm->getItemCount() == 7, "broker does not consume fishing worms");
	require(db->storeQuery("SELECT pending FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("pending") ==
	            0,
	        "collection is not a death-time sale");
	Broker::command(*owner, "sell");
	require(brokerFor(*owner) == npc, "field collector remains throughout represented courier trip");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"),
	        "advance fixture outbound deadline");
	Broker::tick(*owner, exposed + 9000);
	require(db->storeQuery("SELECT pending FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("pending") ==
	                3 &&
	            db->storeQuery("SELECT funds FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("funds") ==
	                1,
	        "4 gp helmet settles to 3 player and 1 economy fund");
	Broker::tick(*owner, exposed + 10000);
	require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_sales WHERE owner=1")->getNumber<unsigned>("n") == 1,
	        "repeated think never repeats sale");
	Broker::command(*owner, "cashout");
	Broker::command(*owner, "cashout");
	require(owner->getBankBalance() == 103 && owner->getBrokerBankWatermark() > 0,
	        "cashout reaches live bank exactly once");
	require(IOLoginData::savePlayer(owner), "actual IOLoginData save after payout");
	require(db->storeQuery("SELECT balance FROM players WHERE id=1")->getNumber<uint64_t>("balance") == 103,
	        "payout survives actual player save");
	// Model the live-memory state after a committed payout whose acknowledgement
	// was lost: SQL and ledger committed, Player baseline/watermark not advanced.
	require(db->executeQuery("UPDATE zedas_broker_accounts SET pending=8 WHERE owner=1"), "fixture pending credit");
	// Use the real command transaction, then deliberately restore pre-ack cache.
	const auto watermark = owner->getBrokerBankWatermark();
	faults->failure = BrokerFaultConnection::Failure::AfterCommit;
	Broker::command(*owner, "cashout");
	require(owner->getBankBalance() == 103 && owner->getBrokerBankWatermark() == watermark,
	        "unknown payout leaves live baseline unchanged");
	owner->setBankBalance(93);
	require(IOLoginData::savePlayer(owner) && IOLoginData::savePlayer(owner),
	        "repeated actual save with stale bank after unacknowledged payout");
	require(db->storeQuery("SELECT balance FROM players WHERE id=1")->getNumber<uint64_t>("balance") == 101,
	        "stale bank save preserves unseen 8 gp and ordinary 10 gp withdrawal exactly once");
	Broker::command(*owner, "cashout");
	require(owner->getBankBalance() == 101, "reconciliation folds unseen bank credit once");
	Player restored(nullptr);
	require(IOLoginData::loadPlayerById(&restored, 1) && restored.getBankBalance() == 101 &&
	            restored.getBrokerBankWatermark() == owner->getBrokerBankWatermark(),
	        "actual login loads bank and payout watermark consistently");
	Broker::command(*other, "cashout");
	require(other->getBankBalance() == 100, "another GUID cannot cash out owner balance");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "advance return deadline");
	Broker::tick(*owner, exposed + 12000);
	require(brokerFor(*owner), "broker returns after represented travel");
	// Fault the real physical claim path at both sides of COMMIT.
	Container *failedCorpse = corpseAt(100, 102);
	Item *failed = add(*failedCorpse, 3267);
	EasyMode::onLootGenerated(*failedCorpse);
	require(g_game.internalTeleport(brokerFor(*owner), Position(99, 101, 7)) == RETURNVALUE_NOERROR,
	        "approach fault fixture");
	faults->stage = "claim";
	faults->failure = BrokerFaultConnection::Failure::BeforeCommit;
	Broker::tick(*owner, exposed + 18000);
	require(failed->getParent() == failedCorpse->getTile() && Broker::locked(failed),
	        "unknown prepare result quarantines its exact ground source");
	require(g_game.internalMoveItem(failed->getTile(), carried, INDEX_WHEREEVER, failed, 1, nullptr) !=
	                RETURNVALUE_NOERROR &&
	            g_game.internalRemoveItem(failed) != RETURNVALUE_NOERROR &&
	            g_game.transformItem(failed, 3264)->getID() == 3267,
	        "native move/remove/transform paths cannot duplicate or mutate an uncertain source");
	Broker::command(*owner, "balance");
	require(!Broker::locked(failed) && failed->getParent() == failedCorpse->getTile(),
	        "rolled-back prepare unlocks the original without recreating anything");
	require(g_game.internalMoveItem(failed->getTile(), carried, INDEX_WHEREEVER, failed, 1, nullptr) ==
	            RETURNVALUE_NOERROR,
	        "manual pickup wins after rollback reconciliation");
	Container *committedCorpse = corpseAt(100, 103);
	Item *committed = add(*committedCorpse, 3267);
    committed->incrementReferenceCounter(); // Test observes this instance after committed disposal.
	EasyMode::onLootGenerated(*committedCorpse);
	Broker::command(*owner, "hire");
	require(g_game.internalTeleport(brokerFor(*owner), Position(99, 102, 7)) == RETURNVALUE_NOERROR,
	        "approach committed fault fixture");
	faults->failure = BrokerFaultConnection::Failure::AfterCommit;
	Broker::tick(*owner, exposed + 20000);
	require(Broker::locked(committed) && committed->getParent() == committedCorpse->getTile(),
	        "lost prepared acknowledgement retains a quarantined ground original");
	Broker::command(*owner, "balance");
	require(!Broker::locked(committed) && committed->getParent() != committedCorpse->getTile(),
	        "same-key reconciliation picks up the actual durable source once");
	require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE sold=0 AND item=3267")
	                ->getNumber<unsigned>("n") == 1,
	        "fault recovery preserves one escrow image");
	Broker::command(*owner, "sell");
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"),
	        "advance sale fault deadline");
	faults->stage = "sale";
	faults->failure = BrokerFaultConnection::Failure::AfterCommit;
	Broker::tick(*owner, exposed + 22000);
	require(committed->getParent() && committed->getParent()->getItem()->getContainer(),
	        "unknown sale acknowledgement retains the private live instance");
	Broker::command(*owner, "balance");
	require(
	    committed->getParent() == nullptr &&
	        db->storeQuery("SELECT pending FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("pending") ==
	            2,
	    "confirmed sale reconciliation consumes once and preserves rounded net");
    committed->decrementReferenceCounter();
	require(db->executeQuery("UPDATE zedas_broker_accounts SET deadline=0 WHERE owner=1"), "advance second return");
	Broker::tick(*owner, exposed + 24000);
	// A generated corpse decayed/removed before collection retires ground provenance.
	Container *decayed = corpseAt(102, 100);
	Item *abandoned = add(*decayed, 3355);
	EasyMode::onLootGenerated(*decayed);
	require(g_game.internalRemoveItem(decayed) == RETURNVALUE_NOERROR, "remove generating corpse before broker claim");
	require(!EasyMode::isBrokerCandidate(*abandoned, exposed + 60000, 5000),
	        "corpse decay/removal abandons unclaimed leftovers");
	Item *removed = add(*committedCorpse, 3355); // A later insertion is also never rescanned.
	removed->incrementReferenceCounter();
	require(g_game.internalRemoveItem(removed) == RETURNVALUE_NOERROR &&
	            !EasyMode::isBrokerCandidate(*removed, exposed + 60000, 5000),
	        "removed/later item cannot be collected");
	removed->decrementReferenceCounter();
	House *house = g_game.map.houses.addHouse(123);
	HouseTile *houseTile = new HouseTile(91, 91, 7, house);
	houseTile->internalAddThing(0, Item::CreateItem(g_game.map.getTile(90, 92, 7)->getGround()->getID()));
	g_game.map.setTile(Position(91, 91, 7), houseTile);
	Container *houseCorpse = corpseAt(91, 91);
	Item *persistent = add(*houseCorpse, 3264);
	EasyMode::onLootGenerated(*houseCorpse);
	require(persistent->getParent() == houseTile && !EasyMode::isBrokerCandidate(*persistent, exposed + 60000, 5000),
	        "persistent house-ground loot cannot enter transient-source escrow");
	// Execute the existing NPC's queued engine path, not a simulated distance check.
	Container *walkingCorpse = corpseAt(103, 100);
	Item *walking = add(*walkingCorpse, 3264);
	EasyMode::onLootGenerated(*walkingCorpse);
	npc = brokerFor(*owner);
	require(npc && g_game.internalTeleport(npc, Position(100, 99, 7)) == RETURNVALUE_NOERROR,
	        "start native path fixture");
	Broker::tick(*owner, exposed + 26000);
	unsigned steps = 0;
	while (!Position::areInRange<1, 1, 0>(npc->getPosition(), walking->getPosition()) && steps++ < 8)
		require(BrokerEngineTestAccess::step(*npc), "native queued path approaches loot");
	require(Position::areInRange<1, 1, 0>(npc->getPosition(), walking->getPosition()),
	        "native approach ends within one tile");
	Broker::tick(*owner, exposed + 27000);
	require(walking->getParent() != walkingCorpse->getTile(), "native walking endpoint permits physical pickup");
	walking->setActionId(60000);
	Broker::command(*owner, "sell");
	require(db->storeQuery("SELECT state FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("state") ==
	                0 &&
	            walking->getActionId() == 60000 && walking->getParent() != walkingCorpse->getTile(),
	        "newly protected live escrow cannot depart for sale or be consumed");
	walking->removeAttribute(ITEM_ATTRIBUTE_ACTIONID);
	std::ifstream originalActions("data/actions/actions.xml");
	const std::string stockActions((std::istreambuf_iterator<char>(originalActions)), std::istreambuf_iterator<char>());
	{
		std::ofstream actions("data/actions/actions.xml");
		actions << "<actions><action itemid='3264' script='misc/fluids.lua'/></actions>";
	}
	require(g_actions->reload(), "reload an unsafe handler for a carried identity");
	Broker::command(*owner, "sell");
	require(db->storeQuery("SELECT state FROM zedas_broker_accounts WHERE owner=1")->getNumber<unsigned>("state") == 0,
	        "handler reload cannot silently sell previously collected inventory");
	{
		std::ofstream actions("data/actions/actions.xml");
		actions << stockActions;
	}
	require(g_actions->reload(), "restore reviewed stock sword handler");
	Container *blockedCorpse = corpseAt(103, 103);
	Item *blocked = add(*blockedCorpse, 3357);
	EasyMode::onLootGenerated(*blockedCorpse);
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy)
			if (dx || dy)
				g_game.map.getTile(103 + dx, 103 + dy, 7)->setFlag(TILESTATE_BLOCKSOLID);
	require(g_game.internalTeleport(npc, Position(101, 101, 7)) == RETURNVALUE_NOERROR, "start blocked route fixture");
	Broker::tick(*owner, exposed + 30000);
	require(blocked->getParent() == blockedCorpse->getTile() && !Broker::locked(blocked),
	        "unreachable loot remains visible and unreserved");
	Container *vanishingCorpse = corpseAt(103, 96);
	Item *vanishing = add(*vanishingCorpse, 3267);
	EasyMode::onLootGenerated(*vanishingCorpse);
	require(g_game.internalTeleport(npc, Position(100, 99, 7)) == RETURNVALUE_NOERROR,
	        "start disappearing target fixture");
	Broker::tick(*owner, exposed + 32000);
	require(vanishing->getParent() == vanishingCorpse->getTile() && !Broker::locked(vanishing),
	        "approach does not reserve or remotely collect loot");
	require(g_game.internalRemoveItem(vanishing) == RETURNVALUE_NOERROR, "loot disappears during approach");
	Broker::tick(*owner, exposed + 34000);
	StaticTile *upstairs = new StaticTile(100, 100, 6);
	upstairs->internalAddThing(Item::CreateItem(g_game.map.getTile(90, 92, 7)->getGround()->getID()));
	g_game.map.setTile(Position(100, 100, 6), upstairs);
	require(g_game.internalTeleport(owner, Position(100, 100, 6)) == RETURNVALUE_NOERROR, "owner changes floor");
	Broker::tick(*owner, exposed + 36000);
	require(!brokerFor(*owner), "incompatible floor abandons work");
	require(db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE sold=0 AND owner=1")
	                ->getNumber<unsigned>("n") == 1,
	        "floor change retains collected escrow");
	require(g_game.internalTeleport(owner, Position(100, 100, 7)) == RETURNVALUE_NOERROR,
	        "return owner to supported floor");
    Broker::tick(*owner, OTSYS_TIME() + Broker::FLOOR_REJOIN_DELAY_MS + 1000);
    Broker::command(*owner, "hire");
    require(g_game.internalTeleport(owner, Position(112, 108, 7)) == RETURNVALUE_NOERROR,
	        "owner teleports beyond leash");
	Broker::tick(*owner, exposed + 40000);
	require(!brokerFor(*owner), "far travel abandons work without teleporting to loot");
	Broker::command(*other, "dismiss");
	require(!brokerFor(*other), "dismiss stops collection");
	Broker::command(*other, "hire");
	other->changeHealth(-other->getHealth());
	Broker::tick(*other, exposed + 28000);
	require(!brokerFor(*other), "owner death stops broker");
	Broker::command(*owner, "hire");
	require(brokerFor(*owner), "hire before actual logout");
	require(g_game.removeCreature(owner, true) && !brokerFor(*owner), "actual logout removes the physical broker");
	Broker::stop(*owner);
	Broker::stop(*other);
}
static void testBrokerWitness()
{
    require(Database::getInstance()->connect(), "visibility fixture DB");
    auto *db = Database::getInstance();
    for (const char *table : {"zedas_broker_escrow", "zedas_broker_sales", "zedas_broker_payouts", "zedas_broker_accounts"})
        require(db->executeQuery(std::string("DELETE FROM ") + table), "reset witness fixture");
    Broker::initialize();
    Player *owner = makePlayer(100, 100);
    owner->setGUID(1);
    Broker::command(*owner, "hire");
    Container *corpse = corpseAt(99, 100);
    Container *bag = Item::CreateItem(2853)->getContainer();
    require(g_game.internalAddItem(corpse, bag) == RETURNVALUE_NOERROR, "generated witness bag");
    Item *helmet = Item::CreateItem(3354);
    require(g_game.internalAddItem(bag, helmet) == RETURNVALUE_NOERROR, "generated brass helmet");
    EasyMode::onLootGenerated(*corpse);
    const auto base = OTSYS_TIME();
    require(EasyMode::isBrokerCandidate(*helmet, base, 0), "exact brass helmet qualifies after spill");
    require(g_game.internalTeleport(owner, Position(112, 108, 7)) == RETURNVALUE_NOERROR, "leave visibility");
    Broker::tick(*owner, base + 1000);
    require(helmet->getParent() == corpse->getTile(), "no remote claim while owner away");
    // Retire both the five-minute snapshot and its bounded corpse registry entry.
    require(EasyMode::isBrokerCandidate(*helmet, base + EasyMode::CORPSE_REGISTRY_TTL_MS + 1, 0),
            "snapshot TTL cannot retire valid qualified fodder");
    for (unsigned i = 0; i <= EasyMode::MAX_REGISTERED_CORPSES; ++i) {
        Container *other = corpseAt(110, 106);
        EasyMode::onLootGenerated(*other);
    }
    require(EasyMode::isBrokerCandidate(*helmet, base + 1, 0), "snapshot eviction preserves qualified fodder");
    require(g_game.internalTeleport(owner, Position(100, 100, 7)) == RETURNVALUE_NOERROR, "return to helmet");
    Broker::command(*owner, "hire");
    require(helmet->getParent() != corpse->getTile() &&
            db->storeQuery("SELECT COUNT(*) AS n FROM zedas_broker_escrow WHERE owner=1 AND item=3354")->getNumber<unsigned>("n") == 1,
            "return collects the exact untouched brass helmet once");
    Container *mutationCorpse = corpseAt(101, 100);
    Item *mutated = Item::CreateItem(3354);
    require(g_game.internalAddItem(mutationCorpse, mutated) == RETURNVALUE_NOERROR, "mutation fixture");
    EasyMode::onLootGenerated(*mutationCorpse);
    auto *tile = mutated->getTile();
    require(g_game.internalMoveItem(tile, owner->getTile(), INDEX_WHEREEVER, mutated, 1, nullptr) == RETURNVALUE_NOERROR,
            "player or actor moves witnessed helmet");
    require(g_game.internalMoveItem(owner->getTile(), tile, INDEX_WHEREEVER, mutated, 1, nullptr) == RETURNVALUE_NOERROR,
            "drop exact helmet back");
    require(!EasyMode::isBrokerCandidate(*mutated, OTSYS_TIME(), 0), "move away and back permanently invalidates witness");
    Broker::stop(*owner);
}
#endif
