#include "otpch.h"
#include "broker.h"
#include "brokerstore.h"
#include "easymode.h"
#include "easyspill.h"
#include "configmanager.h"
#include "container.h"
#include "game.h"
#include "housetile.h"
#include "movement.h"
#include "npc.h"
#include "player.h"
#include "tools.h"
#include <fstream>
#include <regex>

extern Game g_game;
extern ConfigManager g_config;
extern MoveEvents *g_moveEvents;
namespace Broker
{
namespace
{
std::map<uint16_t, uint64_t> catalog;
std::unique_ptr<Store> database;
struct Context {
	uint32_t session = 0, actor = 0;
	bool hired = false;
	int64_t nextThink = 0, targetSince = 0, lastPickup = 0;
	Item *target = nullptr;
	std::unique_ptr<Container> bag{new Container(2854, INVENTORY_LIMIT)};
	std::map<std::string, Item *> carried;
	void clearTarget()
	{
		if (target)
			target->decrementReferenceCounter();
		target = nullptr;
		targetSince = 0;
	}
	~Context()
	{
		clearTarget();
	}
};
struct Pending {
	uint32_t owner;
	Item *item;
	Tile *tile;
	uint16_t id;
	uint16_t count;
	uint64_t unitPrice;
	std::string key;
};
std::map<uint32_t, std::unique_ptr<Context>> contexts;
std::map<Item *, Pending> uncertain;
int64_t nextReconnect = 0;
void message(Player &owner, const std::string &text)
{
	owner.sendTextMessage(MESSAGE_STATUS_SMALL, text);
}
unsigned leash()
{
	return g_config.getNumber(ConfigManager::EASY_MODE_BROKER_LEASH_TILES);
}
bool near(const Position &a, const Position &b, int distance)
{
	return a.z == b.z && Position::getDistanceX(a, b) <= distance && Position::getDistanceY(a, b) <= distance;
}
Npc *actor(Context &context)
{
	return g_game.getNpcByID(context.actor);
}
void removeActor(Context &context)
{
	context.clearTarget();
	if (Npc *npc = actor(context))
		g_game.removeCreature(npc);
	context.actor = 0;
}
bool safeWalk(const Position &pos, bool entering = true)
{
	Tile *tile = g_game.map.getTile(pos);
	if (!tile || !tile->getGround() || dynamic_cast<HouseTile *>(tile) ||
	    (entering && tile->getThingCount() >= EasyMode::CLIENT_TILE_THING_LIMIT) ||
	    tile->hasFlag(TILESTATE_TELEPORT | TILESTATE_MAGICFIELD | TILESTATE_MAILBOX | TILESTATE_BED | TILESTATE_DEPOT |
	                  TILESTATE_BLOCKSOLID | TILESTATE_BLOCKPATH) ||
	    !g_moveEvents || g_moveEvents->hasPositionEvent(pos))
		return false;
	for (size_t i = tile->getFirstIndex(); i < tile->getLastIndex(); ++i) {
		Thing *thing = tile->getThing(i);
		Item *item = thing ? thing->getItem() : nullptr;
		if (item &&
		    (item->getActionId() || (item->getMovementId() || item->isChestQuest()) ||
		     g_moveEvents->getEvent(item, MOVE_EVENT_STEP_IN) || g_moveEvents->getEvent(item, MOVE_EVENT_STEP_OUT)))
			return false;
	}
	return true;
}
void spawn(Player &owner, Context &context)
{
	if (actor(context))
		return;
	for (int dx = -1; dx <= 1; ++dx)
		for (int dy = -1; dy <= 1; ++dy) {
			if (!dx && !dy)
				continue;
			Position pos(owner.getPosition().x + dx, owner.getPosition().y + dy, owner.getPosition().z);
			Tile *tile = g_game.map.getTile(pos);
			if (!safeWalk(pos) || (tile->getCreatures() && !tile->getCreatures()->empty()))
				continue;
			std::unique_ptr<Npc> npc(Npc::createBroker(owner.getGUID()));
			if (g_game.placeCreature(npc.get(), pos)) {
				context.actor = npc.release()->getID();
				return;
			}
		}
}
void hydrate(uint32_t owner, Context &context)
{
	auto rows = database->inventory(owner);
	std::set<std::string> unsold;
	for (const auto &row : rows) {
		const auto &key = row.at("claim_key");
		unsold.insert(key);
		const auto carried = context.carried.find(key);
		if (carried != context.carried.end()) {
			Item &item = *carried->second;
			if (item.getParent() != context.bag.get() || item.getID() != number(row, "item") ||
			    item.getItemCount() != number(row, "count") || !price(item) || price(item) != number(row, "price"))
				throw std::runtime_error("live broker escrow changed; inventory retained");
			continue;
		}
		for (const auto &pending : uncertain)
			if (pending.second.key == key)
				throw std::runtime_error("broker claim awaiting reconciliation");
		const uint64_t id = number(row, "item"), count = number(row, "count");
		if (id > 65535 || !count || count > 100 || context.bag->size() >= INVENTORY_LIMIT)
			throw std::runtime_error("invalid broker recovery image");
		std::unique_ptr<Item> item(Item::CreateItem(id, count));
		if (!item || item->getItemCount() != count || !price(*item) || price(*item) != number(row, "price"))
			throw std::runtime_error("broker recovery catalog mismatch; inventory retained");
		Item *raw = item.release();
		context.bag->addThing(raw);
		context.carried[key] = raw;
	}
	// A known committed sale is the only path consuming private live instances.
	for (auto it = context.carried.begin(); it != context.carried.end();) {
		if (!unsold.count(it->first)) {
			Item *item = it->second;
			context.bag->removeThing(item, item->getItemCount());
			item->setParent(nullptr);
			item->decrementReferenceCounter();
			it = context.carried.erase(it);
		} else
			++it;
	}
}
Context &getContext(Player &owner)
{
	auto &context = contexts[owner.getGUID()];
	if (!context)
		context.reset(new Context);
	if (context->session && context->session != owner.getID())
		removeActor(*context);
	context->session = owner.getID();
	return *context;
}
void complete(Pending &pending)
{
	auto &context = contexts[pending.owner];
	if (!context)
		context.reset(new Context);
	if (pending.item->getID() != pending.id || pending.item->getItemCount() != pending.count ||
	    pending.item->getParent() != pending.tile || !EasyMode::takeBrokerItem(*pending.item, *context->bag))
		throw std::runtime_error("durable broker claim quarantined; ground transfer unavailable");
	context->carried[pending.key] = pending.item;
}
void reconcileClaims()
{
	for (auto it = uncertain.begin(); it != uncertain.end();) {
		Pending &pending = it->second;
		if (database->prepared(pending.key, pending.owner)) {
			database->prepare(pending.key, pending.owner, pending.id, pending.count, pending.unitPrice);
			complete(pending);
		}
		pending.item->decrementReferenceCounter();
		it = uncertain.erase(it);
	}
}
bool reconnect()
{
	if (!database)
		return false;
	if (OTSYS_TIME() < nextReconnect)
		return false;
	try {
		database->connect();
		reconcileClaims();
		nextReconnect = 0;
		return true;
	} catch (const std::exception &e) {
		nextReconnect = OTSYS_TIME() + 5000;
		std::cout << "[Broker paused] " << e.what() << std::endl;
		return false;
	}
}
void claim(Player &owner, Context &context, Item &item, int64_t now)
{
	if (context.bag->size() >= INVENTORY_LIMIT || !price(item) ||
	    !EasyMode::isBrokerCandidate(item, now,
	                                 g_config.getNumber(ConfigManager::EASY_MODE_BROKER_GRACE_SECONDS) * 1000LL))
		return;
	Npc *npc = actor(context);
	if (!npc || !near(npc->getPosition(), item.getPosition(), 1) ||
	    !near(owner.getPosition(), item.getPosition(), leash()) ||
	    !g_game.isSightClear(npc->getPosition(), item.getPosition(), true))
		return;
	Pending pending{owner.getGUID(),     &item,       item.getTile(), item.getID(),
	                item.getItemCount(), price(item), database->key()};
	item.incrementReferenceCounter();
	uncertain.emplace(&item, pending);
	// From this point, a possibly committed durable image must not coexist with
	// a movable ground original. Never reserve during the approach/grace phase.
	database->prepare(pending.key, pending.owner, pending.id, pending.count, pending.unitPrice);
	complete(uncertain.at(&item));
	uncertain.erase(&item);
	item.decrementReferenceCounter();
	context.lastPickup = now;
	context.clearTarget();
}
bool walk(Npc &npc, const Position &goal)
{
	std::forward_list<Direction> path;
	if (!npc.getPathTo(goal, path, 1, 1, true, true, leash() * 2))
		return false;
	Position cursor = npc.getPosition();
	for (Direction dir : path) {
		cursor = getNextPosition(dir, cursor);
		if (!canStep(npc, cursor))
			return false;
	}
	npc.startAutoWalk(path);
	return true;
}
} // namespace
bool enabled()
{
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
	       g_config.getBoolean(ConfigManager::EASY_MODE_LOOT_BROKER);
}
bool locked(const Item *item)
{
	return item && uncertain.count(const_cast<Item *>(item));
}
bool loadCatalog(const std::string &path, std::string &error)
{
	catalog.clear();
	try {
		pugi::xml_document document;
		if (!document.load_file(path.c_str()))
			throw std::runtime_error("catalog XML unavailable");
		const auto root = document.document_element();
		const std::set<std::string> rootAttributes{"buyer", "town", "station", "source", "sha1", "access"};
		const std::set<std::string> itemAttributes{"id", "subtype", "name", "price", "rule"};
		for (const auto attribute : root.attributes())
			if (!rootAttributes.count(attribute.name()))
				throw std::runtime_error("unknown catalog buyer attribute");
		const std::string digest = "3db8996153f564baf26df2c8a70250873a53dd7d";
		if (std::string(root.name()) != "broker" ||
		    std::string(root.attribute("source").value()) != "data/npc/sam.npc" ||
		    std::string(root.attribute("sha1").value()) != digest ||
		    std::string(root.attribute("buyer").value()) != "Sam" ||
		    std::string(root.attribute("town").value()) != "Thais" ||
		    std::string(root.attribute("station").value()) != "32360;32199;7" ||
		    std::string(root.attribute("access").value()) != "unconditional-topic-2")
			throw std::runtime_error("unreviewed catalog buyer/access");
		std::ifstream file("data/npc/sam.npc", std::ios::binary);
		const std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		std::string source;
		for (char c : raw)
			if (c != '\r')
				source += c;
		if (transformToSHA1(source) != digest)
			throw std::runtime_error("NPC purchase source changed; review required");
		const std::regex rule("^\\\"sell\\\".*-> Type=([0-9]+), Amount=1, Price=([0-9]+),.*Topic=2$");
		std::map<uint16_t, uint64_t> checked;
		for (const auto node : root.children()) {
			for (const auto attribute : node.attributes())
				if (!itemAttributes.count(attribute.name()))
					throw std::runtime_error("unknown catalog item attribute");
			Row fields{{"id", node.attribute("id").value()},
			           {"price", node.attribute("price").value()},
			           {"subtype", node.attribute("subtype").value()}};
			const uint64_t id = number(fields, "id"), value = number(fields, "price");
			const std::string line = node.attribute("rule").value();
			std::smatch match;
			if (std::string(node.name()) != "item" || !id || id >= Item::items.size() || id > 65535 || !value ||
			    value > MONEY_LIMIT || number(fields, "subtype") ||
			    Item::items[id].name != node.attribute("name").value() || !std::regex_match(line, match, rule) ||
			    std::stoul(match[1]) != id || std::stoull(match[2]) != value ||
			    source.find("\n" + line + "\n") == std::string::npos || !checked.emplace(id, value).second)
				throw std::runtime_error("invalid/duplicate/unreviewed catalog rule");
			std::unique_ptr<Item> probe(Item::CreateItem(id, 1));
			if (!probe || !EasyMode::isSpillItem(*probe) || id == 3492 || probe->getWorth())
				throw std::runtime_error("unsafe sale identity");
		}
		if (checked.empty())
			throw std::runtime_error("empty sale catalog");
		catalog.swap(checked);
		return true;
	} catch (const std::exception &e) {
		error = e.what();
		return false;
	}
}
uint64_t price(const Item &item)
{
	auto entry = catalog.find(item.getID());
	return entry != catalog.end() && item.getID() != 3492 && !item.getWorth() &&
	               (item.isStackable() || item.getItemCount() == 1) && EasyMode::lootClass(item) == EasyMode::LootClass::BrokerFodder
	           ? entry->second
	           : 0;
}
void initialize()
{
	initialize(std::unique_ptr<Store>());
}
void initialize(std::unique_ptr<Store> connection)
{
	if (!enabled())
		return;
	if (g_config.getBoolean(ConfigManager::ALLOW_CLONES))
		throw std::runtime_error("broker requires unique online player GUIDs (allowClones=false)");
	if (!g_config.getBoolean(ConfigManager::EASY_MODE_LOOT_SPILL))
		throw std::runtime_error("broker requires visible Easy Mode loot spill");
	std::string error;
	if (!loadCatalog(g_config.getString(ConfigManager::EASY_MODE_BROKER_CATALOG_FILE), error))
		throw std::runtime_error("broker catalog: " + error);
	Connection settings;
	settings.host = g_config.getString(ConfigManager::MYSQL_HOST);
	settings.user = g_config.getString(ConfigManager::MYSQL_USER);
	settings.password = g_config.getString(ConfigManager::MYSQL_PASS);
	settings.database = g_config.getString(ConfigManager::MYSQL_DB);
	settings.socket = g_config.getString(ConfigManager::MYSQL_SOCK);
	settings.port = g_config.getNumber(ConfigManager::SQL_PORT);
	database = connection ? std::move(connection) : std::unique_ptr<Store>(new Store(settings));
	database->connect();
}
void stop(Player &owner)
{
	if (!enabled())
		return;
	auto found = contexts.find(owner.getGUID());
	if (found == contexts.end() || found->second->session != owner.getID())
		return;
	removeActor(*found->second);
	found->second->hired = false;
	found->second->session = 0;
}
bool canStep(const Npc &npc, const Position &destination)
{
	auto found = contexts.find(npc.getBrokerOwner());
	if (found == contexts.end())
		return false;
	Player *owner = g_game.getPlayerByID(found->second->session);
	return owner && !owner->isRemoved() && owner->getHealth() > 0 && safeWalk(destination) &&
	       near(owner->getPosition(), destination, leash()) &&
	       (!npc.getParent() ||
	        (near(owner->getPosition(), npc.getPosition(), leash()) && safeWalk(npc.getPosition(), false)));
}
void tick(Player &owner, int64_t now)
{
	if (!enabled() || !database)
		return;
	auto found = contexts.find(owner.getGUID());
	if (found == contexts.end() || found->second->session != owner.getID())
		return;
	Context &context = *found->second;
	if (owner.isRemoved() || owner.getHealth() <= 0) {
		stop(owner);
		return;
	}
	if (!context.hired)
		return;
	if (!now)
		now = OTSYS_TIME();
	if (now < context.nextThink)
		return;
	context.nextThink = now + 1000;
	if (!reconnect()) {
		removeActor(context);
		return;
	}
	try {
		hydrate(owner.getGUID(), context); // Validate recovered images before any sale.
		database->settle(owner.getGUID(), time(nullptr),
		                 g_config.getNumber(ConfigManager::EASY_MODE_BROKER_COMMISSION_BPS));
		database->returned(owner.getGUID(), time(nullptr));
		hydrate(owner.getGUID(), context);
		const auto state = database->status(owner.getGUID());
		if (number(state, "state")) {
			removeActor(context);
			return;
		}
		Npc *npc = actor(context);
		if (npc && !near(owner.getPosition(), npc->getPosition(), leash())) {
			removeActor(context);
			context.hired = false;
			message(owner, "Broker collection paused after travel. Use !broker hire to resume; escrow is safe.");
			return;
		}
		spawn(owner, context);
		npc = actor(context);
		if (!npc)
			return;
		const int32_t desiredSpeed = std::max(82, static_cast<const Creature &>(owner).getStepSpeed() + 20);
		if (npc->getSpeed() != desiredSpeed)
			g_game.changeSpeed(npc, (desiredSpeed - npc->getSpeed()) / 2);
		// Bounded batches depart automatically after 15 seconds without a pickup.
		if (!context.carried.empty() && (context.bag->size() >= INVENTORY_LIMIT || now - context.lastPickup >= 15000)) {
			database->depart(owner.getGUID(), database->key(), time(nullptr));
			message(owner, "Your broker is travelling to Sam in Thais (60 seconds each way).");
			removeActor(context);
			return;
		}
		const int64_t grace = g_config.getNumber(ConfigManager::EASY_MODE_BROKER_GRACE_SECONDS) * 1000LL;
		if (context.target &&
		    (!EasyMode::isBrokerCandidate(*context.target, now, grace) || !price(*context.target) ||
		     !near(owner.getPosition(), context.target->getPosition(), leash()) || now - context.targetSince >= 5000))
			context.clearTarget();
		if (!context.target)
			for (Item *item : EasyMode::brokerCandidates(now, grace)) {
				if (price(*item) && near(owner.getPosition(), item->getPosition(), leash()) &&
				    npc->canSee(item->getPosition()) &&
				    g_game.isSightClear(npc->getPosition(), item->getPosition(), true)) {
					context.target = item;
					item->incrementReferenceCounter();
					context.targetSince = now;
					if (near(npc->getPosition(), item->getPosition(), 1) || walk(*npc, item->getPosition()))
						break;
					context.clearTarget();
				}
			}
		if (context.target) {
			if (near(npc->getPosition(), context.target->getPosition(), 1))
				claim(owner, context, *context.target, now);
			else
				walk(*npc, context.target->getPosition());
		} else if (!near(npc->getPosition(), owner.getPosition(), 1))
			walk(*npc, owner.getPosition());
	} catch (const std::exception &e) {
		removeActor(context);
		message(owner, "Broker paused for database/inventory recovery. Your ledger and escrow are retained.");
		std::cout << "[Broker paused] " << e.what() << std::endl;
	}
}
void command(Player &owner, const std::string &text)
{
	if (!enabled() || !database) {
		message(owner, "Easy Mode broker is disabled.");
		return;
	}
	if (owner.isRemoved() || owner.getHealth() <= 0 || !owner.getGUID())
		return;
	if (!reconnect()) {
		message(owner, "Broker database unavailable; no collection or payout performed.");
		return;
	}
	try {
		database->account(owner.getGUID());
		Context &context = getContext(owner);
		hydrate(owner.getGUID(), context);
		if (text == "hire") {
			context.hired = true;
			context.nextThink = 0;
			context.lastPickup = OTSYS_TIME();
			tick(owner);
			message(owner, "Broker hired. Visible leftovers have a player-first grace period.");
		} else if (text == "dismiss") {
			context.hired = false;
			removeActor(context);
			message(owner, "Broker dismissed. Inventory and balances remain saved.");
		} else if (text == "sell") {
			database->depart(owner.getGUID(), database->key(), time(nullptr));
			removeActor(context);
			message(owner, "Any carried loot is on its way to Sam in Thais.");
		} else if (text == "cashout" || text == "cash out") {
			const Bank bank = database->cashout(owner.getGUID(), database->key(), owner.getBankBalance(),
			                                    owner.getBrokerBankWatermark());
			owner.setBankBalance(bank.balance);
			owner.setBrokerBankWatermark(bank.watermark);
			message(owner,
			        "Broker pending proceeds deposited to your bank. Bank: " + std::to_string(bank.balance) + " gp.");
		} else if (text.empty() || text == "status" || text == "balance") {
			const auto state = database->status(owner.getGUID());
			message(owner, "Broker: " + std::string(context.hired ? "hired" : "dismissed") + ", escrow " +
			                   std::to_string(context.carried.size()) + "/20, pending " + state.at("pending") +
			                   " gp, retained economy funds " + state.at("funds") + " gp, travel state " +
			                   state.at("state") + ".");
		} else
			message(owner, "Use !broker hire, dismiss, sell, balance, or cashout. Travel is 60 seconds each way.");
	} catch (const std::exception &e) {
		message(owner, "Broker operation paused; inventory and ledger retained.");
		std::cout << "[Broker] " << e.what() << std::endl;
	}
}
} // namespace Broker
