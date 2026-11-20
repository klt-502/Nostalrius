#include "otpch.h"
#include "easyhouse.h"
#include "actions.h"
#include "configmanager.h"
#include "database.h"
#include "game.h"
#include "housetile.h"
#include "movement.h"
#include "pugixml.hpp"
#include "tools.h"
#include <fstream>
extern ConfigManager g_config;
extern Game g_game;
extern Actions *g_actions;
extern MoveEvents *g_moveEvents;
namespace {
std::set<std::pair<const Map *, uint32_t>> installedHouses;
bool activateFixtures(uint32_t houseId, const std::string &plan);
void require(bool ok, const std::string &why) {
	if (!ok)
		throw std::runtime_error(why);
}
void attrs(pugi::xml_node n, std::initializer_list<const char *> allowed) {
	std::set<std::string> seen;
	for (auto a : n.attributes()) {
		require(seen.insert(a.name()).second, "duplicate attribute");
		require(std::find_if(allowed.begin(), allowed.end(),
							 [&](const char *s) { return a.name() == std::string(s); }) != allowed.end(),
				"unknown attribute");
	}
}
unsigned number(pugi::xml_node n, const char *key, unsigned lo, unsigned hi) {
	std::string s = n.attribute(key).value();
	require(!s.empty() && s.size() <= 10, "missing/invalid number");
	uint64_t v = 0;
	for (char c : s) {
		require(c >= '0' && c <= '9', "integer required");
		v = v * 10 + c - '0';
	}
	require(v >= lo && v <= hi, "number out of range");
	return v;
}
std::string token(pugi::xml_node n, const char *key) {
	std::string s = n.attribute(key).value();
	require(!s.empty() && s.size() <= 48, "missing/long identity");
	for (char c : s)
		require((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-', "invalid identity token");
	return s;
}
Position pos(pugi::xml_node n) {
	return Position(number(n, "x", 1, 65535), number(n, "y", 1, 65535), number(n, "z", 0, 15));
}
bool inert(uint16_t id, bool loot) {
	if (!g_actions || !g_moveEvents || id >= Item::items.size())
		return false;
	const auto &t = Item::items[id];
	// Initial reviewed catalog: native inert, non-removable fixtures; tiny loose rewards only.
	const std::set<uint16_t> fixtures = {2322, 2323, 2324, 2572, 2581, 4306};
	const std::set<uint16_t> rewards = {3031, 3600, 3607};
	if (!(loot ? rewards : fixtures).count(id) || t.id != id || t.isContainer() || t.isChest() || t.isKey() ||
		t.isTeleport() || t.isMailbox() || t.isMagicField() || t.isSplash() || t.forceUse || t.distUse ||
		t.isGroundTile() || t.isDoor() || t.isBed() || t.isDepot() || t.isRune() || t.corpse || t.isFluidContainer() ||
		t.decayTime || t.charges || t.destroy || t.rotatable || t.changeUse || t.transformToOnUse ||
		t.transformToFree || t.transformEquipTo || t.transformDeEquipTo || t.collisionEvent || t.separationEvent ||
		t.conditionDamage || t.canReadText || t.canWriteText || t.disguise || t.isHangable ||
		t.fluidSource != FLUID_NONE || g_moveEvents->hasItemEvent(id))
		return false;
	const auto use = g_actions->getItemGroundUse(id);
	if (use != Action::GroundUse::None && !(loot && t.nutrition > 0 && use == Action::GroundUse::Food))
		return false;
	if (loot)
		return t.moveable && t.pickupable && !t.multiUseEvent;
	return !t.moveable && !t.pickupable && !t.useEvent && !t.multiUseEvent && !t.useable && !t.forceUse && !t.distUse;
}
bool emptyPoint(Map &map, const Position &p, uint16_t ground, uint32_t houseId = 0) {
	Tile *tile = map.getTile(p);
	HouseTile *ht = tile ? dynamic_cast<HouseTile *>(tile) : nullptr;
	if (!tile || (houseId ? (!ht || ht->getHouse()->getId() != houseId) : ht != nullptr) || !tile->getGround() ||
		tile->getGround()->getID() != ground || tile->getThingCount() != 1 || tile->getGround()->getActionId() ||
		tile->getGround()->hasAttribute(static_cast<itemAttrTypes>(~0U)) || g_moveEvents->hasPositionEvent(p) ||
		g_moveEvents->hasItemEvent(ground) || g_actions->hasItemAction(ground))
		return false;
	if (tile->hasFlag(TILESTATE_TELEPORT | TILESTATE_MAGICFIELD | TILESTATE_MAILBOX | TILESTATE_BED | TILESTATE_DEPOT |
					  TILESTATE_BLOCKSOLID | TILESTATE_BLOCKPATH | TILESTATE_REFRESH))
		return false;
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy) {
			Tile *near = map.getTile(p.x + dx, p.y + dy, p.z);
			HouseTile *other = near ? dynamic_cast<HouseTile *>(near) : nullptr;
			if (other && (!houseId || other->getHouse()->getId() != houseId))
				return false;
			if (near && near->getCreatures() && !near->getCreatures()->empty())
				return false;
		}
	return true;
}
} // namespace
bool EasyMode::HouseOverlay::load(const std::string &file, const std::string &mapIdentity, std::string &error) {
	std::ifstream input(file, std::ios::binary);
	if (!input) {
		profiles.clear();
		interiors.clear();
		error = "cannot read manifest";
		return false;
	}
	return loadXml(std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()), mapIdentity,
				   error);
}
bool EasyMode::HouseOverlay::loadXml(const std::string &xml, const std::string &mapIdentity, std::string &error) {
	profiles.clear();
	interiors.clear(); // Failed reload never retains authorization; processed issuance stays retired.
	try {
		pugi::xml_document d;
		require(xml.size() <= 1048576 && bool(d.load_buffer(xml.data(), xml.size())), "cannot parse/bounded manifest");
		size_t roots = 0;
		for (auto child : d.children())
			if (child.type() == pugi::node_element)
				++roots;
		require(roots == 1, "multiple roots");
		auto root = d.document_element();
		require(std::string(root.name()) == "easyhouses", "bad root");
		attrs(root, {"version", "mapSha1"});
		require(std::string(root.attribute("version").value()) == "1", "bad version");
		identity = root.attribute("mapSha1").value();
		require(identity.size() == 40 && identity == mapIdentity, "map identity mismatch");
		std::map<std::string, std::map<std::string, HouseRole>> checkedProfiles;
		std::vector<HouseInterior> checked;
		std::set<std::string> ids;
		std::set<uint32_t> houseIds;
		std::set<std::tuple<int, int, int>> coordinates;
		for (auto n : root.children()) {
			require(n.type() == pugi::node_element, "unexpected node");
			if (std::string(n.name()) == "profile") {
				attrs(n, {"id"});
				std::string id = token(n, "id");
				require(!checkedProfiles.count(id), "duplicate profile");
				std::map<std::string, HouseRole> roles;
				for (auto r : n.children()) {
					require(std::string(r.name()) == "fixture" || std::string(r.name()) == "loot", "unknown role kind");
					attrs(r, {"role", "item", "name", "count"});
					HouseRole role;
					role.loot = std::string(r.name()) == "loot";
					role.item = number(r, "item", 1, 65535);
					role.count = number(r, "count", 1, 5);
					role.name = r.attribute("name").value();
					require(!r.first_child() && inert(role.item, role.loot) && Item::items[role.item].name == role.name,
							"unsafe/unreviewed role item or stale name");
					require((role.item == 3031 || role.count == 1) &&
								(!role.loot || role.item == 3031 || role.count == 1),
							"invalid quantity");
					require(roles.emplace(token(r, "role"), role).second, "duplicate role");
				}
				require(
					!roles.empty() && roles.size() <= 16 &&
						std::any_of(roles.begin(), roles.end(),
									[](const std::pair<const std::string, HouseRole> &r) { return !r.second.loot; }),
					"profile needs an inert fixture");
				checkedProfiles.emplace(id, std::move(roles));
			} else if (std::string(n.name()) == "interior") {
				attrs(n, {"id", "profile", "classification", "review", "houseId", "houseSignature", "x", "y", "z",
						  "ground"});
				HouseInterior in;
				in.houseId = number(n, "houseId", 0, UINT32_MAX);
				in.houseSignature = n.attribute("houseSignature").value();
				const std::string classification = n.attribute("classification").value();
				require(in.houseId ? classification == "reviewed-unowned-house" && in.houseSignature.size() == 40 &&
										 houseIds.insert(in.houseId).second
								   : classification == "reviewed-public" && in.houseSignature.empty(),
						"unsafe classification/house identity");
				in.id = token(n, "id");
				in.profile = token(n, "profile");
				in.review = n.attribute("review").value();
				in.anchor = pos(n);
				in.ground = number(n, "ground", 1, 65535);
				require(!in.review.empty() && in.review.size() <= 256 && ids.insert(in.id).second,
						"missing review/duplicate interior");
				std::set<std::string> roles;
				for (auto p : n.children()) {
					require(std::string(p.name()) == "place" && !p.first_child(), "invalid placement");
					attrs(p, {"role", "x", "y", "z", "ground"});
					HousePoint point;
					point.role = token(p, "role");
					point.pos = pos(p);
					point.ground = number(p, "ground", 1, 65535);
					require(point.pos.z == in.anchor.z && Position::getDistanceX(point.pos, in.anchor) <= 16 &&
								Position::getDistanceY(point.pos, in.anchor) <= 16 && roles.insert(point.role).second &&
								coordinates.emplace(point.pos.x, point.pos.y, point.pos.z).second,
							"duplicate placement/role or floor mismatch");
					in.points.push_back(point);
				}
				require(!in.points.empty() && in.points.size() <= 16, "invalid interior size");
				checked.push_back(in);
			} else
				require(false, "unknown manifest element");
		}
		require(checked.size() <= 64, "pilot limit exceeded");
		for (const auto &in : checked) {
			require(checkedProfiles.count(in.profile), "unknown profile");
			unsigned gp = 0;
			for (const auto &p : in.points) {
				require(checkedProfiles.at(in.profile).count(p.role), "unknown profile role");
				auto r = checkedProfiles.at(in.profile).at(p.role);
				require(!in.houseId || !r.loot, "ownable houses are fixture-only; no loot or containers");
				if (r.item == 3031)
					gp += r.count;
			}
			require(gp <= 5, "interior currency cap exceeded");
		}
		profiles.swap(checkedProfiles);
		interiors.swap(checked);
		return true;
	} catch (const std::exception &e) {
		profiles.clear();
		interiors.clear();
		error = e.what();
		return false;
	}
}
EasyMode::HouseCounts EasyMode::HouseOverlay::apply(Map &map, bool furnishing, bool loot, const Claim &claim) {
	HouseCounts counts;
	if (!g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) || (!furnishing && !loot))
		return counts;
	for (const auto &in : interiors) {
		if (processed.count(in.id))
			continue;
		if (in.houseId) {
			if (!furnishing || !ownableHouseFixturesEnabled())
				continue;
			if (installedHouses.count({&map, in.houseId}))
				continue;
			House *house = map.houses.getHouse(in.houseId);
			bool valid = house && house->getOwner() == 0 && houseFixtureSignature(*house) == in.houseSignature &&
						 emptyPoint(map, in.anchor, in.ground, in.houseId);
			for (const auto &p : in.points)
				valid = valid && emptyPoint(map, p.pos, p.ground, in.houseId) &&
						inert(profiles.at(in.profile).at(p.role).item, false);
			if (!valid) {
				++counts.rejected;
				continue;
			}
			const std::string plan = snapshot(in);
			if (!activateFixtures(in.houseId, plan)) {
				++counts.rejected;
				continue;
			}
			HouseOverlay activated;
			std::string error;
			if (!activated.loadXml(plan, identity, error)) {
				++counts.rejected;
				continue;
			}
			auto applied = activated.restoreStored(map, in.houseId);
			counts.approved += applied.approved;
			counts.fixtures += applied.fixtures;
			counts.rejected += applied.rejected;
			processed.insert(in.id);
			continue;
		}
		bool valid = emptyPoint(map, in.anchor, in.ground);
		for (const auto &p : in.points)
			valid = valid && emptyPoint(map, p.pos, p.ground) &&
					inert(profiles.at(in.profile).at(p.role).item, profiles.at(in.profile).at(p.role).loot);
		if (!valid) {
			++counts.rejected;
			continue;
		}
		++counts.approved;
		processed.insert(in.id);
		for (const auto &p : in.points) {
			const auto &role = profiles.at(in.profile).at(p.role);
			if (role.loot ? !loot : !furnishing)
				continue;
			const std::string canonical = identity + "/" + in.id + "/" + in.profile + "/" + p.role + "/" +
										  std::to_string(p.pos.x) + "/" + std::to_string(p.pos.y) + "/" +
										  std::to_string(p.pos.z) + "/" + std::to_string(role.item) + "/" +
										  std::to_string(role.count);
			if (role.loot && (!claim || !claim(in.id + "/" + p.role, transformToSHA1(canonical)))) {
				++counts.rejected;
				continue;
			}
			std::unique_ptr<Item> item(Item::CreateItem(role.item, role.count));
			if (!item || g_game.internalAddItem(map.getTile(p.pos), item.get(), INDEX_WHEREEVER,
												FLAG_IGNOREAUTOSTACK) != RETURNVALUE_NOERROR) {
				++counts.rejected;
				continue;
			}
			item.release();
			if (role.loot)
				++counts.loot;
			else
				++counts.fixtures;
		}
	}
	return counts;
}
namespace {
class HouseLedger {
	MYSQL *connection = nullptr;
	bool ready = false;
	bool schemaValue(const char *query, const char *expected) {
		if (mysql_query(connection, query) != 0)
			return false;
		MYSQL_RES *result = mysql_store_result(connection);
		if (!result)
			return false;
		MYSQL_ROW row = mysql_fetch_row(result);
		const bool ok = row && row[0] && std::string(row[0]) == expected;
		mysql_free_result(result);
		return ok;
	}

