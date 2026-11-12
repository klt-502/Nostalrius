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
#include <set>

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
	bool returnSpeechPending = false;
	int64_t nextPickupSpeech = 0;
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
// Use classic monster-noise speech: no NPC SAY/channel/status-text substitute.
void noise(Context &context, const std::string &text, int64_t now, bool pickup)
{
	Npc *npc = actor(context);
	if (!npc || (pickup && now < context.nextPickupSpeech))
		return;
	g_game.internalCreatureSay(npc, TALKTYPE_MONSTER_SAY, text, false);
	if (pickup)
		context.nextPickupSpeech = now + 3000;
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
	    !EasyMode::isBrokerCandidate(item, now, FODDER_GRACE_MS))
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
	noise(context, "Got " + item.getNameDescription() + ".", now, true);
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
namespace
{
std::string readNormalized(const std::string &path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
		throw std::runtime_error("reviewed broker source unavailable");
	const std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	std::string result;
	for (size_t i = 0; i < raw.size(); ++i)
		if (raw[i] != '\r' || i + 1 == raw.size() || raw[i + 1] != '\n')
			result += raw[i];
	return result;
}
bool sameAttributes(const pugi::xml_node &node, const pugi::xml_node &approved)
{
	size_t count = 0, other = 0;
	for (const auto attribute : node.attributes()) {
		++count;
		if (!approved.attribute(attribute.name()) ||
		    std::string(approved.attribute(attribute.name()).value()) != attribute.value())
			return false;
	}
	for (const auto attribute : approved.attributes()) {
		++other;
		if (!node.attribute(attribute.name()) ||
		    std::string(node.attribute(attribute.name()).value()) != attribute.value())
			return false;
	}
	return count == other;
}
} // namespace
bool loadCatalog(const std::string &path, std::string &error)
{
	catalog.clear();
	try {
		// One reviewed data authority, pinned after the source/access/tier audit.
		// A configured subset may remove entries; it cannot invent buyers/prices.
		static const char *REVIEWED_CATALOG_SHA1 = "a5e8020c3a2484f48f50a25e0170647e00d1d5e6";
		const std::string canonical = readNormalized("data/zedas/broker.xml");
		if (transformToSHA1(canonical) != REVIEWED_CATALOG_SHA1)
			throw std::runtime_error("canonical broker review changed; re-audit required");
		pugi::xml_document reviewed, configured;
		if (!reviewed.load_string(canonical.c_str()) || !configured.load_file(path.c_str()))
			throw std::runtime_error("broker XML unavailable");
		const auto root = configured.document_element(), reference = reviewed.document_element();
		if (std::string(root.name()) != "broker" || !sameAttributes(root, reference))
			throw std::runtime_error("invalid broker catalog version");
		std::map<uint16_t, uint64_t> checked;
		std::set<std::string> buyers;
		for (const auto buyer : root.children()) {
			const std::string id = buyer.attribute("id").value();
			const auto approvedBuyer = reference.find_child_by_attribute("buyer", "id", id.c_str());
			if (std::string(buyer.name()) != "buyer" || !approvedBuyer || !buyers.insert(id).second ||
			    !sameAttributes(buyer, approvedBuyer))
				throw std::runtime_error("unreviewed/duplicate broker buyer or access");
			std::istringstream dependencies(buyer.attribute("dependencies").value());
			std::string dependency;
			while (std::getline(dependencies, dependency, ';')) {
				const auto separator = dependency.find('|');
				if (separator == std::string::npos ||
				    transformToSHA1(readNormalized(dependency.substr(0, separator))) !=
				        dependency.substr(separator + 1))
					throw std::runtime_error("included NPC source changed; review required");
			}
			const auto source = readNormalized(buyer.attribute("source").value());
			const std::string confirmation = buyer.attribute("confirmation").value();
			if (transformToSHA1(source) != buyer.attribute("sha1").value() ||
			    source.find("\n" + confirmation + "\n") == std::string::npos ||
			    confirmation.find("Count(Type)>=Amount") == std::string::npos ||
			    confirmation.find("Delete(Type)") == std::string::npos ||
			    confirmation.find("CreateMoney") == std::string::npos)
				throw std::runtime_error("NPC purchase source/confirmation changed; review required");
			for (const auto node : buyer.children()) {
				Row fields{{"id", node.attribute("id").value()},
				           {"price", node.attribute("price").value()},
				           {"subtype", node.attribute("subtype").value()}};
				const auto itemId = number(fields, "id"), value = number(fields, "price");
				const auto approvedItem =
				    approvedBuyer.find_child_by_attribute("item", "id", node.attribute("id").value());
				const std::string rule = node.attribute("rule").value();
				if (std::string(node.name()) != "item" || !approvedItem || !sameAttributes(node, approvedItem) ||
				    !itemId || itemId > 65535 || itemId >= Item::items.size() || !value || value > MONEY_LIMIT ||
				    number(fields, "subtype") || Item::items[itemId].name != node.attribute("name").value() ||
				    source.find("\n" + rule + "\n") == std::string::npos || !checked.emplace(itemId, value).second)
					throw std::runtime_error("unreviewed/duplicate broker item or price");
				std::unique_ptr<Item> probe(Item::CreateItem(itemId, 1));
				if (!probe || EasyMode::lootClass(*probe) != EasyMode::LootClass::BrokerFodder)
					throw std::runtime_error("broker class/state is not safe fodder");
			}
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
	               (item.isStackable() || item.getItemCount() == 1) &&
	               EasyMode::lootClass(item) == EasyMode::LootClass::BrokerFodder
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
		if (number(database->status(owner.getGUID()), "state") == 2)
			context.returnSpeechPending = true;
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
		if (context.returnSpeechPending) {
			noise(context, "Back from town.", now, false);
			context.returnSpeechPending = false;
		}
		const int32_t desiredSpeed = std::max(82, static_cast<const Creature &>(owner).getStepSpeed() + 20);
		if (npc->getSpeed() != desiredSpeed)
			g_game.changeSpeed(npc, (desiredSpeed - npc->getSpeed()) / 2);
		// Bounded batches depart automatically after 15 seconds without a pickup.
		if (!context.carried.empty() && (context.bag->size() >= INVENTORY_LIMIT || now - context.lastPickup >= 15000)) {
			database->depart(owner.getGUID(), database->key(), time(nullptr));
			noise(context, "Off to town!", now, false);
			removeActor(context);
			return;
		}
		const int64_t grace = FODDER_GRACE_MS;
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
			message(owner, "Broker hired. Approved routine leftovers can be collected immediately.");
		} else if (text == "dismiss") {
			context.hired = false;
			removeActor(context);
			message(owner, "Broker dismissed. Inventory and balances remain saved.");
		} else if (text == "sell") {
			if (context.carried.empty()) {
				message(owner, "No carried loot to sell.");
				return;
			}
			database->depart(owner.getGUID(), database->key(), time(nullptr));
			noise(context, "Off to town!", OTSYS_TIME(), false);
			removeActor(context);
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
