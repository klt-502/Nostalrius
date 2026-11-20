#ifndef ZEDAS_OWNABLE_HOUSE_CASES_H
#define ZEDAS_OWNABLE_HOUSE_CASES_H
#include "easyhouse.h"
#include "iomapserialize.h"
#include "iologindata.h"
static Item *ownableItem(Tile *tile, uint16_t id) {
	if (!tile)
		return nullptr;
	for (size_t i = tile->getFirstIndex(); i < tile->getLastIndex(); ++i) {
		Thing *thing = tile->getThing(i);
		if (thing && thing->getItem() && thing->getItem()->getID() == id)
			return thing->getItem();
	}
	return nullptr;
}
static void testOwnableHouse(const std::string &stage) {
	using namespace EasyMode;
	require(g_game.groups.load(), "load actual groups for native owner persistence");
	Town *town = new Town(1);
	town->setName("Fixture town");
	town->setTemplePos(Position(100, 100, 7));
	require(g_game.map.towns.addTown(1, town), "fixture native depot town");
	Database *db = Database::getInstance();
	require(db->connect(), "explicit disposable ownable-house DB");
	const uint32_t hid = 900001;
	if (stage == "ownable_seed") {
		require(db->executeQuery("DELETE FROM zedas_house_fixtures WHERE house_id=900001"),
				"clear fixture test plan only");
		require(db->executeQuery("DELETE FROM houses WHERE id=900001"), "clear fixture house only");
		require(db->executeQuery("DELETE FROM players WHERE id IN(900101,900102)"), "clear fixture identities only");
		require(db->executeQuery(
					"INSERT INTO players(id,name,account_id,vocation,conditions,town_id,cap) VALUES(900101,'Fixture "
					"Seller',1234567,3,'',1,400),(900102,'Fixture Buyer',1234567,4,'',1,400)"),
				"seed fixture identities");
		require(db->executeQuery("INSERT INTO houses(id,owner,paid,warnings,name,rent,town_id,size,beds) "
								 "VALUES(900001,0,0,0,'Fixture lease',1,1,3,0)"),
				"seed fixture ownable house");
	}
	House *house = g_game.map.houses.addHouse(hid);
	house->setName(stage == "ownable_mismatch" ? "Changed identity" : "Fixture lease");
	house->setEntryPos(Position(100, 99, 6));
	house->setTownId(1);
	house->setRent(1);
	uint16_t ground = g_game.map.getTile(100, 100, 7)->getGround()->getID();
	for (const Position &p : {Position(101, 101, 6), Position(100, 100, 6), Position(100, 101, 6)}) {
		HouseTile *tile = new HouseTile(p.x, p.y, p.z, house);
		tile->internalAddThing(INDEX_WHEREEVER, Item::CreateItem(ground));
		g_game.map.setTile(p, tile);
		house->addTile(tile);
	}
	if (stage != "ownable_mismatch")
		require(houseFixtureSignature(*house) == "867f4b0e0173cecd7b30d4f868523383ee99fe83",
				"canonical native identity agrees with source review tool");
	require(IOMapSerialize::loadHouseInfo(), "native ownership restore before fixtures");
	auto tile = g_game.map.getTile(100, 101, 6);
	const std::string mapIdentity(40, 'a');
	auto manifest = [&](std::string name = "Fixture lease") {
		(void)name;
		return "<easyhouses version=\"1\" mapSha1=\"" + mapIdentity +
			   "\"><profile id=\"lease\"><fixture role=\"table\" item=\"2322\" name=\"table\" count=\"1\"/><fixture "
			   "role=\"carpet\" item=\"2581\" name=\"oriental carpet\" count=\"1\"/></profile><interior "
			   "id=\"lease-fixture\" profile=\"lease\" classification=\"reviewed-unowned-house\" review=\"SYNTHETIC "
			   "ONLY\" houseId=\"900001\" houseSignature=\"" +
			   houseFixtureSignature(*house) + "\" x=\"100\" y=\"100\" z=\"6\" ground=\"" + std::to_string(ground) +
			   "\"><place role=\"table\" x=\"100\" y=\"101\" z=\"6\" ground=\"" + std::to_string(ground) +
			   "\"/><place role=\"carpet\" x=\"101\" y=\"101\" z=\"6\" ground=\"" + std::to_string(ground) +
			   "\"/></interior></easyhouses>";
	};
	auto planCount = [&]() {
		return db->storeQuery("SELECT COUNT(*) AS n FROM zedas_house_fixtures WHERE house_id=900001")
			->getNumber<unsigned>("n");
	};
	auto apply = [&]() {
		HouseOverlay overlay;
		std::string error;
		require(overlay.loadXml(manifest(), mapIdentity, error), error.c_str());
		return overlay.apply(g_game.map, true, true, claimHouseLoot);
	};
	if (stage == "ownable_seed") {
		HouseOverlay bad;
		std::string error;
		std::string wrong = manifest();
		auto at = wrong.find(houseFixtureSignature(*house));
		wrong.replace(at, 40, std::string(40, 'b'));
		require(bad.loadXml(wrong, mapIdentity, error), "parse deliberately stale house signature");
		require(bad.apply(g_game.map, true, true, claimHouseLoot).rejected == 1 && planCount() == 0,
				"signature mismatch blocks activation");
		wrong = manifest();
		auto before = std::string("<fixture role=\"table\" item=\"2322\" name=\"table\" count=\"1\"/>");
		at = wrong.find(before);
		require(at != std::string::npos, "fixture test XML match");
		wrong.replace(at, before.size(), "<loot role=\"table\" item=\"3600\" name=\"bread\" count=\"1\"/>");
		require(!bad.loadXml(wrong, mapIdentity, error),
				"ownable plans cannot select loot even with loot globally enabled");
		Player seller(nullptr);
		require(IOLoginData::loadPlayerById(&seller, 900101), "load test owner through native persistence");
		Item *bag = Item::CreateItem(2853);
		Item *coin = Item::CreateItem(3031, 11);
		require(g_game.internalAddItem(tile, bag) == RETURNVALUE_NOERROR &&
					g_game.internalAddItem(bag->getContainer(), coin) == RETURNVALUE_NOERROR,
				"player contents before activation");
		auto result = apply();
		require(result.rejected == 1 && planCount() == 0 && bag->getParent() == tile,
				"occupied unowned house never modified or activated");
		require(g_game.internalMoveItem(tile, &seller, INDEX_WHEREEVER, bag, 1, nullptr) == RETURNVALUE_NOERROR,
				"owner takes original bag");
		house->setOwner(900101, true);
		result = apply();
		require(result.rejected == 1 && planCount() == 0 && !ownableItem(tile, 2322),
				"owned house without prior activation is never furnished");
		house->setOwner(0, true, &seller);
		require(db->executeQuery("UPDATE houses SET owner=900102 WHERE id=900001"),
				"simulate owner update after in-memory preflight");
		result = apply();
		require(result.rejected == 1 && planCount() == 0, "atomic DB ownership guard prevents raced activation");
		require(db->executeQuery("UPDATE houses SET owner=0 WHERE id=900001"), "restore fixture vacant state");
		result = apply();
		require(result.fixtures == 2 && planCount() == 1, "activate once while genuinely unowned");
		Item *fixture = ownableItem(tile, 2322);
		require(fixture && !fixture->isMoveable() && !fixture->isPickupable(), "native fixtures cannot be farmed");
		require(!EasyMode::isBrokerCandidate(*fixture, OTSYS_TIME(), 0), "fixture has no broker loot witness");
		house->setOwner(900101, true);
		require(house->getOwner() == 900101 && ownableItem(tile, 2322) == fixture,
				"purchase retains exact fixed furnishings");
		require(g_game.internalMoveItem(&seller, tile, INDEX_WHEREEVER, bag, 1, nullptr) == RETURNVALUE_NOERROR,
				"put original player bag on fixture");
		result = apply();
		require(!result.fixtures && bag->getParent() == tile && ownableItem(tile, 2322) == fixture,
				"reapply does not duplicate or touch player contents");
		require(IOMapSerialize::saveHouseInfo() && IOMapSerialize::saveHouseItems(),
				"native house save with player contents");
		auto stored = db->storeQuery("SELECT data FROM tile_store WHERE house_id=900001");
		require(bool(stored), "player bag serialized");
		unsigned long length;
		const char *data = stored->getStream("data", length);
		PropStream stream;
		stream.init(data, length);
		uint16_t x, y, id;
		uint8_t z;
		uint32_t count;
		require(stream.read(x) && stream.read(y) && stream.read(z) && stream.read(count) && count == 1 &&
					stream.read(id) && id == 2853,
				"native saver writes player container, excludes server fixtures");
		require(IOLoginData::savePlayer(&seller), "native save of emptied owner inventory");
		// Simulate a crash after durable activation: no fixture serialization is needed for recovery.
		require(g_game.internalRemoveItem(fixture, 1, false, FLAG_IGNORENOTMOVEABLE) == RETURNVALUE_NOERROR,
				"test-only loss of unsaved fixture instance");
		return;
	}
	if (stage == "ownable_corrupt") {
		require(db->executeQuery("UPDATE zedas_house_fixtures SET plan_hash=REPEAT('0',40) WHERE house_id=900001"),
				"corrupt only disposable plan hash");
		auto rejected = restoreOwnableHouseFixtures(g_game.map, mapIdentity);
		require(rejected.rejected == 1 && !ownableItem(tile, 2322),
				"corrupted snapshot cannot authorize fixture restoration");
		require(db->executeQuery("UPDATE zedas_house_fixtures SET plan_hash=SHA1(plan_data) WHERE house_id=900001"),
				"repair only disposable test hash");
		return;
	}
	const bool disabled = stage == "ownable_disabled" || stage == "ownable_master_disabled";
	auto restored = restoreOwnableHouseFixtures(g_game.map, mapIdentity);
	if (disabled || stage == "ownable_mismatch") {
		require(restored.fixtures == 0 && !ownableItem(tile, 2322),
				"disabled/source-mismatch restoration fails closed");
		IOMapSerialize::loadHouseItems(&g_game.map);
		require(planCount() == 1, "disabled or stale source never deletes activation history");
		return;
	}
	require(restored.fixtures == 2,
			"durable activation reconstructs before native items regardless current owner/manifest");
	Item *fixture = ownableItem(tile, 2322);
	IOMapSerialize::loadHouseItems(&g_game.map);
	require(ownableItem(tile, 2322) == fixture, "native contents restore does not recreate or remove fixture");
	auto again = restoreOwnableHouseFixtures(g_game.map, mapIdentity);
	require(again.fixtures == 0, "repair/reload is idempotent, not destructive reset");
	if (stage == "ownable_transfer") {
		Item *bag = ownableItem(tile, 2853);
		require(bag && bag->getContainer()->size() == 1 &&
					bag->getContainer()->getItemList().front()->getItemCount() == 11,
				"player contents survive restart over reconstructed fixture");
		Player buyer(nullptr);
		require(IOLoginData::loadPlayerById(&buyer, 900102), "load next owner");
		auto *deed = house->getTransferItem();
		require(deed && house->executeTransfer(deed, &buyer), "native house deed transfer/sale");
		require(house->getOwner() == 900102 && ownableItem(tile, 2322) == fixture && !ownableItem(tile, 2853),
				"sale retains fixture and handles goods through native transfer");
		Player seller(nullptr);
		require(IOLoginData::loadPlayerById(&seller, 900101), "reload offline seller after transfer");
		std::vector<Item *> goods(seller.getDepotLocker(1, true)->getItemList().begin(),
								  seller.getDepotLocker(1, true)->getItemList().end());
		unsigned gold = 0;
		for (size_t i = 0; i < goods.size(); ++i) {
			if (goods[i]->getID() == 3031)
				gold += goods[i]->getItemCount();
			if (auto *c = goods[i]->getContainer())
				goods.insert(goods.end(), c->getItemList().begin(), c->getItemList().end());
		}
		require(gold == 11, "seller goods delivered to native depot without loss");
		require(IOMapSerialize::saveHouseInfo() && IOMapSerialize::saveHouseItems(), "save transferred house");
	} else if (stage == "ownable_vacate") {
		Player buyer(nullptr);
		require(IOLoginData::loadPlayerById(&buyer, 900102), "load vacating owner");
		house->setOwner(0, true, &buyer);
		require(ownableItem(tile, 2322) == fixture && planCount() == 1,
				"leave/repossess/reset ownership keeps same world fixture plan");
		auto r = apply();
		require(r.fixtures == 0 && planCount() == 1, "return to unowned does not restock or give another plan");
		require(IOMapSerialize::saveHouseInfo() && IOMapSerialize::saveHouseItems(), "save vacant house");
	} else if (stage == "ownable_rebuy") {
		house->setOwner(900101, true);
		require(ownableItem(tile, 2322) == fixture, "re-purchase keeps same fixed objects");
		require(IOMapSerialize::saveHouseInfo(), "save new owner");
	} else if (stage == "ownable_cleanup") {
		require(db->executeQuery("DELETE FROM zedas_house_fixtures WHERE house_id=900001"),
				"fixture-only cleanup plan");
		require(db->executeQuery("DELETE FROM tile_store WHERE house_id=900001"), "fixture-only cleanup tile data");
		require(db->executeQuery("DELETE FROM houses WHERE id=900001"), "fixture-only cleanup house");
		require(db->executeQuery("DELETE FROM players WHERE id IN(900101,900102)"), "fixture-only cleanup players");
	}
}
#endif