  public:
	HouseLedger() {
		connection = mysql_init(nullptr);
		if (!connection)
			return;
		unsigned timeout = 2;
		bool reconnect = false;
		mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
		mysql_options(connection, MYSQL_OPT_READ_TIMEOUT, &timeout);
		mysql_options(connection, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
		mysql_options(connection, MYSQL_OPT_RECONNECT, &reconnect);
		ready = mysql_real_connect(connection, g_config.getString(ConfigManager::MYSQL_HOST).c_str(),
								   g_config.getString(ConfigManager::MYSQL_USER).c_str(),
								   g_config.getString(ConfigManager::MYSQL_PASS).c_str(),
								   g_config.getString(ConfigManager::MYSQL_DB).c_str(),
								   g_config.getNumber(ConfigManager::SQL_PORT),
								   g_config.getString(ConfigManager::MYSQL_SOCK).c_str(), 0) &&
				mysql_autocommit(connection, true) == 0 && mysql_set_character_set(connection, "utf8mb4") == 0;
	}
	~HouseLedger() {
		if (connection)
			mysql_close(connection);
	}
	bool fixtureSchema() {
		return ready &&
			   schemaValue(
				   "SELECT ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='houses'",
				   "InnoDB") &&
			   schemaValue("SELECT ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND "
						   "TABLE_NAME='zedas_house_fixtures'",
						   "InnoDB") &&
			   schemaValue(
				   "SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) FROM information_schema.STATISTICS WHERE "
				   "TABLE_SCHEMA=DATABASE() AND TABLE_NAME='zedas_house_fixtures' AND INDEX_NAME='PRIMARY'",
				   "house_id");
	}
	std::string escaped(const std::string &text) {
		// Hex literals avoid SQL-mode-dependent quoting and preserve UTF-8 snapshot bytes.
		static const char hex[] = "0123456789abcdef";
		std::string encoded;
		encoded.reserve(text.size() * 2);
		for (unsigned char c : text) {
			encoded += hex[c >> 4];
			encoded += hex[c & 15];
		}
		return "CONVERT(0x" + encoded + " USING utf8mb4)";
	}
	bool activate(uint32_t houseId, const std::string &plan) {
		if (plan.size() > 65535 || !fixtureSchema())
			return false;
		// Ownership is checked by the same atomic INSERT SELECT that commits activation.
		std::string query = "INSERT INTO zedas_house_fixtures(house_id,plan_hash,plan_data) SELECT id," +
							escaped(transformToSHA1(plan)) + "," + escaped(plan) +
							" FROM houses WHERE id=" + std::to_string(houseId) + " AND owner=0";
		if (mysql_real_query(connection, query.data(), query.size()) != 0) {
			if (mysql_errno(connection) != 1062)
				ready = false;
			return false;
		}
		return mysql_affected_rows(connection) == 1;
	}
	std::vector<std::tuple<uint32_t, std::string, std::string>> plans() {
		std::vector<std::tuple<uint32_t, std::string, std::string>> rows;
		if (!fixtureSchema()) {
			ready = false;
			std::cout << "[Zedas ownable houses] activation ledger unavailable/schema invalid; no restoration"
					  << std::endl;
			return rows;
		}
		if (!fixtureSchema() ||
			mysql_query(connection,
						"SELECT house_id,plan_hash,plan_data FROM zedas_house_fixtures ORDER BY house_id") != 0)
			return rows;
		MYSQL_RES *result = mysql_store_result(connection);
		if (!result)
			return rows;
		while (MYSQL_ROW row = mysql_fetch_row(result)) {
			if (!row[0] || !row[1] || !row[2])
				continue;
			auto lengths = mysql_fetch_lengths(result);
			rows.emplace_back(static_cast<uint32_t>(std::strtoul(row[0], nullptr, 10)), std::string(row[1], lengths[1]),
							  std::string(row[2], lengths[2]));
		}
		mysql_free_result(result);
		return rows;
	}
	bool claim(const std::string &key, const std::string &identity) {
		// Separate bounded connection: no unrelated transaction, native automatic retry or uncertain regrant.
		if (!ready)
			return false;
		if (!schemaValue("SELECT ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND "
						 "TABLE_NAME='zedas_house_loot'",
						 "InnoDB") ||
			!schemaValue("SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) FROM information_schema.STATISTICS "
						 "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='zedas_house_loot' AND INDEX_NAME='PRIMARY'",
						 "slot_key")) {
			ready = false;
			return false;
		}
		const std::string query =
			"INSERT INTO zedas_house_loot(slot_key,identity) VALUES('" + key + "','" + identity + "')";
		if (mysql_real_query(connection, query.data(), query.size()) == 0)
			return true;
		if (mysql_errno(connection) != 1062)
			ready = false; // Duplicate is retired; any other error disables issuance this session.
		return false;
	}
};
HouseLedger &fixtureLedger() {
	static HouseLedger ledger;
	return ledger;
}
bool activateFixtures(uint32_t houseId, const std::string &plan) { return fixtureLedger().activate(houseId, plan); }
} // namespace
bool EasyMode::claimHouseLoot(const std::string &key, const std::string &identity) {
	if (key.empty() || key.size() > 128 || identity.size() != 40)
		return false;
	for (char c : key)
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '/'))
			return false;
	for (char c : identity)
		if (!((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9')))
			return false;
	static HouseLedger ledger;
	return ledger.claim(key, identity);
}
void EasyMode::initializeHouses() {
	static HouseOverlay overlay;
	const bool master = g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED);
	const bool fixtures = master && g_config.getBoolean(ConfigManager::EASY_MODE_HOUSE_FURNISHING);
	const bool loot = master && g_config.getBoolean(ConfigManager::EASY_MODE_HOUSE_LOOT);
	if (!fixtures && !loot) {
		std::cout << "[Zedas houses] disabled; approved interiors: 0; fixtures loaded: 0; loot containers: 0; "
					 "rejected/mismatched: 0"
				  << std::endl;
		return;
	}
	std::ifstream map("data/world/" + g_config.getString(ConfigManager::MAP_NAME) + ".otbm", std::ios::binary);
	std::string raw((std::istreambuf_iterator<char>(map)), std::istreambuf_iterator<char>()), error;
	if (!map || !overlay.load("data/zedas/houses.xml", transformToSHA1(raw), error)) {
		std::cout << "[Zedas houses] disabled: " << error << "; approved interiors: 0; rejected/mismatched: 1"
				  << std::endl;
		return;
	}
	auto c = overlay.apply(g_game.map, fixtures, loot, claimHouseLoot);
	std::cout << "[Zedas houses] approved interiors: " << c.approved << "; fixtures loaded: " << c.fixtures
			  << "; loose loot: " << c.loot << "; loot containers: 0; rejected/mismatched: " << c.rejected << std::endl;
}

bool EasyMode::ownableHouseFixturesEnabled() {
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
		   g_config.getBoolean(ConfigManager::EASY_MODE_HOUSE_FURNISHING) &&
		   g_config.getBoolean(ConfigManager::EASY_MODE_OWNABLE_HOUSE_FURNISHING);
}
std::string EasyMode::houseFixtureSignature(const House &house) {
	std::vector<Position> tiles;
	for (auto tile : house.getTiles())
		tiles.push_back(tile->getPosition());
	std::sort(tiles.begin(), tiles.end(),
			  [](const Position &a, const Position &b) { return std::tie(a.z, a.y, a.x) < std::tie(b.z, b.y, b.x); });
	auto position = [](const Position &p) {
		return std::to_string(p.x) + "," + std::to_string(p.y) + "," + std::to_string(p.z) + "\n";
	};
	std::string canonical =
		std::to_string(house.getId()) + "\n" + house.getName() + "\n" + position(house.getEntryPosition());
	for (const auto &p : tiles)
		canonical += position(p);
	return transformToSHA1(canonical);
}
std::string EasyMode::HouseOverlay::snapshot(const HouseInterior &in) const {
	pugi::xml_document d;
	auto root = d.append_child("easyhouses");
	root.append_attribute("version") = 1;
	root.append_attribute("mapSha1") = identity.c_str();
	auto profile = root.append_child("profile");
	profile.append_attribute("id") = in.profile.c_str();
	for (const auto &point : in.points) {
		auto r = profiles.at(in.profile).at(point.role);
		auto n = profile.append_child("fixture");
		n.append_attribute("role") = point.role.c_str();
		n.append_attribute("item") = r.item;
		n.append_attribute("name") = r.name.c_str();
		n.append_attribute("count") = 1;
	}
	auto n = root.append_child("interior");
	n.append_attribute("id") = in.id.c_str();
	n.append_attribute("profile") = in.profile.c_str();
	n.append_attribute("classification") = "reviewed-unowned-house";
	n.append_attribute("review") = in.review.c_str();
	n.append_attribute("houseId") = in.houseId;
	n.append_attribute("houseSignature") = in.houseSignature.c_str();
	auto place = [](pugi::xml_node n, const Position &p, uint16_t ground) {
		n.append_attribute("x") = p.x;
		n.append_attribute("y") = p.y;
		n.append_attribute("z") = p.z;
		n.append_attribute("ground") = ground;
	};
	place(n, in.anchor, in.ground);
	for (const auto &point : in.points) {
		auto p = n.append_child("place");
		p.append_attribute("role") = point.role.c_str();
		place(p, point.pos, point.ground);
	}
	std::ostringstream out;
	d.save(out);
	return out.str();
}
EasyMode::HouseCounts EasyMode::HouseOverlay::restoreStored(Map &map, uint32_t expected) {
	HouseCounts counts;
	if (!ownableHouseFixturesEnabled())
		return counts;
	if (interiors.size() != 1 || interiors[0].houseId != expected) {
		++counts.rejected;
		return counts;
	}
	const auto &in = interiors[0];
	if (installedHouses.count({&map, expected}))
		return counts;
	House *house = map.houses.getHouse(expected);
	bool valid =
		house && houseFixtureSignature(*house) == in.houseSignature && emptyPoint(map, in.anchor, in.ground, expected);
	for (const auto &p : in.points)
		valid = valid && emptyPoint(map, p.pos, p.ground, expected) &&
				inert(profiles.at(in.profile).at(p.role).item, false);
	if (!valid) {
		++counts.rejected;
		return counts;
	}
	installedHouses.insert({&map, expected});
	++counts.approved;
	for (const auto &p : in.points) {
		auto r = profiles.at(in.profile).at(p.role);
		std::unique_ptr<Item> item(Item::CreateItem(r.item));
		if (g_game.internalAddItem(map.getTile(p.pos), item.get(), INDEX_WHEREEVER, FLAG_IGNOREAUTOSTACK) !=
			RETURNVALUE_NOERROR) {
			++counts.rejected;
			continue;
		}
		item.release();
		++counts.fixtures;
	}
	return counts;
}
EasyMode::HouseCounts EasyMode::restoreOwnableHouseFixtures(Map &map, const std::string &mapIdentity) {
	HouseCounts counts;
	if (!ownableHouseFixturesEnabled())
		return counts;
	for (const auto &row : fixtureLedger().plans()) {
		const uint32_t id = std::get<0>(row);
		const std::string &hash = std::get<1>(row);
		const std::string &xml = std::get<2>(row);
		HouseOverlay plan;
		std::string error;
		if (xml.size() > 65535 || transformToSHA1(xml) != hash || !plan.loadXml(xml, mapIdentity, error)) {
			++counts.rejected;
			continue;
		}
		auto result = plan.restoreStored(map, id);
		counts.approved += result.approved;
		counts.fixtures += result.fixtures;
		counts.rejected += result.rejected;
	}
	return counts;
}
void EasyMode::restoreOwnableHouseFixturesFromMap(Map &map, const std::string &filename) {
	if (!ownableHouseFixturesEnabled())
		return;
	std::ifstream input(filename, std::ios::binary);
	if (!input) {
		std::cout << "[Zedas ownable houses] map unavailable; no restoration" << std::endl;
		return;
	}
	std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	auto c = restoreOwnableHouseFixtures(map, transformToSHA1(raw));
	std::cout << "[Zedas ownable houses] restored houses: " << c.approved << "; fixtures: " << c.fixtures
			  << "; rejected: " << c.rejected << std::endl;
}
