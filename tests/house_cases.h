#ifndef ZEDAS_HOUSE_CASES_H
#define ZEDAS_HOUSE_CASES_H
#include "easyhouse.h"
#include "house.h"
#include "housetile.h"
struct HouseOverlayTestAccess {
	static void owner(House &house) { house.owner = 1; }
};
static void testHouses(const std::string &scenario) {
	if (scenario == "houses_store_seed" || scenario == "houses_store_replay") {
		require(Database::getInstance()->connect(), "connect explicitly disposable house ledger fixture");
		if (scenario == "houses_store_seed")
			require(Database::getInstance()->executeQuery("DELETE FROM zedas_house_loot"),
					"reset house issuance fixture only");
		const bool first = scenario == "houses_store_seed";
		require(EasyMode::claimHouseLoot("synthetic/food", std::string(40, 'a')) == first,
				"durable one-time issuance across real process restart");
		require(!EasyMode::claimHouseLoot("synthetic/food", std::string(40, 'b')),
				"changed identity never reuses old slot key");
		return;
	}
	using namespace EasyMode;
	const bool active = scenario != "houses_disabled";
	const bool loot = scenario != "houses_fixture_only";
	const auto ground = g_game.map.getTile(100, 100, 7)->getGround()->getID();
	const std::string identity(40, 'a');
	const std::string profile = "<profile id=\"poor-residence\"><fixture role=\"table\" item=\"2322\" name=\"table\" "
								"count=\"1\"/><loot role=\"food\" item=\"3600\" name=\"bread\" count=\"1\"/></profile>";
	const auto interior = [&](std::string id, int x) {
		return "<interior id=\"" + id +
			   "\" profile=\"poor-residence\" classification=\"reviewed-public\" review=\"SYNTHETIC ONLY\" "
			   "houseId=\"0\" x=\"" +
			   std::to_string(x) + "\" y=\"100\" z=\"7\" ground=\"" + std::to_string(ground) +
			   "\"><place role=\"table\" x=\"" + std::to_string(x) + "\" y=\"101\" z=\"7\" ground=\"" +
			   std::to_string(ground) + "\"/><place role=\"food\" x=\"" + std::to_string(x) +
			   "\" y=\"102\" z=\"7\" ground=\"" + std::to_string(ground) + "\"/></interior>";
	};
	auto write = [&](const std::string &p, const std::string &in) {
		std::ofstream f("house-test.xml");
		f << "<easyhouses version=\"1\" mapSha1=\"" << identity << "\">" << p << in << "</easyhouses>";
	};
	HouseOverlay overlay;
	std::string error;
	write(profile, interior("synthetic", 105));
	require(overlay.load("house-test.xml", identity, error), error.c_str());
	std::set<std::string> issued;
	int attempts = 0;
	auto claim = [&](const std::string &key, const std::string &) {
		++attempts;
		return issued.insert(key).second;
	};
	auto counts = overlay.apply(g_game.map, true, loot, claim);
	require(counts.fixtures == size_t(active) && counts.loot == size_t(active && loot), "master/furnishing/loot gates");
	require(!active ? attempts == 0 : true, "disabled system never consults persistence");
	if (!active)
		return;
	auto *tile = g_game.map.getTile(105, 101, 7);
	Item *table = tile->getTopTopItem();
	if (!table)
		table = tile->getTopDownItem();
	require(table && table->getID() == 2322 && !table->isMoveable() && !table->isPickupable(),
			"fixture is native immovable, non-pickupable furniture");
	require(!EasyMode::isBrokerCandidate(*table, OTSYS_TIME(), 0), "fixture has no generated-corpse broker witness");
	auto again = overlay.apply(g_game.map, true, loot, claim);
	require(again.fixtures == 0 && again.loot == 0, "same-session apply is idempotent");
	require(overlay.load("house-test.xml", identity, error), "reload valid manifest");
	again = overlay.apply(g_game.map, true, loot, claim);
	require(!again.fixtures && !again.loot, "manifest reload never replenishes");
	// Simulated fresh world: discard only fixture-owned test instances, never unrelated/player items.
	require(g_game.internalRemoveItem(table, 1, false, FLAG_IGNORENOTMOVEABLE) == RETURNVALUE_NOERROR,
			"test-only fixture cleanup");
	Item *food = g_game.map.getTile(105, 102, 7)->getTopDownItem();
	if (food) {
		require(food->getID() == 3600, "one physical food item");
		require(!EasyMode::isBrokerCandidate(*food, OTSYS_TIME(), 0), "house loot has no broker provenance");
		require(g_game.internalRemoveItem(food) == RETURNVALUE_NOERROR, "test-only collect one-time food");
	}
	HouseOverlay restart;
	require(restart.load("house-test.xml", identity, error), "restart manifest");
	again = restart.apply(g_game.map, true, loot, claim);
	require(again.fixtures == 1 && again.loot == 0,
			"fresh-world restart regenerates inert fixture but never issued loot");
	require(issued.size() == size_t(loot), "one issuance identity only");
	// Durable reservation followed by crash before placement: never compensate/reissue.
	write(profile, interior("crash", 108));
	HouseOverlay crash;
	require(crash.load("house-test.xml", identity, error), "crash fixture parse");
	issued.insert("crash/food");
	again = crash.apply(g_game.map, true, true, claim);
	require(again.loot == 0, "reserved-before-crash reward never reissued");
	write(profile, interior("wrong-ground", 103));
	{
		std::ifstream f("house-test.xml");
		std::string xml((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		auto at = xml.find("ground=\"" + std::to_string(ground) + "\"");
		xml.replace(at, std::string("ground=\"" + std::to_string(ground) + "\"").size(), "ground=\"65535\"");
		std::ofstream out("house-test.xml");
		out << xml;
	}
	HouseOverlay wrongGround;
	require(wrongGround.load("house-test.xml", identity, error), "parse exact-ground test");
	again = wrongGround.apply(g_game.map, true, true, claim);
	require(again.rejected == 1 && !again.fixtures && !again.loot,
			"changed exact ground identity blocks all placements");
	makePlayer(101, 99);
	write(profile, interior("near-player", 101));
	HouseOverlay nearPlayer;
	require(nearPlayer.load("house-test.xml", identity, error), "near-actor parse");
	again = nearPlayer.apply(g_game.map, true, true, claim);
	require(again.rejected == 1 && !again.fixtures, "nearby actors block furnishing");
	// Player object at a target: reject complete interior and preserve exact instance.
	Item *playerBag = Item::CreateItem(2853);
	auto *target = g_game.map.getTile(110, 102, 7);
	require(g_game.internalAddItem(target, playerBag) == RETURNVALUE_NOERROR, "place unrelated player container");
	Item *nested = Item::CreateItem(2853);
	require(g_game.internalAddItem(playerBag->getContainer(), nested) == RETURNVALUE_NOERROR,
			"player nested container");
	write(profile, interior("occupied", 110));
	HouseOverlay occupied;
	require(occupied.load("house-test.xml", identity, error), "occupied fixture parse");
	again = occupied.apply(g_game.map, true, true, claim);
	require(again.rejected == 1 && !again.fixtures && !again.loot && playerBag->getParent() == target &&
				nested->getParent() == playerBag->getContainer(),
			"no destruction/replacement of player/nested contents");
	// All HouseTiles, including unowned/owned, are categorically excluded.
	House *house = new House(987654);
	auto *houseTile = new HouseTile(89, 101, 6, house);
	houseTile->internalAddThing(INDEX_WHEREEVER, Item::CreateItem(ground));
	g_game.map.setTile(Position(89, 101, 6), houseTile);
	for (int y : {100, 102}) {
		auto *t = new StaticTile(89, y, 6);
		t->internalAddThing(Item::CreateItem(ground));
		g_game.map.setTile(Position(89, y, 6), t);
	}
	std::string ownableXml = interior("ownable", 89);
	for (size_t at = 0; (at = ownableXml.find("z=\"7\"", at)) != std::string::npos; at += 5)
		ownableXml.replace(at, 5, "z=\"6\"");
	write(profile, ownableXml);
	HouseOverlay ownable;
	require(ownable.load("house-test.xml", identity, error), "ownable fixture parse");
	again = ownable.apply(g_game.map, true, true, claim);
	require(again.rejected == 1 && !again.fixtures, "unowned ownable HouseTile rejected");
	HouseOverlayTestAccess::owner(*house);
	again = ownable.apply(g_game.map, true, true, claim);
	require(again.rejected == 1 && !again.fixtures, "owned HouseTile rejected too");
	// Fail-closed parser variants, including generated containers and nested content.
	std::vector<std::string> bad = {
		"<profile id=\"bad\"><loot role=\"bag\" item=\"2853\" name=\"bag\" count=\"1\"/></profile>",
		"<profile id=\"bad\"><fixture role=\"bed\" item=\"3230\" name=\"bed\" count=\"1\"/></profile>",
		profile + profile,
		"<profile id=\"bad\" id=\"other\"/>",
		"<interior id=\"quest\" classification=\"quest\" houseId=\"0\"/>",
		"<unknown/>",
		"<profile id=\"bad\"><loot role=\"food\" item=\"3600\" name=\"bread\" count=\"1.5\"/></profile>",
		"<profile id=\"bad\"><loot role=\"food\" item=\"3600\" name=\"bread\" count=\"1\"><loot/></loot></profile>"};
	for (const auto &p : bad) {
		write(p, "");
		require(!overlay.load("house-test.xml", identity, error) && overlay.size() == 0,
				"invalid manifest clears all authorization");
	}
	write(profile, interior("wrong-map", 104));
	require(!overlay.load("house-test.xml", std::string(40, 'b'), error) && overlay.size() == 0,
			"map drift fails closed");
	HouseOverlay shipped;
	require(shipped.load("data/zedas/houses.xml", "b1a0819aa36549e5185ec3bb21a0e125d381a500", error) &&
				shipped.size() == 0,
			error.c_str());
}
#endif
